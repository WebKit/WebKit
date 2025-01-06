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
#include "ChildChangeInvalidation.h"
#include "ChildListMutationScope.h"
#include "ClassCollection.h"
#include "CommonAtomStrings.h"
#include "CommonVM.h"
#include "ContainerNodeAlgorithms.h"
#include "Editor.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSlotElement.h"
#include "HTMLTableRowsCollection.h"
#include "InspectorInstrumentation.h"
#include "JSNodeCustom.h"
#include "LabelsNodeList.h"
#include "LocalFrameView.h"
#include "MutationEvent.h"
#include "NodeRareData.h"
#include "NodeRenderStyle.h"
#include "RadioNodeList.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderWidget.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#include "ScriptDisallowedScope.h"
#include "SelectorQuery.h"
#include "SlotAssignment.h"
#include "StaticNodeList.h"
#include "TemplateContentDocumentFragment.h"
#include <algorithm>
#include <variant>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ContainerNode);

struct SameSizeAsContainerNode : public Node {
    void* firstChild;
    void* lastChild;
};

static_assert(sizeof(ContainerNode) == sizeof(SameSizeAsContainerNode), "ContainerNode should stay small");

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Ref<Node>&);

unsigned ScriptDisallowedScope::s_count = 0;
#if ASSERT_ENABLED
ScriptDisallowedScope::EventAllowedScope* ScriptDisallowedScope::EventAllowedScope::s_currentScope = nullptr;
#endif

ALWAYS_INLINE auto ContainerNode::removeAllChildrenWithScriptAssertion(ChildChange::Source source, NodeVector& children, DeferChildrenChanged deferChildrenChanged) -> DidRemoveElements
{
    ASSERT(children.isEmpty());
    collectChildNodes(*this, children);

    if (UNLIKELY(isDocumentFragmentForInnerOuterHTML())) {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        RELEASE_ASSERT(!connectedSubframeCount() && !hasRareData() && !wrapper());
        bool hadElementChild = false;
        while (RefPtr child = m_firstChild.get()) {
            hadElementChild |= is<Element>(*child);
            removeBetween(nullptr, child->protectedNextSibling().get(), *child);
        }
        document().incDOMTreeVersion();
        return hadElementChild ? DidRemoveElements::Yes : DidRemoveElements::No;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(*this));
    if (source == ChildChange::Source::API) {
        ChildListMutationScope mutation(*this);
        for (auto& child : children) {
            mutation.willRemoveChild(child.get());
            child->notifyMutationObserversNodeWillDetach();
            dispatchChildRemovalEvents(child);
        }
    } else {
        ASSERT(source == ChildChange::Source::Parser);
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (UNLIKELY(document().hasMutationObserversOfType(MutationObserverOptionType::ChildList))) {
            ChildListMutationScope mutation(*this);
            for (auto& child : children)
                mutation.willRemoveChild(child.get());
        }
    }

    disconnectSubframesIfNeeded(*this, SubframeDisconnectPolicy::DescendantsOnly);

    ContainerNode::ChildChange childChange { ChildChange::Type::AllChildrenRemoved, nullptr, nullptr, nullptr, source, ContainerNode::ChildChange::AffectsElements::Unknown };

    bool hadElementChild = false;

    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    {
        Style::ChildChangeInvalidation styleInvalidation(*this, childChange);

        if (UNLIKELY(isShadowRoot() || isInShadowTree()))
            containingShadowRoot()->willRemoveAllChildren(*this);

        Ref<Document> { document() }->nodeChildrenWillBeRemoved(*this);

        while (RefPtr child = m_firstChild.get()) {
            if (is<Element>(*child))
                hadElementChild = true;

            removeBetween(nullptr, child->protectedNextSibling().get(), *child);
            auto subtreeObservability = notifyChildNodeRemoved(*this, *child);
            if (source == ChildChange::Source::API && subtreeObservability == RemovedSubtreeObservability::MaybeObservableByRefPtr)
                willCreatePossiblyOrphanedTreeByRemoval(*child);
        }

        childChange.affectsElements = hadElementChild ? ContainerNode::ChildChange::AffectsElements::Yes : ContainerNode::ChildChange::AffectsElements::No;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(!document().selection().selection().isOrphan());

    if (deferChildrenChanged == DeferChildrenChanged::No) {
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
        auto treeVersion = document().domTreeVersion();
#endif
        childrenChanged(childChange);
        ASSERT_WITH_SECURITY_IMPLICATION(document().domTreeVersion() > treeVersion);
    }

    return hadElementChild ? DidRemoveElements::Yes : DidRemoveElements::No;
}

static ContainerNode::ChildChange makeChildChangeForRemoval(Node& childToRemove, ContainerNode::ChildChange::Source source)
{
    auto changeType = [&] {
        if (is<Element>(childToRemove))
            return ContainerNode::ChildChange::Type::ElementRemoved;
        if (is<Text>(childToRemove))
            return ContainerNode::ChildChange::Type::TextRemoved;
        return ContainerNode::ChildChange::Type::NonContentsChildRemoved;
    }();

    return {
        changeType,
        dynamicDowncast<Element>(childToRemove),
        ElementTraversal::previousSibling(childToRemove),
        ElementTraversal::nextSibling(childToRemove),
        source,
        changeType == ContainerNode::ChildChange::Type::ElementRemoved ? ContainerNode::ChildChange::AffectsElements::Yes : ContainerNode::ChildChange::AffectsElements::No
    };
}

ALWAYS_INLINE bool ContainerNode::removeNodeWithScriptAssertion(Node& childToRemove, ChildChange::Source source)
{
    Ref protectedChildToRemove { childToRemove };
    ASSERT_WITH_SECURITY_IMPLICATION(childToRemove.parentNode() == this);
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        ChildListMutationScope(*this).willRemoveChild(childToRemove);
    }

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(childToRemove));
    if (source == ChildChange::Source::API) {
        childToRemove.notifyMutationObserversNodeWillDetach();
        dispatchChildRemovalEvents(protectedChildToRemove);
        if (childToRemove.parentNode() != this)
            return false;
    }

    if (source == ChildChange::Source::Parser) {
        // FIXME: Merge these two code paths. It's a bug in the parser not to update connectedSubframeCount in time.
        disconnectSubframesIfNeeded(*this, SubframeDisconnectPolicy::DescendantsOnly);
    } else {
        ASSERT(source == ChildChange::Source::API);
        if (auto containerChild = dynamicDowncast<ContainerNode>(childToRemove))
            disconnectSubframesIfNeeded(*containerChild, SubframeDisconnectPolicy::RootAndDescendants);
    }

    if (childToRemove.parentNode() != this)
        return false;

    auto childChange = makeChildChangeForRemoval(childToRemove, source);

    RemovedSubtreeObservability subtreeObservability;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        Style::ChildChangeInvalidation styleInvalidation(*this, childChange);

        if (UNLIKELY(isShadowRoot() || isInShadowTree()))
            containingShadowRoot()->resolveSlotsBeforeNodeInsertionOrRemoval();

        Ref<Document> { document() }->nodeWillBeRemoved(childToRemove);

        ASSERT_WITH_SECURITY_IMPLICATION(childToRemove.parentNode() == this);
        ASSERT(!childToRemove.isDocumentFragment());

        RefPtr previousSibling = childToRemove.previousSibling();
        RefPtr nextSibling = childToRemove.nextSibling();

        removeBetween(previousSibling.get(), nextSibling.get(), childToRemove);
        subtreeObservability = notifyChildNodeRemoved(*this, childToRemove);
    }

    if (source == ChildChange::Source::API && subtreeObservability == RemovedSubtreeObservability::MaybeObservableByRefPtr)
        willCreatePossiblyOrphanedTreeByRemoval(childToRemove);

    ASSERT_WITH_SECURITY_IMPLICATION(!document().selection().selection().isOrphan());

    // FIXME: Move childrenChanged into ScriptDisallowedScope block.
    childrenChanged(childChange);

    return true;
}

