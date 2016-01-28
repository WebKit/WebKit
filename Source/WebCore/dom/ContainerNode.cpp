/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "ChildListMutationScope.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassCollection.h"
#include "ContainerNodeAlgorithms.h"
#include "Editor.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSlotElement.h"
#include "HTMLTableRowsCollection.h"
#include "InlineTextBox.h"
#include "JSLazyEventListener.h"
#include "JSNode.h"
#include "LabelsNodeList.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NodeOrString.h"
#include "NodeRareData.h"
#include "NodeRenderStyle.h"
#include "RadioNodeList.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "RenderWidget.h"
#include "RootInlineBox.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SelectorQuery.h"
#include "StyleTreeResolver.h"
#include "TemplateContentDocumentFragment.h"
#include <algorithm>
#include <wtf/CurrentTime.h>

namespace WebCore {

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Node&);

ChildNodesLazySnapshot* ChildNodesLazySnapshot::latestSnapshot;

#ifndef NDEBUG
unsigned NoEventDispatchAssertion::s_count = 0;
#endif

static void collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes, ExceptionCode& ec)
{
    if (!is<DocumentFragment>(node)) {
        nodes.append(node);
        if (ContainerNode* oldParent = node.parentNode())
            oldParent->removeChild(node, ec);
        return;
    }

    getChildNodes(node, nodes);
    downcast<DocumentFragment>(node).removeChildren();
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
    removeDetachedChildrenInContainer(*this);
}

static inline void destroyRenderTreeIfNeeded(Node& child)
{
    // FIXME: Get rid of the named flow test.
    if (!child.renderer() && !child.isNamedFlowContentNode() && !is<HTMLSlotElement>(child))
        return;
    if (is<Element>(child))
        Style::detachRenderTree(downcast<Element>(child));
    else if (is<Text>(child))
        Style::detachTextRenderer(downcast<Text>(child));
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    ASSERT(oldParent);

    NodeVector children;
    getChildNodes(*oldParent, children);

    if (oldParent->document().hasMutationObserversOfType(MutationObserver::ChildList)) {
        ChildListMutationScope mutation(*oldParent);
        for (auto& child : children)
            mutation.willRemoveChild(child);
    }

    // FIXME: We need to do notifyMutationObserversNodeWillDetach() for each child,
    // probably inside removeDetachedChildrenInContainer.

    oldParent->removeDetachedChildren();

    for (auto& child : children) {
        destroyRenderTreeIfNeeded(child);

        // FIXME: We need a no mutation event version of adoptNode.
        RefPtr<Node> adoptedChild = document().adoptNode(&child.get(), ASSERT_NO_EXCEPTION);
        parserAppendChild(*adoptedChild);
        // FIXME: Together with adoptNode above, the tree scope might get updated recursively twice
        // (if the document changed or oldParent was in a shadow tree, AND *this is in a shadow tree).
        // Can we do better?
        treeScope().adoptIfNeeded(adoptedChild.get());
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
#if ENABLE(TEMPLATE_ELEMENT)
    Document& document = node->document();
    return &document == document.templateDocument();
#else
    UNUSED_PARAM(node);
    return false;
#endif
}

static inline bool containsConsideringHostElements(const Node& newChild, const Node& newParent)
{
    return (newParent.isInShadowTree() || isInTemplateContent(&newParent))
        ? newChild.containsIncludingHostElements(&newParent)
        : newChild.contains(&newParent);
}

static inline ExceptionCode checkAcceptChild(ContainerNode& newParent, Node& newChild, const Node* refChild, Document::AcceptChildOperation operation)
{
    // Use common case fast path if possible.
    if ((newChild.isElementNode() || newChild.isTextNode()) && newParent.isElementNode()) {
        ASSERT(!newParent.isDocumentTypeNode());
        ASSERT(isChildTypeAllowed(newParent, newChild));
        if (containsConsideringHostElements(newChild, newParent))
            return HIERARCHY_REQUEST_ERR;
        return 0;
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild.isPseudoElement());
    if (newChild.isPseudoElement())
        return HIERARCHY_REQUEST_ERR;

    if (containsConsideringHostElements(newChild, newParent))
        return HIERARCHY_REQUEST_ERR;

    if (is<Document>(newParent)) {
        if (!downcast<Document>(newParent).canAcceptChild(newChild, refChild, operation))
            return HIERARCHY_REQUEST_ERR;
    } else if (!isChildTypeAllowed(newParent, newChild))
        return HIERARCHY_REQUEST_ERR;

    return 0;
}

static inline bool checkAcceptChildGuaranteedNodeTypes(ContainerNode& newParent, Node& newChild, ExceptionCode& ec)
{
    ASSERT(!newParent.isDocumentTypeNode());
    ASSERT(isChildTypeAllowed(newParent, newChild));
    if (newChild.contains(&newParent)) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }

    return true;
}

// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity
bool ContainerNode::ensurePreInsertionValidity(Node& newChild, Node* refChild, ExceptionCode& ec)
{
    ec = checkAcceptChild(*this, newChild, refChild, Document::AcceptChildOperation::InsertOrAdd);
    return !ec;
}

// https://dom.spec.whatwg.org/#concept-node-replace
static inline bool checkPreReplacementValidity(ContainerNode& newParent, Node& newChild, Node& oldChild, ExceptionCode& ec)
{
    ec = checkAcceptChild(newParent, newChild, &oldChild, Document::AcceptChildOperation::Replace);
    return !ec;
}

bool ContainerNode::insertBefore(Ref<Node>&& newChild, Node* refChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild)
        return appendChild(WTFMove(newChild), ec);

    // Make sure adding the new child is OK.
    if (!ensurePreInsertionValidity(newChild, refChild, ec))
        return false;

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    if (refChild->previousSibling() == newChild.ptr() || refChild == newChild.ptr()) // nothing to do
        return true;

    Ref<Node> next(*refChild);

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild, targets, ec);
    if (ec)
        return false;
    if (targets.isEmpty())
        return true;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*this, newChild, ec))
        return false;

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

        treeScope().adoptIfNeeded(child.ptr());

        insertBeforeCommon(next, child);

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::insertBeforeCommon(Node& nextChild, Node& newChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

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
    child.setParentNode(this);

    if (m_lastChild) {
        child.setPreviousSibling(m_lastChild);
        m_lastChild->setNextSibling(&child);
    } else
        m_firstChild = &child;

    m_lastChild = &child;
}

void ContainerNode::notifyChildInserted(Node& child, ChildChangeSource source)
{
    ChildListMutationScope(*this).childAdded(child);

    NodeVector postInsertionNotificationTargets;
    notifyChildNodeInserted(*this, child, postInsertionNotificationTargets);

    ChildChange change;
    change.type = child.isElementNode() ? ElementInserted : child.isTextNode() ? TextInserted : NonContentsChildChanged;
    change.previousSiblingElement = ElementTraversal::previousSibling(child);
    change.nextSiblingElement = ElementTraversal::nextSibling(child);
    change.source = source;

    childrenChanged(change);

    for (auto& target : postInsertionNotificationTargets)
        target->finishedInsertingSubtree();
}

void ContainerNode::notifyChildRemoved(Node& child, Node* previousSibling, Node* nextSibling, ChildChangeSource source)
{
    notifyChildNodeRemoved(*this, child);

    ChildChange change;
    change.type = is<Element>(child) ? ElementRemoved : is<Text>(child) ? TextRemoved : NonContentsChildChanged;
    change.previousSiblingElement = (!previousSibling || is<Element>(*previousSibling)) ? downcast<Element>(previousSibling) : ElementTraversal::previousSibling(*previousSibling);
    change.nextSiblingElement = (!nextSibling || is<Element>(*nextSibling)) ? downcast<Element>(nextSibling) : ElementTraversal::nextSibling(*nextSibling);
    change.source = source;

    childrenChanged(change);
}

