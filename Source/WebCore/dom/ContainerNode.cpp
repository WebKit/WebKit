/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ContainerNode.h"

#include "AXObjectCache.h"
#include "AllDescendantsCollection.h"
#include "ChildListMutationScope.h"
#include "ClassCollection.h"
#include "CommonVM.h"
#include "ContainerNodeAlgorithms.h"
#include "Editor.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSlotElement.h"
#include "HTMLTableRowsCollection.h"
#include "InlineTextBox.h"
#include "InspectorInstrumentation.h"
#include "JSNode.h"
#include "LabelsNodeList.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NodeRareData.h"
#include "NodeRenderStyle.h"
#include "RadioNodeList.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderWidget.h"
#include "RootInlineBox.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#include "ScriptDisallowedScope.h"
#include "SelectorQuery.h"
#include "SlotAssignment.h"
#include "TemplateContentDocumentFragment.h"
#include <algorithm>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Variant.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ContainerNode);

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Ref<Node>&);

ChildNodesLazySnapshot* ChildNodesLazySnapshot::latestSnapshot;

unsigned ScriptDisallowedScope::s_count = 0;
#if !ASSERT_DISABLED
ScriptDisallowedScope::EventAllowedScope* ScriptDisallowedScope::EventAllowedScope::s_currentScope = nullptr;
#endif

ALWAYS_INLINE NodeVector ContainerNode::removeAllChildrenWithScriptAssertion(ChildChangeSource source, DeferChildrenChanged deferChildrenChanged)
{
    auto children = collectChildNodes(*this);

    if (source == ContainerNode::ChildChangeSource::API) {
        ChildListMutationScope mutation(*this);
        for (auto& child : children) {
            mutation.willRemoveChild(child.get());
            child->notifyMutationObserversNodeWillDetach();
            dispatchChildRemovalEvents(child);
        }
    } else {
        ASSERT(source == ContainerNode::ChildChangeSource::Parser);
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (UNLIKELY(document().hasMutationObserversOfType(MutationObserver::ChildList))) {
            ChildListMutationScope mutation(*this);
            for (auto& child : children)
                mutation.willRemoveChild(child.get());
        }
    }

    disconnectSubframesIfNeeded(*this, DescendantsOnly);

    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (UNLIKELY(isShadowRoot() || isInShadowTree()))
        containingShadowRoot()->willRemoveAllChildren(*this);

    document().nodeChildrenWillBeRemoved(*this);

    while (RefPtr<Node> child = m_firstChild) {
        removeBetween(nullptr, child->nextSibling(), *child);
        notifyChildNodeRemoved(*this, *child);
    }

    if (deferChildrenChanged == DeferChildrenChanged::No)
        childrenChanged(ContainerNode::ChildChange { ContainerNode::AllChildrenRemoved, nullptr, nullptr, source });

    return children;
}

ALWAYS_INLINE bool ContainerNode::removeNodeWithScriptAssertion(Node& childToRemove, ChildChangeSource source)
{
    Ref<Node> protectedChildToRemove(childToRemove);
    ASSERT_WITH_SECURITY_IMPLICATION(childToRemove.parentNode() == this);
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        ChildListMutationScope(*this).willRemoveChild(childToRemove);
    }

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(childToRemove));
    if (source == ContainerNode::ChildChangeSource::API) {
        childToRemove.notifyMutationObserversNodeWillDetach();
        dispatchChildRemovalEvents(protectedChildToRemove);
        if (childToRemove.parentNode() != this)
            return false;
    }

    if (source == ContainerNode::ChildChangeSource::Parser) {
        // FIXME: Merge these two code paths. It's a bug in the parser not to update connectedSubframeCount in time.
        disconnectSubframesIfNeeded(*this, DescendantsOnly);
    } else {
        if (is<ContainerNode>(childToRemove))
            disconnectSubframesIfNeeded(downcast<ContainerNode>(childToRemove), RootAndDescendants);
    }

    if (childToRemove.parentNode() != this)
        return false;

    ChildChange change;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        if (UNLIKELY(isShadowRoot() || isInShadowTree()))
            containingShadowRoot()->resolveSlotsBeforeNodeInsertionOrRemoval();

        document().nodeWillBeRemoved(childToRemove);

        ASSERT_WITH_SECURITY_IMPLICATION(childToRemove.parentNode() == this);
        ASSERT(!childToRemove.isDocumentFragment());

        RefPtr<Node> previousSibling = childToRemove.previousSibling();
        RefPtr<Node> nextSibling = childToRemove.nextSibling();
        removeBetween(previousSibling.get(), nextSibling.get(), childToRemove);
        notifyChildNodeRemoved(*this, childToRemove);

        change.type = is<Element>(childToRemove) ? ElementRemoved : (is<Text>(childToRemove) ? TextRemoved : NonContentsChildRemoved);
        change.previousSiblingElement = (!previousSibling || is<Element>(*previousSibling)) ? downcast<Element>(previousSibling.get()) : ElementTraversal::previousSibling(*previousSibling);
        change.nextSiblingElement = (!nextSibling || is<Element>(*nextSibling)) ? downcast<Element>(nextSibling.get()) : ElementTraversal::nextSibling(*nextSibling);
        change.source = source;
    }

    // FIXME: Move childrenChanged into ScriptDisallowedScope block.
    childrenChanged(change);

    return true;
}