enum class ReplacedAllChildren { No, YesIncludingElements, YesNotIncludingElements };

static ContainerNode::ChildChange makeChildChangeForInsertion(ContainerNode& containerNode, Node& child, Node* beforeChild, ContainerNode::ChildChange::Source source, ReplacedAllChildren replacedAllChildren)
{
    if (replacedAllChildren != ReplacedAllChildren::No)
        return { ContainerNode::ChildChange::Type::AllChildrenReplaced, nullptr, nullptr, nullptr, source, replacedAllChildren == ReplacedAllChildren::YesIncludingElements ? ContainerNode::ChildChange::AffectsElements::Yes : ContainerNode::ChildChange::AffectsElements::No };

    auto changeType = [&] {
        if (is<Element>(child))
            return ContainerNode::ChildChange::Type::ElementInserted;
        if (is<Text>(child))
            return ContainerNode::ChildChange::Type::TextInserted;
        return ContainerNode::ChildChange::Type::NonContentsChildInserted;
    }();

    auto* beforeChildElement = dynamicDowncast<Element>(beforeChild);
    return {
        changeType,
        dynamicDowncast<Element>(child),
        beforeChild ? ElementTraversal::previousSibling(*beforeChild) : ElementTraversal::lastChild(containerNode),
        !beforeChild || beforeChildElement ? beforeChildElement : ElementTraversal::nextSibling(*beforeChild),
        source,
        changeType == ContainerNode::ChildChange::Type::ElementInserted ? ContainerNode::ChildChange::AffectsElements::Yes : ContainerNode::ChildChange::AffectsElements::No
    };
}

enum class ClonedChildIncludesElements { No, Yes };
static ContainerNode::ChildChange makeChildChangeForCloneInsertion(ClonedChildIncludesElements clonedChildIncludesElements)
{
    return { ContainerNode::ChildChange::Type::AllChildrenReplaced, nullptr, nullptr, nullptr, ContainerNode::ChildChange::Source::Clone,
        clonedChildIncludesElements == ClonedChildIncludesElements::Yes ? ContainerNode::ChildChange::AffectsElements::Yes : ContainerNode::ChildChange::AffectsElements::No };
}