void ContainerNode::parserInsertBefore(Ref<Node>&& newChild, Node& nextChild)
{
    ASSERT(nextChild.parentNode() == this);
    ASSERT(!newChild->isDocumentFragment());
#if ENABLE(TEMPLATE_ELEMENT)
    ASSERT(!hasTagName(HTMLNames::templateTag));
#endif

    if (nextChild.previousSibling() == newChild.ptr() || &nextChild == newChild.ptr()) // nothing to do
        return;

    if (&document() != &newChild->document())
        document().adoptNode(newChild.ptr(), ASSERT_NO_EXCEPTION);

    insertBeforeCommon(nextChild, newChild);

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    notifyChildInserted(newChild, ChildChangeSourceParser);

    newChild->setNeedsStyleRecalc(ReconstructRenderTree);
}

bool ContainerNode::replaceChild(Ref<Node>&& newChild, Node& oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    if (&oldChild == newChild.ptr()) // nothing to do
        return true;

    // Make sure replacing the old child with the new is ok
    if (!checkPreReplacementValidity(*this, newChild, oldChild, ec))
        return false;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (oldChild.parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    ChildListMutationScope mutation(*this);

    RefPtr<Node> refChild = oldChild.nextSibling();
    if (refChild.get() == newChild.ptr())
        refChild = refChild->nextSibling();

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild, targets, ec);
    if (ec)
        return false;

    // Does this one more time because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkPreReplacementValidity(*this, newChild, oldChild, ec))
        return false;

    // Remove the node we're replacing.
    Ref<Node> protectOldChild(oldChild);
    removeChild(oldChild, ec);
    if (ec)
        return false;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkPreReplacementValidity(*this, newChild, oldChild, ec))
        return false;

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

        treeScope().adoptIfNeeded(child.ptr());

        {
            NoEventDispatchAssertion assertNoEventDispatch;
            if (refChild)
                insertBeforeCommon(*refChild, child.get());
            else
                appendChildCommon(child);
        }

        updateTreeAfterInsertion(child.get());
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::willRemoveChild(Node& child)
{
    ASSERT(child.parentNode());

    ChildListMutationScope(*child.parentNode()).willRemoveChild(child);
    child.notifyMutationObserversNodeWillDetach();
    dispatchChildRemovalEvents(child);

    if (child.parentNode() != this)
        return;

    child.document().nodeWillBeRemoved(child); // e.g. mutation event listener can create a new range.
    if (is<ContainerNode>(child))
        disconnectSubframesIfNeeded(downcast<ContainerNode>(child), RootAndDescendants);
}

static void willRemoveChildren(ContainerNode& container)
{
    NodeVector children;
    getChildNodes(container, children);

    ChildListMutationScope mutation(container);
    for (auto& child : children) {
        mutation.willRemoveChild(child.get());
        child->notifyMutationObserversNodeWillDetach();

        // fire removed from document mutation events.
        dispatchChildRemovalEvents(child.get());
    }

    container.document().nodeChildrenWillBeRemoved(container);

    disconnectSubframesIfNeeded(container, DescendantsOnly);
}

void ContainerNode::disconnectDescendantFrames()
{
    disconnectSubframesIfNeeded(*this, RootAndDescendants);
}

bool ContainerNode::removeChild(Node& oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (oldChild.parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    Ref<Node> child(oldChild);

    document().removeFocusedNodeOfSubtree(child.ptr());

#if ENABLE(FULLSCREEN_API)
    document().removeFullScreenElementOfSubtree(&child.get());
#endif

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    willRemoveChild(child);

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, child);

        notifyChildRemoved(child, prev, next, ChildChangeSourceAPI);
    }


    if (document().svgExtensions()) {
        Element* shadowHost = this->shadowHost();
        if (!shadowHost || !shadowHost->hasTagName(SVGNames::useTag))
            document().accessSVGExtensions().rebuildElements();
    }

    dispatchSubtreeModifiedEvent();

    return true;
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    InspectorInstrumentation::didRemoveDOMNode(oldChild.document(), oldChild);

    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(oldChild.parentNode() == this);

    destroyRenderTreeIfNeeded(oldChild);

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == &oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == &oldChild)
        m_lastChild = previousChild;

    oldChild.setPreviousSibling(nullptr);
    oldChild.setNextSibling(nullptr);
    oldChild.setParentNode(nullptr);

    document().adoptIfNeeded(&oldChild);
}