enum class ReplacedAllChildren { No, Yes };

template<typename DOMInsertionWork>
static ALWAYS_INLINE void executeNodeInsertionWithScriptAssertion(ContainerNode& containerNode, Node& child,
    ContainerNode::ChildChangeSource source, ReplacedAllChildren replacedAllChildren, DOMInsertionWork doNodeInsertion)
{
    NodeVector postInsertionNotificationTargets;
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        if (UNLIKELY(containerNode.isShadowRoot() || containerNode.isInShadowTree()))
            containerNode.containingShadowRoot()->resolveSlotsBeforeNodeInsertionOrRemoval();

        doNodeInsertion();
        ChildListMutationScope(containerNode).childAdded(child);
        postInsertionNotificationTargets = notifyChildNodeInserted(containerNode, child);
    }

    // FIXME: Move childrenChanged into ScriptDisallowedScope block.
    if (replacedAllChildren == ReplacedAllChildren::Yes)
        containerNode.childrenChanged(ContainerNode::ChildChange { ContainerNode::AllChildrenReplaced, nullptr, nullptr, source });
    else {
        containerNode.childrenChanged(ContainerNode::ChildChange {
            child.isElementNode() ? ContainerNode::ElementInserted : (child.isTextNode() ? ContainerNode::TextInserted : ContainerNode::NonContentsChildInserted),
            ElementTraversal::previousSibling(child),
            ElementTraversal::nextSibling(child),
            source
        });
    }

    ASSERT(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(child));
    for (auto& target : postInsertionNotificationTargets)
        target->didFinishInsertingNode();

    if (source == ContainerNode::ChildChangeSource::API)
        dispatchChildInsertionEvents(child);
}

static ExceptionOr<void> collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes)
{
    if (!is<DocumentFragment>(node)) {
        nodes.append(node);
        auto* oldParent = node.parentNode();
        if (!oldParent)
            return { };
        return oldParent->removeChild(node);
    }

    nodes = collectChildNodes(node);
    downcast<DocumentFragment>(node).removeChildren();
    return { };
}

// FIXME: This function must get a new name.
// It removes all children, not just a category called "detached children".
// So this name is terribly confusing.
void ContainerNode::removeDetachedChildren()
{
    if (connectedSubframeCount()) {
        for (Node* child = firstChild(); child; child = child->nextSibling())
            child->updateAncestorConnectedSubframeCountForRemoval();
    }
    // FIXME: We should be able to ASSERT(!attached()) here: https://bugs.webkit.org/show_bug.cgi?id=107801
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    removeDetachedChildrenInContainer(*this);
}