template<typename DOMInsertionWork>
static ALWAYS_INLINE void executeNodeInsertionWithScriptAssertion(ContainerNode& containerNode, Node& child, Node* beforeChild,
    ContainerNode::ChildChange::Source source, ReplacedAllChildren replacedAllChildren, DOMInsertionWork doNodeInsertion)
{
    auto childChange = makeChildChangeForInsertion(containerNode, child, beforeChild, source, replacedAllChildren);

    NodeVector postInsertionNotificationTargets;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        Style::ChildChangeInvalidation styleInvalidation(containerNode, childChange);

        if (UNLIKELY(containerNode.isShadowRoot() || containerNode.isInShadowTree()))
            containerNode.containingShadowRoot()->resolveSlotsBeforeNodeInsertionOrRemoval();

        doNodeInsertion();
        ChildListMutationScope(containerNode).childAdded(child);
        notifyChildNodeInserted(containerNode, child, postInsertionNotificationTargets);
    }

    // FIXME: Move childrenChanged into ScriptDisallowedScope block.
    containerNode.childrenChanged(childChange);

    ASSERT(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(child));
    for (auto& target : postInsertionNotificationTargets)
        target->didFinishInsertingNode();

    if (source == ContainerNode::ChildChange::Source::API)
        dispatchChildInsertionEvents(child);
}

template<typename DOMInsertionWork>
static ALWAYS_INLINE void executeParserNodeInsertionIntoIsolatedTreeWithoutNotifyingParent(ContainerNode& containerNode, Node& child, DOMInsertionWork doNodeInsertion)
{
    NodeVector postInsertionNotificationTargets;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        ASSERT(!containerNode.inRenderedDocument());

        if (UNLIKELY(containerNode.isShadowRoot() || containerNode.isInShadowTree()))
            containerNode.containingShadowRoot()->resolveSlotsBeforeNodeInsertionOrRemoval();

        doNodeInsertion();
        ChildListMutationScope(containerNode).childAdded(child);
        notifyChildNodeInserted(containerNode, child, postInsertionNotificationTargets);
    }
    ASSERT(postInsertionNotificationTargets.isEmpty());
    containerNode.setHasHeldBackChildrenChanged();
}

ExceptionOr<void> ContainerNode::removeSelfOrChildNodesForInsertion(Node& child, NodeVector& nodesForInsertion)
{
    if (auto fragment = dynamicDowncast<DocumentFragment>(child)) {
        if (!fragment->hasChildNodes())
            return { };

        ASSERT(nodesForInsertion.isEmpty());
        fragment->removeAllChildrenWithScriptAssertion(ContainerNode::ChildChange::Source::API, nodesForInsertion);

        fragment->rebuildSVGExtensionsElementsIfNecessary();
        fragment->dispatchSubtreeModifiedEvent();

        return { };
    }

    nodesForInsertion.append(child);
    RefPtr oldParent = child.parentNode();
    if (!oldParent)
        return { };
    return oldParent->removeChild(child);
}

// FIXME: This function must get a new name.
// It removes all children, not just a category called "detached children".
// So this name is terribly confusing.
void ContainerNode::removeDetachedChildren()
{
    if (connectedSubframeCount()) {
        for (RefPtr child = firstChild(); child; child = child->nextSibling())
            child->updateAncestorConnectedSubframeCountForRemoval();
    }
    // FIXME: We should be able to ASSERT(!attached()) here: https://bugs.webkit.org/show_bug.cgi?id=107801
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    removeDetachedChildrenInContainer(*this);
}

static inline bool hasDisplayContents(Element *element)
{
    return element && element->hasDisplayContents();
}

static inline void destroyRenderTreeIfNeeded(Node& child)
{
    auto childAsElement = dynamicDowncast<Element>(child);
    if (!child.renderer() && !hasDisplayContents(childAsElement))
        return;
    if (childAsElement)
        RenderTreeUpdater::tearDownRenderers(*childAsElement);
    else if (auto text = dynamicDowncast<Text>(child))
        RenderTreeUpdater::tearDownRenderer(*text);
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    ASSERT(oldParent);

    NodeVector children;
    oldParent->removeAllChildrenWithScriptAssertion(ChildChange::Source::Parser, children);

    for (auto& child : children) {
        if (child->parentNode()) // Previous parserAppendChild may have mutated DOM.
            continue;
        ASSERT(!ensurePreInsertionValidity(child, nullptr).hasException());
        child->setTreeScopeRecursively(treeScope());
        parserAppendChild(child);
    }
}

ContainerNode::~ContainerNode()
{
    if (!isDocumentNode())
        willBeDeletedFrom(Ref<Document> { document() });
    removeDetachedChildren();
}

static inline bool isChildTypeAllowed(ContainerNode& newParent, Node& child)
{
    if (!child.isDocumentFragment())
        return newParent.childTypeAllowed(child.nodeType());

    for (RefPtr node = child.firstChild(); node; node = node->nextSibling()) {
        if (!newParent.childTypeAllowed(node->nodeType()))
            return false;
    }
    return true;
}