void ContainerNode::parserRemoveChild(Node& oldChild)
{
    ASSERT(oldChild.parentNode() == this);
    ASSERT(!oldChild.isDocumentFragment());

    Node* prev = oldChild.previousSibling();
    Node* next = oldChild.nextSibling();

    oldChild.updateAncestorConnectedSubframeCountForRemoval();

    ChildListMutationScope(*this).willRemoveChild(oldChild);
    oldChild.notifyMutationObserversNodeWillDetach();

    removeBetween(prev, next, oldChild);

    notifyChildRemoved(oldChild, prev, next, ChildChangeSourceParser);
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    // The container node can be removed from event handlers.
    Ref<ContainerNode> protect(*this);

    // exclude this node when looking for removed focusedNode since only children will be removed
    document().removeFocusedNodeOfSubtree(this, true);

#if ENABLE(FULLSCREEN_API)
    document().removeFullScreenElementOfSubtree(this, true);
#endif

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren(*this);

    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            while (RefPtr<Node> child = m_firstChild) {
                removeBetween(0, child->nextSibling(), *child);
                notifyChildNodeRemoved(*this, *child);
            }
        }

        ChildChange change = { AllChildrenRemoved, nullptr, nullptr, ChildChangeSourceAPI };
        childrenChanged(change);
    }

    if (document().svgExtensions()) {
        Element* shadowHost = this->shadowHost();
        if (!shadowHost || !shadowHost->hasTagName(SVGNames::useTag))
            document().accessSVGExtensions().rebuildElements();
    }

    dispatchSubtreeModifiedEvent();
}

bool ContainerNode::appendChild(Ref<Node>&& newChild, ExceptionCode& ec)
{
    Ref<ContainerNode> protect(*this);

    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    ec = 0;

    // Make sure adding the new child is ok
    if (!ensurePreInsertionValidity(newChild, nullptr, ec))
        return false;

    if (newChild.ptr() == m_lastChild) // nothing to do
        return true;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(newChild, targets, ec);
    if (ec)
        return false;

    if (targets.isEmpty())
        return true;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*this, newChild, ec))
        return false;

    InspectorInstrumentation::willInsertDOMNode(document(), *this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (auto& child : targets) {
        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child->parentNode())
            break;

        treeScope().adoptIfNeeded(child.ptr());

        // Append child to the end of the list
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            appendChildCommon(child);
        }

        updateTreeAfterInsertion(child.get());
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::parserAppendChild(Ref<Node>&& newChild)
{
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->isDocumentFragment());
#if ENABLE(TEMPLATE_ELEMENT)
    ASSERT(!hasTagName(HTMLNames::templateTag));
#endif

    if (&document() != &newChild->document())
        document().adoptNode(newChild.ptr(), ASSERT_NO_EXCEPTION);

    {
        NoEventDispatchAssertion assertNoEventDispatch;
        // FIXME: This method should take a PassRefPtr.
        appendChildCommon(newChild);
        treeScope().adoptIfNeeded(newChild.ptr());
    }

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    notifyChildInserted(newChild, ChildChangeSourceParser);

    newChild->setNeedsStyleRecalc(ReconstructRenderTree);
}

void ContainerNode::childrenChanged(const ChildChange& change)
{
    document().incDOMTreeVersion();
    if (change.source == ChildChangeSourceAPI && change.type != TextChanged)
        document().updateRangesAfterChildrenChanged(*this);
    invalidateNodeListAndCollectionCachesInAncestors();
}