static inline void destroyRenderTreeIfNeeded(Node& child)
{
    bool isElement = is<Element>(child);
    auto hasDisplayContents = isElement && downcast<Element>(child).hasDisplayContents();
    if (!child.renderer() && !hasDisplayContents)
        return;
    if (isElement)
        RenderTreeUpdater::tearDownRenderers(downcast<Element>(child));
    else if (is<Text>(child))
        RenderTreeUpdater::tearDownRenderer(downcast<Text>(child));
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    ASSERT(oldParent);

    auto children = oldParent->removeAllChildrenWithScriptAssertion(ChildChangeSource::Parser);

    // FIXME: assert that we don't dispatch events here since this container node is still disconnected.
    for (auto& child : children) {
        RELEASE_ASSERT(!child->parentNode() && &child->treeScope() == &treeScope());
        ASSERT(!ensurePreInsertionValidity(child, nullptr).hasException());
        child->setTreeScopeRecursively(treeScope());
        parserAppendChild(child);
    }
}

ContainerNode::~ContainerNode()
{
    if (!isDocumentNode())
        willBeDeletedFrom(document());
    removeDetachedChildren();
}

static inline bool isChildTypeAllowed(ContainerNode& newParent, Node& child)
{
    if (!child.isDocumentFragment())
        return newParent.childTypeAllowed(child.nodeType());

    for (Node* node = child.firstChild(); node; node = node->nextSibling()) {
        if (!newParent.childTypeAllowed(node->nodeType()))
            return false;
    }
    return true;
}

static inline bool isInTemplateContent(const Node* node)
{
    Document& document = node->document();
    return &document == document.templateDocument();
}

static inline bool containsConsideringHostElements(const Node& newChild, const Node& newParent)
{
    return (newParent.isInShadowTree() || isInTemplateContent(&newParent))
        ? newChild.containsIncludingHostElements(&newParent)
        : newChild.contains(&newParent);
}

static inline ExceptionOr<void> checkAcceptChild(ContainerNode& newParent, Node& newChild, const Node* refChild, Document::AcceptChildOperation operation)
{
    // Use common case fast path if possible.
    if ((newChild.isElementNode() || newChild.isTextNode()) && newParent.isElementNode()) {
        ASSERT(!newParent.isDocumentTypeNode());
        ASSERT(isChildTypeAllowed(newParent, newChild));
        if (containsConsideringHostElements(newChild, newParent))
            return Exception { HierarchyRequestError };
        if (operation == Document::AcceptChildOperation::InsertOrAdd && refChild && refChild->parentNode() != &newParent)
            return Exception { NotFoundError };
        return { };
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild.isPseudoElement());
    if (newChild.isPseudoElement())
        return Exception { HierarchyRequestError };

    if (containsConsideringHostElements(newChild, newParent))
        return Exception { HierarchyRequestError };

    if (operation == Document::AcceptChildOperation::InsertOrAdd && refChild && refChild->parentNode() != &newParent)
        return Exception { NotFoundError };

    if (is<Document>(newParent)) {
        if (!downcast<Document>(newParent).canAcceptChild(newChild, refChild, operation))
            return Exception { HierarchyRequestError };
    } else if (!isChildTypeAllowed(newParent, newChild))
        return Exception { HierarchyRequestError };

    return { };
}

static inline ExceptionOr<void> checkAcceptChildGuaranteedNodeTypes(ContainerNode& newParent, Node& newChild)
{
    ASSERT(!newParent.isDocumentTypeNode());
    ASSERT(isChildTypeAllowed(newParent, newChild));
    if (newChild.contains(&newParent))
        return Exception { HierarchyRequestError };
    return { };
}

// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity
ExceptionOr<void> ContainerNode::ensurePreInsertionValidity(Node& newChild, Node* refChild)
{
    return checkAcceptChild(*this, newChild, refChild, Document::AcceptChildOperation::InsertOrAdd);
}

// https://dom.spec.whatwg.org/#concept-node-replace
static inline ExceptionOr<void> checkPreReplacementValidity(ContainerNode& newParent, Node& newChild, Node& oldChild)
{
    return checkAcceptChild(newParent, newChild, &oldChild, Document::AcceptChildOperation::Replace);
}