static bool containsIncludingHostElements(const Node& possibleAncestor, const Node& node)
{
    RefPtr<const Node> currentNode = &node;
    do {
        if (currentNode == &possibleAncestor)
            return true;
        RefPtr<const ContainerNode> parent = currentNode->parentNode();
        if (!parent) {
            if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*currentNode))
                parent = shadowRoot->host();
            else if (auto* fragment = dynamicDowncast<TemplateContentDocumentFragment>(*currentNode))
                parent = fragment->host();
        }
        currentNode = WTFMove(parent);
    } while (currentNode);

    return false;
}

enum class ShouldValidateChildParent : bool { No, Yes };
static inline ExceptionOr<void> checkAcceptChild(ContainerNode& newParent, Node& newChild, const Node* refChild, Document::AcceptChildOperation operation, ShouldValidateChildParent shouldValidateChildParent)
{
    if (containsIncludingHostElements(newChild, newParent))
        return Exception { ExceptionCode::HierarchyRequestError };

    // Use common case fast path if possible.
    if ((newChild.isElementNode() || newChild.isTextNode()) && newParent.isElementNode()) {
        ASSERT(!newParent.isDocumentTypeNode());
        ASSERT(isChildTypeAllowed(newParent, newChild));
        if (shouldValidateChildParent == ShouldValidateChildParent::Yes && refChild && refChild->parentNode() != &newParent)
            return Exception { ExceptionCode::NotFoundError };
        return { };
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild.isPseudoElement());
    if (newChild.isPseudoElement())
        return Exception { ExceptionCode::HierarchyRequestError };

    if (shouldValidateChildParent == ShouldValidateChildParent::Yes && refChild && refChild->parentNode() != &newParent)
        return Exception { ExceptionCode::NotFoundError };

    if (auto newParentDocument = dynamicDowncast<Document>(newParent)) {
        if (!newParentDocument->canAcceptChild(newChild, refChild, operation))
            return Exception { ExceptionCode::HierarchyRequestError };
    } else if (!isChildTypeAllowed(newParent, newChild))
        return Exception { ExceptionCode::HierarchyRequestError };

    return { };
}

static inline ExceptionOr<void> checkAcceptChildGuaranteedNodeTypes(ContainerNode& newParent, Node& newChild)
{
    ASSERT(!newParent.isDocumentTypeNode());
    ASSERT(isChildTypeAllowed(newParent, newChild));
    if (containsIncludingHostElements(newChild, newParent))
        return Exception { ExceptionCode::HierarchyRequestError };
    return { };
}

// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity
ExceptionOr<void> ContainerNode::ensurePreInsertionValidity(Node& newChild, Node* refChild)
{
    return checkAcceptChild(*this, newChild, refChild, Document::AcceptChildOperation::InsertOrAdd, ShouldValidateChildParent::Yes);
}

// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity when node is a new DocumentFragment created in "converting nodes into a node"
ExceptionOr<void> ContainerNode::ensurePreInsertionValidityForPhantomDocumentFragment(NodeVector& newChildren, Node* refChild)
{
    if (UNLIKELY(is<Document>(*this))) {
        bool hasSeenElement = false;
        for (auto& child : newChildren) {
            if (!is<Element>(child))
                continue;
            if (hasSeenElement)
                return Exception { ExceptionCode::HierarchyRequestError };
            hasSeenElement = true;
        }
    }
    for (auto& child : newChildren) {
        if (auto result = checkAcceptChild(*this, child, refChild, Document::AcceptChildOperation::InsertOrAdd, ShouldValidateChildParent::Yes); result.hasException())
            return result;
    }
    return { };
}

// https://dom.spec.whatwg.org/#concept-node-replace
static inline ExceptionOr<void> checkPreReplacementValidity(ContainerNode& newParent, Node& newChild, Node& oldChild, ShouldValidateChildParent shouldValidateChildParent)
{
    return checkAcceptChild(newParent, newChild, &oldChild, Document::AcceptChildOperation::Replace, shouldValidateChildParent);
}