void ContainerNode::cloneChildNodes(ContainerNode& clone)
{
    ExceptionCode ec = 0;
    Document& targetDocument = clone.document();
    for (Node* child = firstChild(); child && !ec; child = child->nextSibling()) {
        Ref<Node> clonedChild = child->cloneNodeInternal(targetDocument, CloningOperation::SelfWithTemplateContent);
        clone.appendChild(clonedChild.copyRef(), ec);

        if (!ec && is<ContainerNode>(*child))
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

    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<Node> c = &child;
    Ref<Document> document(child.document());

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, false));
    }
}

static void dispatchChildRemovalEvents(Node& child)
{
    if (child.isInShadowTree()) {
        InspectorInstrumentation::willRemoveDOMNode(child.document(), child);
        return;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());

    willCreatePossiblyOrphanedTreeByRemoval(&child);
    InspectorInstrumentation::willRemoveDOMNode(child.document(), child);

    RefPtr<Node> c = &child;
    Ref<Document> document(child.document());

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, true, c->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, false));
    }
}

void ContainerNode::updateTreeAfterInsertion(Node& child)
{
    ASSERT(child.refCount());

    notifyChildInserted(child, ChildChangeSourceAPI);

    child.setNeedsStyleRecalc(ReconstructRenderTree);

    dispatchChildInsertionEvents(child);
}

void ContainerNode::setAttributeEventListener(const AtomicString& eventType, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    setAttributeEventListener(eventType, JSLazyEventListener::createForNode(*this, attributeName, attributeValue));
}

Element* ContainerNode::querySelector(const String& selectors, ExceptionCode& ec)
{
    if (SelectorQuery* selectorQuery = document().selectorQueryForString(selectors, ec))
        return selectorQuery->queryFirst(*this);
    return nullptr;
}

RefPtr<NodeList> ContainerNode::querySelectorAll(const String& selectors, ExceptionCode& ec)
{
    if (SelectorQuery* selectorQuery = document().selectorQueryForString(selectors, ec))
        return selectorQuery->queryAll(*this);
    return nullptr;
}

Ref<HTMLCollection> ContainerNode::getElementsByTagName(const AtomicString& localName)
{
    ASSERT(!localName.isNull());

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTagCollection>(*this, ByHTMLTag, localName);
    return ensureRareData().ensureNodeLists().addCachedCollection<TagCollection>(*this, ByTag, localName);
}

RefPtr<NodeList> ContainerNode::getElementsByTagNameForObjC(const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    return getElementsByTagName(localName);
}

Ref<HTMLCollection> ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    ASSERT(!localName.isNull());

    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureRareData().ensureNodeLists().addCachedCollectionWithQualifiedName(*this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

RefPtr<NodeList> ContainerNode::getElementsByTagNameNSForObjC(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    return getElementsByTagNameNS(namespaceURI, localName);
}

Ref<NodeList> ContainerNode::getElementsByName(const String& elementName)
{
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<NameNodeList>(*this, elementName);
}

Ref<HTMLCollection> ContainerNode::getElementsByClassName(const AtomicString& classNames)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<ClassCollection>(*this, ByClass, classNames);
}

Ref<NodeList> ContainerNode::getElementsByClassNameForObjC(const AtomicString& classNames)
{
    return getElementsByClassName(classNames);
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

void ContainerNode::append(Vector<NodeOrString>&& nodeOrStringVector, ExceptionCode& ec)
{
    RefPtr<Node> node = convertNodesOrStringsIntoNode(*this, WTFMove(nodeOrStringVector), ec);
    if (ec || !node)
        return;

    appendChild(node.releaseNonNull(), ec);
}

void ContainerNode::prepend(Vector<NodeOrString>&& nodeOrStringVector, ExceptionCode& ec)
{
    RefPtr<Node> node = convertNodesOrStringsIntoNode(*this, WTFMove(nodeOrStringVector), ec);
    if (ec || !node)
        return;

    insertBefore(node.releaseNonNull(), firstChild(), ec);
}

HTMLCollection* ContainerNode::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cachedCollection<HTMLCollection>(type) : nullptr;
}

} // namespace WebCore