ExceptionOr<void> ContainerNode::insertBefore(Node& newChild, Node* refChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    // Make sure adding the new child is OK.
    auto validityCheckResult = ensurePreInsertionValidity(newChild, refChild);
    if (validityCheckResult.hasException())
        return validityCheckResult.releaseException();

    if (refChild == &newChild)
        refChild = newChild.nextSibling();

    // insertBefore(node, null) is equivalent to appendChild(node)
    if (!refChild)
        return appendChildWithoutPreInsertionValidityCheck(newChild);

    Ref<ContainerNode> protectedThis(*this);
    Ref<Node> next(*refChild);

    NodeVector targets;
    auto removeResult = collectChildrenAndRemoveFromOldParent(newChild, targets);
    if (removeResult.hasException())
        return removeResult.releaseException();
    if (targets.isEmpty())
        return { };

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    auto checkAcceptResult = checkAcceptChildGuaranteedNodeTypes(*this, newChild);
    if (checkAcceptResult.hasException())
        return checkAcceptResult.releaseException();

    InspectorInstrumentation::willInsertDOMNode(document(), *this);

    ChildListMutationScope mutation(*this);
    for (auto& child : targets) {
        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child->parentNode())
            break;

        executeNodeInsertionWithScriptAssertion(*this, child.get(), ChildChangeSource::API, ReplacedAllChildren::No, [&] {
            child->setTreeScopeRecursively(treeScope());
            insertBeforeCommon(next, child);
        });
    }

    dispatchSubtreeModifiedEvent();
    return { };
}

void ContainerNode::insertBeforeCommon(Node& nextChild, Node& newChild)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    ASSERT(!newChild.parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild.nextSibling());
    ASSERT(!newChild.previousSibling());
    ASSERT(!newChild.isShadowRoot());

    Node* prev = nextChild.previousSibling();
    ASSERT(m_lastChild != prev);
    nextChild.setPreviousSibling(&newChild);
    if (prev) {
        ASSERT(m_firstChild != &nextChild);
        ASSERT(prev->nextSibling() == &nextChild);
        prev->setNextSibling(&newChild);
    } else {
        ASSERT(m_firstChild == &nextChild);
        m_firstChild = &newChild;
    }
    newChild.setParentNode(this);
    newChild.setPreviousSibling(prev);
    newChild.setNextSibling(&nextChild);
}

void ContainerNode::appendChildCommon(Node& child)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    child.setParentNode(this);

    if (m_lastChild) {
        child.setPreviousSibling(m_lastChild);
        m_lastChild->setNextSibling(&child);
    } else
        m_firstChild = &child;

    m_lastChild = &child;
}

void ContainerNode::parserInsertBefore(Node& newChild, Node& nextChild)
{
    ASSERT(nextChild.parentNode() == this);
    ASSERT(!newChild.isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));

    if (nextChild.previousSibling() == &newChild || &nextChild == &newChild) // nothing to do
        return;

    executeNodeInsertionWithScriptAssertion(*this, newChild, ChildChangeSource::Parser, ReplacedAllChildren::No, [&] {
        if (&document() != &newChild.document())
            document().adoptNode(newChild);

        insertBeforeCommon(nextChild, newChild);

        newChild.updateAncestorConnectedSubframeCountForInsertion();
    });
}

ExceptionOr<void> ContainerNode::replaceChild(Node& newChild, Node& oldChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protectedThis(*this);

    // Make sure replacing the old child with the new is ok
    auto validityResult = checkPreReplacementValidity(*this, newChild, oldChild);
    if (validityResult.hasException())
        return validityResult.releaseException();

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild.parentNode() != this)
        return Exception { NotFoundError };

    RefPtr<Node> refChild = oldChild.nextSibling();
    if (refChild.get() == &newChild)
        refChild = refChild->nextSibling();

    NodeVector targets;
    {
        ChildListMutationScope mutation(*this);
        auto collectResult = collectChildrenAndRemoveFromOldParent(newChild, targets);
        if (collectResult.hasException())
            return collectResult.releaseException();
    }

    // Do this one more time because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    validityResult = checkPreReplacementValidity(*this, newChild, oldChild);
    if (validityResult.hasException())
        return validityResult.releaseException();

    // Remove the node we're replacing.
    Ref<Node> protectOldChild(oldChild);

    ChildListMutationScope mutation(*this);

    // If oldChild == newChild then oldChild no longer has a parent at this point.
    if (oldChild.parentNode()) {
        auto removeResult = removeChild(oldChild);
        if (removeResult.hasException())
            return removeResult.releaseException();

        // Does this one more time because removeChild() fires a MutationEvent.
        validityResult = checkPreReplacementValidity(*this, newChild, oldChild);
        if (validityResult.hasException())
            return validityResult.releaseException();
    }

    InspectorInstrumentation::willInsertDOMNode(document(), *this);

    // Add the new child(ren).
    for (auto& child : targets) {
        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "refChild" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (refChild && refChild->parentNode() != this)
            break;
        if (child->parentNode())
            break;

        executeNodeInsertionWithScriptAssertion(*this, child.get(), ChildChangeSource::API, ReplacedAllChildren::No, [&] {
            child->setTreeScopeRecursively(treeScope());
            if (refChild)
                insertBeforeCommon(*refChild, child.get());
            else
                appendChildCommon(child);
        });
    }

    dispatchSubtreeModifiedEvent();
    return { };
}