ExceptionOr<void> ContainerNode::insertBefore(Node& newChild, RefPtr<Node>&& refChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    // Make sure adding the new child is OK.
    auto validityCheckResult = ensurePreInsertionValidity(newChild, refChild.get());
    if (validityCheckResult.hasException())
        return validityCheckResult.releaseException();

    if (refChild == &newChild)
        refChild = newChild.nextSibling();

    // insertBefore(node, null) is equivalent to appendChild(node)
    if (!refChild)
        return appendChildWithoutPreInsertionValidityCheck(newChild);

    Ref<ContainerNode> protectedThis(*this);
    Ref next = refChild.releaseNonNull();

    NodeVector targets;
    auto removeResult = removeSelfOrChildNodesForInsertion(newChild, targets);
    if (removeResult.hasException())
        return removeResult.releaseException();
    if (targets.isEmpty())
        return { };

    // We need this extra check because removeSelfOrChildNodesForInsertion() can fire mutation events.
    for (auto& child : targets) {
        auto checkAcceptResult = checkAcceptChildGuaranteedNodeTypes(*this, child);
        if (checkAcceptResult.hasException())
            return checkAcceptResult.releaseException();
    }

    InspectorInstrumentation::willInsertDOMNode(protectedDocument(), *this);

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

        executeNodeInsertionWithScriptAssertion(*this, child.get(), next.ptr(), ChildChange::Source::API, ReplacedAllChildren::No, [&] {
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

    RefPtr previousSibling = nextChild.previousSibling();
    ASSERT(m_lastChild != previousSibling.get());
    nextChild.setPreviousSibling(&newChild);
    if (previousSibling) {
        ASSERT(m_firstChild != &nextChild);
        ASSERT(previousSibling->nextSibling() == &nextChild);
        previousSibling->setNextSibling(&newChild);
    } else {
        ASSERT(m_firstChild == &nextChild);
        m_firstChild = &newChild;
    }
    newChild.setParentNode(this);
    newChild.setPreviousSibling(previousSibling.get());
    newChild.setNextSibling(&nextChild);
}

void ContainerNode::appendChildCommon(Node& child)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    child.setParentNode(this);

    if (auto lastChild = protectedLastChild()) {
        child.setPreviousSibling(lastChild.get());
        lastChild->setNextSibling(&child);
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

    executeNodeInsertionWithScriptAssertion(*this, newChild, &nextChild, ChildChange::Source::Parser, ReplacedAllChildren::No, [&] {
        if (&document() != &newChild.document())
            document().adoptNode(newChild);

        insertBeforeCommon(nextChild, newChild);
        newChild.setTreeScopeRecursively(treeScope());
        newChild.updateAncestorConnectedSubframeCountForInsertion();
    });
}

ExceptionOr<void> ContainerNode::replaceChild(Node& newChild, Node& oldChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref protectedThis { *this };

    // Make sure replacing the old child with the new is ok
    auto validityResult = checkPreReplacementValidity(*this, newChild, oldChild, ShouldValidateChildParent::Yes);
    if (validityResult.hasException())
        return validityResult.releaseException();

    RefPtr refChild = oldChild.nextSibling();
    if (refChild.get() == &newChild)
        refChild = refChild->nextSibling();

    NodeVector targets;
    {
        ChildListMutationScope mutation(*this);
        auto collectResult = removeSelfOrChildNodesForInsertion(newChild, targets);
        if (collectResult.hasException())
            return collectResult.releaseException();
    }

    // Do this one more time because removeSelfOrChildNodesForInsertion() fires a MutationEvent.
    for (auto& child : targets) {
        validityResult = checkPreReplacementValidity(*this, child, oldChild, ShouldValidateChildParent::No);
        if (validityResult.hasException())
            return validityResult.releaseException();
    }

    // Remove the node we're replacing.
    Ref protectOldChild { oldChild };

    ChildListMutationScope mutation(*this);

    // If oldChild == newChild then oldChild no longer has a parent at this point.
    if (oldChild.parentNode()) {
        auto removeResult = removeChild(oldChild);
        if (removeResult.hasException())
            return removeResult.releaseException();

        // Does this one more time because removeChild() fires a MutationEvent.
        for (auto& child : targets) {
            validityResult = checkPreReplacementValidity(*this, child, oldChild, ShouldValidateChildParent::No);
            if (validityResult.hasException())
                return validityResult.releaseException();
        }
    }

    InspectorInstrumentation::willInsertDOMNode(protectedDocument(), *this);

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

        executeNodeInsertionWithScriptAssertion(*this, child.get(), refChild.get(), ChildChange::Source::API, ReplacedAllChildren::No, [&] {
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
    disconnectSubframesIfNeeded(*this, SubframeDisconnectPolicy::RootAndDescendants);
}

ExceptionOr<void> ContainerNode::removeChild(Node& oldChild)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref protectedThis { *this };
    Ref protectedOldChild { oldChild };

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild.parentNode() != this)
        return Exception { ExceptionCode::NotFoundError };

    if (!removeNodeWithScriptAssertion(oldChild, ChildChange::Source::API))
        return Exception { ExceptionCode::NotFoundError };

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();

    return { };
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    InspectorInstrumentation::didRemoveDOMNode(Ref<Document> { oldChild.document() }, oldChild);

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    ASSERT(oldChild.parentNode() == this);

    destroyRenderTreeIfNeeded(oldChild);

    if (UNLIKELY(hasShadowRootContainingSlots()))
        shadowRoot()->willRemoveAssignedNode(oldChild);

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
    removeNodeWithScriptAssertion(oldChild, ChildChange::Source::Parser);
}

// https://dom.spec.whatwg.org/#concept-node-replace-all
void ContainerNode::replaceAll(Node* node)
{
    if (!node) {
        ChildListMutationScope mutation(*this);
        removeChildren();
        return;
    }

    // FIXME: The code below is roughly correct for a new text node with no parent, but needs enhancement to work properly for more complex cases.

    if (!hasChildNodes()) {
        appendChildWithoutPreInsertionValidityCheck(*node);
        return;
    }

    Ref protectedThis { *this };
    ChildListMutationScope mutation(*this);
    NodeVector removedChildren;
    auto didRemoveElements = removeAllChildrenWithScriptAssertion(ChildChange::Source::API, removedChildren, DeferChildrenChanged::Yes);

    auto replacedAllChildren = is<Element>(*node) || didRemoveElements == DidRemoveElements::Yes ? ReplacedAllChildren::YesIncludingElements : ReplacedAllChildren::YesNotIncludingElements;

    executeNodeInsertionWithScriptAssertion(*this, *node, nullptr, ChildChange::Source::API, replacedAllChildren, [&] {
        InspectorInstrumentation::willInsertDOMNode(protectedDocument(), *this);
        node->setTreeScopeRecursively(treeScope());
        appendChildCommon(*node);
    });

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();
}

// https://dom.spec.whatwg.org/#string-replace-all
void ContainerNode::stringReplaceAll(String&& string)
{
    replaceAll(string.isEmpty() ? nullptr : document().createTextNode(WTFMove(string)).ptr());
}

inline void ContainerNode::rebuildSVGExtensionsElementsIfNecessary()
{
    Ref<Document> document = this->document();
    if (document->svgExtensionsIfExists() && !is<SVGUseElement>(shadowHost()))
        document->checkedSVGExtensions()->rebuildElements();
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    Ref protectedThis { *this };
    NodeVector removedChildren;
    removeAllChildrenWithScriptAssertion(ChildChange::Source::API, removedChildren);

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
    Ref protectedThis { *this };

    NodeVector targets;
    auto removeResult = removeSelfOrChildNodesForInsertion(newChild, targets);
    if (removeResult.hasException())
        return removeResult.releaseException();

    if (targets.isEmpty())
        return { };

    // We need this extra check because removeSelfOrChildNodesForInsertion() can fire mutation events.
    for (auto& child : targets) {
        auto nodeTypeResult = checkAcceptChildGuaranteedNodeTypes(*this, child);
        if (nodeTypeResult.hasException())
            return nodeTypeResult.releaseException();
    }

    InspectorInstrumentation::willInsertDOMNode(protectedDocument(), *this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (auto& child : targets) {
        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child->parentNode())
            break;

        // Append child to the end of the list
        executeNodeInsertionWithScriptAssertion(*this, child.get(), nullptr, ChildChange::Source::API, ReplacedAllChildren::No, [&] {
            child->setTreeScopeRecursively(treeScope());
            appendChildCommon(child);
        });
    }

    dispatchSubtreeModifiedEvent();
    return { };
}

ExceptionOr<void> ContainerNode::insertChildrenBeforeWithoutPreInsertionValidityCheck(NodeVector&& newChildren, Node* nextChild)
{
    RefPtr refChild = nextChild;
    for (auto& child : newChildren) {
        if (RefPtr oldParent = child->parentNode()) {
            if (refChild.get() == child.ptr())
                refChild = child->nextSibling();
            if (auto result = oldParent->removeChild(child); result.hasException())
                return result.releaseException();
        }
    }

    // We need this extra check because removeChild() above can fire mutation events.
    for (auto& child : newChildren) {
        auto nodeTypeResult = checkAcceptChildGuaranteedNodeTypes(*this, child);
        if (nodeTypeResult.hasException())
            return nodeTypeResult.releaseException();
    }

    InspectorInstrumentation::willInsertDOMNode(protectedDocument(), *this);

    ChildListMutationScope mutation(*this);
    for (auto& child : newChildren) {
        if (refChild && refChild->parentNode() != this) // Event listeners moved nextChild elsewhere.
            break;
        if (child->parentNode()) // Event listeners inserted this child elsewhere.
            break;
        executeNodeInsertionWithScriptAssertion(*this, child.get(), refChild.get(), ChildChange::Source::API, ReplacedAllChildren::No, [&] {
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

void ContainerNode::parserAppendChild(Node& newChild)
{
    ASSERT(!newChild.parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild.isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));

    executeNodeInsertionWithScriptAssertion(*this, newChild, nullptr, ChildChange::Source::Parser, ReplacedAllChildren::No, [&] {
        if (&document() != &newChild.document())
            document().adoptNode(newChild);

        appendChildCommon(newChild);
        newChild.setTreeScopeRecursively(treeScope());
        newChild.updateAncestorConnectedSubframeCountForInsertion();
    });
}

void ContainerNode::parserAppendChildIntoIsolatedTree(Node& newChild)
{
    ASSERT(!inRenderedDocument());
    ASSERT(!newChild.traverseToRootNode().wrapper());
    ASSERT(!newChild.parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(is<Element>(*this) || is<DocumentFragment>(*this)); // Only Element calls parserNotifyChildrenChanged
    ASSERT(is<DocumentFragment>(*this) || !isParsingChildrenFinished());
    ASSERT(!newChild.isDocumentFragment());
    ASSERT(!hasTagName(HTMLNames::templateTag));
    RELEASE_ASSERT(&newChild.document() == &document());

    executeParserNodeInsertionIntoIsolatedTreeWithoutNotifyingParent(*this, newChild, [&] {
        appendChildCommon(newChild);
        newChild.setTreeScopeRecursively(treeScope());
        newChild.updateAncestorConnectedSubframeCountForInsertion();
    });
}

void ContainerNode::parserNotifyChildrenChanged()
{
    ASSERT(!inRenderedDocument());
    ASSERT(!traverseToRootNode().wrapper());
    ASSERT(is<Element>(*this));
    ASSERT(hasHeldBackChildrenChanged());
    clearHasHeldBackChildrenChanged();
    childrenChanged(ChildChange { ContainerNode::ChildChange::Type::AllChildrenReplaced, nullptr, nullptr, nullptr, ChildChange::Source::Parser,
        firstElementChild() ? ChildChange::AffectsElements::Yes : ChildChange::AffectsElements::No });
}

ExceptionOr<void> ContainerNode::appendChild(ChildChange::Source source, Node& newChild)
{
    if (source == ChildChange::Source::Parser) {
        parserAppendChild(newChild);
        return { };
    }
    return appendChild(newChild);
}

void ContainerNode::childrenChanged(const ChildChange& change)
{
    Ref<Document> document = this->document();
    document->incDOMTreeVersion();

    if (change.affectsElements == ChildChange::AffectsElements::Yes)
        document->invalidateAccessKeyCache();

    // FIXME: Unclear why it's always safe to skip this when parser is adding children.
    // FIXME: Seems like it's equally safe to skip for TextInserted and TextRemoved as for TextChanged.
    // FIXME: Should use switch for change type so we remember to update when adding new types.
    if (change.source == ChildChange::Source::API && change.type != ChildChange::Type::TextChanged)
        document->updateRangesAfterChildrenChanged(*this);

    if (change.affectsElements == ChildChange::AffectsElements::Yes)
        invalidateNodeListAndCollectionCachesInAncestors();
    else if (change.type != ChildChange::Type::TextChanged) {
        if (hasRareData()) {
            if (auto* lists = rareData()->nodeLists())
                lists->clearChildNodeListCache();
        }
    }
}

void ContainerNode::cloneChildNodes(TreeScope& treeScope, ContainerNode& clone)
{
    NodeVector postInsertionNotificationTargets;
    bool hadElement = false;
    for (RefPtr child = firstChild(); child; child = child->nextSibling()) {
        Ref clonedChild = child->cloneNodeInternal(treeScope, CloningOperation::SelfWithTemplateContent);
        {
            WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
            ScriptDisallowedScope::InMainThread scriptDisallowedScope;

            clonedChild->setTreeScopeRecursively(treeScope);

            clone.appendChildCommon(clonedChild);
            notifyChildNodeInserted(clone, clonedChild, postInsertionNotificationTargets);

            hadElement = hadElement || is<Element>(clonedChild);
        }
        if (RefPtr childAsContainerNode = dynamicDowncast<ContainerNode>(*child))
            childAsContainerNode->cloneChildNodes(treeScope, downcast<ContainerNode>(clonedChild));
    }
    clone.childrenChanged(makeChildChangeForCloneInsertion(hadElement ? ClonedChildIncludesElements::Yes : ClonedChildIncludesElements::No));

    for (auto& target : postInsertionNotificationTargets)
        target->didFinishInsertingNode();
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

    RefPtr c = &child;
    Ref document = child.document();

    if (c->parentNode() && document->hasListenerType(Document::ListenerType::DOMNodeInserted))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, Event::CanBubble::Yes, c->protectedParentNode().get()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->isConnected() && document->hasListenerType(Document::ListenerType::DOMNodeInsertedIntoDocument)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, Event::CanBubble::No));
    }
}

static void dispatchChildRemovalEvents(Ref<Node>& child)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(child));
    Ref<Document> document = child->document();
    InspectorInstrumentation::willRemoveDOMNode(document, child.get());

    if (child->isInShadowTree() || document->shouldNotFireMutationEvents())
        return;

    // dispatch pre-removal mutation events
    if (child->parentNode() && document->hasListenerType(Document::ListenerType::DOMNodeRemoved))
        child->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, Event::CanBubble::Yes, child->protectedParentNode().get()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (child->isConnected() && document->hasListenerType(Document::ListenerType::DOMNodeRemovedFromDocument)) {
        for (RefPtr currentNode = child.copyRef(); currentNode; currentNode = NodeTraversal::next(*currentNode, child.ptr()))
            currentNode->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, Event::CanBubble::No));
    }
}