void ContainerNode::disconnectDescendantFrames()
{
    disconnectSubframesIfNeeded(*this, RootAndDescendants);
}

ExceptionOr<void> ContainerNode::removeChild(Node& oldChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protectedThis(*this);

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild.parentNode() != this)
        return Exception { NotFoundError };

    if (!removeNodeWithScriptAssertion(oldChild, ChildChangeSource::API))
        return Exception { NotFoundError };

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();

    return { };
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    InspectorInstrumentation::didRemoveDOMNode(oldChild.document(), oldChild);

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    ASSERT(oldChild.parentNode() == this);

    destroyRenderTreeIfNeeded(oldChild);

    if (nextChild) {
        nextChild->setPreviousSibling(previousChild);
        oldChild.setNextSibling(nullptr);
    } else {
        ASSERT(m_lastChild == &oldChild);
        m_lastChild = previousChild;
    }
    if (previousChild) {
        previousChild->setNextSibling(nextChild);
        oldChild.setPreviousSibling(nullptr);
    } else {
        ASSERT(m_firstChild == &oldChild);
        m_firstChild = nextChild;
    }

    ASSERT(m_firstChild != &oldChild);
    ASSERT(m_lastChild != &oldChild);
    ASSERT(!oldChild.previousSibling());
    ASSERT(!oldChild.nextSibling());
    oldChild.setParentNode(nullptr);

    oldChild.setTreeScopeRecursively(document());
}

void ContainerNode::parserRemoveChild(Node& oldChild)
{
    removeNodeWithScriptAssertion(oldChild, ChildChangeSource::Parser);
}

// https://dom.spec.whatwg.org/#concept-node-replace-all
void ContainerNode::replaceAllChildren(std::nullptr_t)
{
    ChildListMutationScope mutation(*this);
    removeChildren();
}

// https://dom.spec.whatwg.org/#concept-node-replace-all
void ContainerNode::replaceAllChildren(Ref<Node>&& node)
{
    // This function assumes the input node is not a DocumentFragment and is parentless to decrease complexity.
    ASSERT(!is<DocumentFragment>(node));
    ASSERT(!node->parentNode());

    if (!hasChildNodes()) {
        // appendChildWithoutPreInsertionValidityCheck() can only throw when node has a parent and we already asserted it doesn't.
        auto result = appendChildWithoutPreInsertionValidityCheck(node);
        ASSERT_UNUSED(result, !result.hasException());
        return;
    }

    Ref<ContainerNode> protectedThis(*this);
    ChildListMutationScope mutation(*this);
    removeAllChildrenWithScriptAssertion(ChildChangeSource::API, DeferChildrenChanged::Yes);

    executeNodeInsertionWithScriptAssertion(*this, node.get(), ChildChangeSource::API, ReplacedAllChildren::Yes, [&] {
        ASSERT(!ensurePreInsertionValidity(node, nullptr).hasException());
        InspectorInstrumentation::willInsertDOMNode(document(), *this);
        node->setTreeScopeRecursively(treeScope());
        appendChildCommon(node);
    });

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();
}

inline void ContainerNode::rebuildSVGExtensionsElementsIfNecessary()
{
    if (document().svgExtensions() && !is<SVGUseElement>(shadowHost()))
        document().accessSVGExtensions().rebuildElements();
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    Ref<ContainerNode> protectedThis(*this);
    removeAllChildrenWithScriptAssertion(ChildChangeSource::API);

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();
}

ExceptionOr<void> ContainerNode::appendChild(Node& newChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    // Make sure adding the new child is ok
    auto validityCheckResult = ensurePreInsertionValidity(newChild, nullptr);
    if (validityCheckResult.hasException())
        return validityCheckResult.releaseException();

    return appendChildWithoutPreInsertionValidityCheck(newChild);
}

ExceptionOr<void> ContainerNode::appendChildWithoutPreInsertionValidityCheck(Node& newChild)
{
    Ref<ContainerNode> protectedThis(*this);

    NodeVector targets;
    auto removeResult = collectChildrenAndRemoveFromOldParent(newChild, targets);
    if (removeResult.hasException())
        return removeResult.releaseException();

    if (targets.isEmpty())
        return { };

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    auto nodeTypeResult = checkAcceptChildGuaranteedNodeTypes(*this, newChild);
    if (nodeTypeResult.hasException())
        return nodeTypeResult.releaseException();

    InspectorInstrumentation::willInsertDOMNode(document(), *this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (auto& child : targets) {
        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child->parentNode())
            break;

        // Append child to the end of the list
        executeNodeInsertionWithScriptAssertion(*this, child.get(), ChildChangeSource::API, ReplacedAllChildren::No, [&] {
            child->setTreeScopeRecursively(treeScope());
            appendChildCommon(child);
        });
    }

    dispatchSubtreeModifiedEvent();
    return { };
}

void ContainerNode::parserAppendChild(Node& newChild)
{
    ASSERT(!newChild.parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild.isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));

    executeNodeInsertionWithScriptAssertion(*this, newChild, ChildChangeSource::Parser, ReplacedAllChildren::No, [&] {
        if (&document() != &newChild.document())
            document().adoptNode(newChild);

        appendChildCommon(newChild);
        newChild.setTreeScopeRecursively(treeScope());
        newChild.updateAncestorConnectedSubframeCountForInsertion();
    });
}

static bool affectsElements(const ContainerNode::ChildChange& change)
{
    switch (change.type) {
    case ContainerNode::ElementInserted:
    case ContainerNode::ElementRemoved:
    case ContainerNode::AllChildrenRemoved:
    case ContainerNode::AllChildrenReplaced:
        return true;
    case ContainerNode::TextInserted:
    case ContainerNode::TextRemoved:
    case ContainerNode::TextChanged:
    case ContainerNode::NonContentsChildInserted:
    case ContainerNode::NonContentsChildRemoved:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void ContainerNode::childrenChanged(const ChildChange& change)
{
    document().incDOMTreeVersion();

    if (affectsElements(change))
        document().invalidateAccessKeyCache();

    // FIXME: Unclear why it's always safe to skip this when parser is adding children.
    // FIXME: Seems like it's equally safe to skip for TextInserted and TextRemoved as for TextChanged.
    // FIXME: Should use switch for change type so we remember to update when adding new types.
    if (change.source == ChildChangeSource::API && change.type != TextChanged)
        document().updateRangesAfterChildrenChanged(*this);

    invalidateNodeListAndCollectionCachesInAncestors();
}

void ContainerNode::cloneChildNodes(ContainerNode& clone)
{
    Document& targetDocument = clone.document();
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        auto clonedChild = child->cloneNodeInternal(targetDocument, CloningOperation::SelfWithTemplateContent);
        if (!clone.appendChild(clonedChild).hasException() && is<ContainerNode>(*child))
            downcast<ContainerNode>(*child).cloneChildNodes(downcast<ContainerNode>(clonedChild.get()));
    }
}

unsigned ContainerNode::countChildNodes() const
{
    unsigned count = 0;
    for (Node* child = firstChild(); child; child = child->nextSibling())
        ++count;
    return count;
}

Node* ContainerNode::traverseToChildAt(unsigned index) const
{
    Node* child = firstChild();
    for (; child && index > 0; --index)
        child = child->nextSibling();
    return child;
}

static void dispatchChildInsertionEvents(Node& child)
{
    if (child.isInShadowTree())
        return;

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(child));

    RefPtr<Node> c = &child;
    Ref<Document> document(child.document());

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, Event::CanBubble::Yes, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->isConnected() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, Event::CanBubble::No));
    }
}