ExceptionOr<Element*> ContainerNode::querySelector(const String& selectors)
{
    auto query = protectedDocument()->selectorQueryForString(selectors);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().queryFirst(*this);
}

ExceptionOr<Ref<NodeList>> ContainerNode::querySelectorAll(const String& selectors)
{
    auto document = protectedDocument();
    if (auto results = document->resultForSelectorAll(*this, selectors))
        return Ref<NodeList> { StaticWrapperNodeList::create(results.releaseNonNull()) };
    auto query = document->selectorQueryForString(selectors);
    if (query.hasException())
        return query.releaseException();
    auto isCacheable = query.returnValue().shouldStoreInDocument();
    auto classNameToMatch = query.returnValue().classNameToMatch();
    auto nodeList = query.releaseReturnValue().queryAll(*this);
    if (isCacheable)
        document->addResultForSelectorAll(*this, selectors, nodeList, classNameToMatch);
    return nodeList;
}

Ref<HTMLCollection> ContainerNode::getElementsByTagName(const AtomString& qualifiedName)
{
    ASSERT(!qualifiedName.isNull());

    if (qualifiedName == starAtom())
        return ensureRareData().ensureNodeLists().addCachedCollection<AllDescendantsCollection>(*this, CollectionType::AllDescendants);

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTagCollection>(*this, CollectionType::ByHTMLTag, qualifiedName);
    return ensureRareData().ensureNodeLists().addCachedCollection<TagCollection>(*this, CollectionType::ByTag, qualifiedName);
}