static void dispatchChildRemovalEvents(Ref<Node>& child)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(child));
    InspectorInstrumentation::willRemoveDOMNode(child->document(), child.get());

    if (child->isInShadowTree())
        return;

    // FIXME: This doesn't belong in dispatchChildRemovalEvents.
    // FIXME: Nodes removed from a shadow tree should also be kept alive.
    willCreatePossiblyOrphanedTreeByRemoval(child.ptr());

    Ref<Document> document = child->document();

    // dispatch pre-removal mutation events
    if (child->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        child->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, Event::CanBubble::Yes, child->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (child->isConnected() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (RefPtr<Node> currentNode = child.copyRef(); currentNode; currentNode = NodeTraversal::next(*currentNode, child.ptr()))
            currentNode->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, Event::CanBubble::No));
    }
}

ExceptionOr<Element*> ContainerNode::querySelector(const String& selectors)
{
    auto query = document().selectorQueryForString(selectors);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().queryFirst(*this);
}

ExceptionOr<Ref<NodeList>> ContainerNode::querySelectorAll(const String& selectors)
{
    auto query = document().selectorQueryForString(selectors);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().queryAll(*this);
}

Ref<HTMLCollection> ContainerNode::getElementsByTagName(const AtomicString& qualifiedName)
{
    ASSERT(!qualifiedName.isNull());

    if (qualifiedName == starAtom())
        return ensureRareData().ensureNodeLists().addCachedCollection<AllDescendantsCollection>(*this, AllDescendants);

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTagCollection>(*this, ByHTMLTag, qualifiedName);
    return ensureRareData().ensureNodeLists().addCachedCollection<TagCollection>(*this, ByTag, qualifiedName);
}

Ref<HTMLCollection> ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    ASSERT(!localName.isNull());
    return ensureRareData().ensureNodeLists().addCachedTagCollectionNS(*this, namespaceURI.isEmpty() ? nullAtom() : namespaceURI, localName);
}

Ref<NodeList> ContainerNode::getElementsByName(const String& elementName)
{
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<NameNodeList>(*this, elementName);
}

Ref<HTMLCollection> ContainerNode::getElementsByClassName(const AtomicString& classNames)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<ClassCollection>(*this, ByClass, classNames);
}

Ref<RadioNodeList> ContainerNode::radioNodeList(const AtomicString& name)
{
    ASSERT(hasTagName(HTMLNames::formTag) || hasTagName(HTMLNames::fieldsetTag));
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<RadioNodeList>(*this, name);
}

Ref<HTMLCollection> ContainerNode::children()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<NodeChildren>::traversalType>>(*this, NodeChildren);
}

Element* ContainerNode::firstElementChild() const
{
    return ElementTraversal::firstChild(*this);
}

Element* ContainerNode::lastElementChild() const
{
    return ElementTraversal::lastChild(*this);
}

unsigned ContainerNode::childElementCount() const
{
    auto children = childrenOfType<Element>(*this);
    return std::distance(children.begin(), children.end());
}

ExceptionOr<void> ContainerNode::append(Vector<NodeOrString>&& vector)
{
    auto result = convertNodesOrStringsIntoNode(WTFMove(vector));
    if (result.hasException())
        return result.releaseException();

    auto node = result.releaseReturnValue();
    if (!node)
        return { };

    return appendChild(*node);
}

ExceptionOr<void> ContainerNode::prepend(Vector<NodeOrString>&& vector)
{
    auto result = convertNodesOrStringsIntoNode(WTFMove(vector));
    if (result.hasException())
        return result.releaseException();

    auto node = result.releaseReturnValue();
    if (!node)
        return { };

    return insertBefore(*node, firstChild());
}

HTMLCollection* ContainerNode::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cachedCollection<HTMLCollection>(type) : nullptr;
}

} // namespace WebCore