Ref<HTMLCollection> ContainerNode::getElementsByTagNameNS(const AtomString& namespaceURI, const AtomString& localName)
{
    ASSERT(!localName.isNull());
    return ensureRareData().ensureNodeLists().addCachedTagCollectionNS(*this, namespaceURI.isEmpty() ? nullAtom() : namespaceURI, localName);
}

Ref<HTMLCollection> ContainerNode::getElementsByClassName(const AtomString& classNames)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<ClassCollection>(*this, CollectionType::ByClass, classNames);
}

Ref<RadioNodeList> ContainerNode::radioNodeList(const AtomString& name)
{
    ASSERT(hasTagName(HTMLNames::formTag) || hasTagName(HTMLNames::fieldsetTag));
    return ensureRareData().ensureNodeLists().addCacheWithAtomName<RadioNodeList>(*this, name);
}

Ref<HTMLCollection> ContainerNode::children()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<CollectionType::NodeChildren>::traversalType>>(*this, CollectionType::NodeChildren);
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
    return std::distance(children.begin(), { });
}

ExceptionOr<void> ContainerNode::append(FixedVector<NodeOrString>&& vector)
{
    auto result = convertNodesOrStringsIntoNodeVector(WTFMove(vector));
    if (result.hasException())
        return result.releaseException();

    auto newChildren = result.releaseReturnValue();
    if (auto checkResult = ensurePreInsertionValidityForPhantomDocumentFragment(newChildren); checkResult.hasException())
        return checkResult;

    Ref protectedThis { *this };
    ChildListMutationScope mutation(*this);
    if (auto appendResult = insertChildrenBeforeWithoutPreInsertionValidityCheck(WTFMove(newChildren)); appendResult.hasException())
        return appendResult;

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();

    return { };
}

ExceptionOr<void> ContainerNode::prepend(FixedVector<NodeOrString>&& vector)
{
    auto result = convertNodesOrStringsIntoNodeVector(WTFMove(vector));
    if (result.hasException())
        return result.releaseException();

    RefPtr nextChild = protectedFirstChild();
    auto newChildren = result.releaseReturnValue();
    if (auto checkResult = ensurePreInsertionValidityForPhantomDocumentFragment(newChildren, nextChild.get()); checkResult.hasException())
        return checkResult;

    Ref protectedThis { *this };
    ChildListMutationScope mutation(*this);
    if (auto appendResult = insertChildrenBeforeWithoutPreInsertionValidityCheck(WTFMove(newChildren), nextChild.get()); appendResult.hasException())
        return appendResult;

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();

    return { };
}

// https://dom.spec.whatwg.org/#dom-parentnode-replacechildren
ExceptionOr<void> ContainerNode::replaceChildren(FixedVector<NodeOrString>&& vector)
{
    auto result = convertNodesOrStringsIntoNodeVector(WTFMove(vector));
    if (result.hasException())
        return result.releaseException();
    auto newChildren = result.releaseReturnValue();

    if (auto checkResult = ensurePreInsertionValidityForPhantomDocumentFragment(newChildren); checkResult.hasException())
        return checkResult;

    Ref protectedThis { *this };
    ChildListMutationScope mutation(*this);
    NodeVector removedChildren;
    removeAllChildrenWithScriptAssertion(ChildChange::Source::API, removedChildren, DeferChildrenChanged::No);

    if (auto appendResult = insertChildrenBeforeWithoutPreInsertionValidityCheck(WTFMove(newChildren)); appendResult.hasException())
        return appendResult;

    rebuildSVGExtensionsElementsIfNecessary();
    dispatchSubtreeModifiedEvent();

    return { };
}

HTMLCollection* ContainerNode::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cachedCollection<HTMLCollection>(type) : nullptr;
}

ContainerNode& ContainerNode::traverseToRootNode() const
{
    return traverseToRootNodeInternal(*this);
}

} // namespace WebCore
