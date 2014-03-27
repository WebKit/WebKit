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
#include "ClassNodeList.h"
#include "ContainerNodeAlgorithms.h"
#include "Editor.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "InsertionPoint.h"
#include "JSLazyEventListener.h"
#include "JSNode.h"
#include "LabelsNodeList.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NodeRareData.h"
#include "NodeRenderStyle.h"
#include "RadioNodeList.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "RenderWidget.h"
#include "ResourceLoadScheduler.h"
#include "RootInlineBox.h"
#include "SelectorQuery.h"
#include "TemplateContentDocumentFragment.h"
#include <wtf/CurrentTime.h>

#if ENABLE(DELETION_UI)
#include "DeleteButtonController.h"
#endif

namespace WebCore {

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Node&);

ChildNodesLazySnapshot* ChildNodesLazySnapshot::latestSnapshot = 0;

#ifndef NDEBUG
unsigned NoEventDispatchAssertion::s_count = 0;
#endif

static void collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes, ExceptionCode& ec)
{
    if (!node.isDocumentFragment()) {
        nodes.append(node);
        if (ContainerNode* oldParent = node.parentNode())
            oldParent->removeChild(&node, ec);
        return;
    }

    getChildNodes(node, nodes);
    toContainerNode(node).removeChildren();
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
    removeDetachedChildrenInContainer<Node, ContainerNode>(*this);
}

static inline void destroyRenderTreeIfNeeded(Node& child)
{
    // FIXME: Get rid of the named flow test.
    if (!child.renderer() && !child.isNamedFlowContentNode())
        return;
    if (child.isElementNode())
        Style::detachRenderTree(toElement(child));
    else if (child.isTextNode())
        Style::detachTextRenderer(toText(child));
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    ASSERT(oldParent);

    NodeVector children;
    getChildNodes(*oldParent, children);

    if (oldParent->document().hasMutationObserversOfType(MutationObserver::ChildList)) {
        ChildListMutationScope mutation(*oldParent);
        for (unsigned i = 0; i < children.size(); ++i)
            mutation.willRemoveChild(children[i].get());
    }

    // FIXME: We need to do notifyMutationObserversNodeWillDetach() for each child,
    // probably inside removeDetachedChildrenInContainer.

    oldParent->removeDetachedChildren();

    for (unsigned i = 0; i < children.size(); ++i) {
        Node& child = children[i].get();

        destroyRenderTreeIfNeeded(child);

        // FIXME: We need a no mutation event version of adoptNode.
        RefPtr<Node> adoptedChild = document().adoptNode(&children[i].get(), ASSERT_NO_EXCEPTION);
        parserAppendChild(adoptedChild.get());
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

static inline bool isChildTypeAllowed(ContainerNode* newParent, Node* child)
{
    if (!child->isDocumentFragment())
        return newParent->childTypeAllowed(child->nodeType());

    for (Node* node = child->firstChild(); node; node = node->nextSibling()) {
        if (!newParent->childTypeAllowed(node->nodeType()))
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

static inline bool containsConsideringHostElements(const Node* newChild, const Node* newParent)
{
    return (newParent->isInShadowTree() || isInTemplateContent(newParent))
        ? newChild->containsIncludingHostElements(newParent)
        : newChild->contains(newParent);
}

static inline ExceptionCode checkAcceptChild(ContainerNode* newParent, Node* newChild, Node* oldChild)
{
    // Not mentioned in spec: throw NOT_FOUND_ERR if newChild is null
    if (!newChild)
        return NOT_FOUND_ERR;

    // Use common case fast path if possible.
    if ((newChild->isElementNode() || newChild->isTextNode()) && newParent->isElementNode()) {
        ASSERT(!newParent->isReadOnlyNode());
        ASSERT(!newParent->isDocumentTypeNode());
        ASSERT(isChildTypeAllowed(newParent, newChild));
        if (containsConsideringHostElements(newChild, newParent))
            return HIERARCHY_REQUEST_ERR;
        return 0;
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild->isPseudoElement());
    if (newChild->isPseudoElement())
        return HIERARCHY_REQUEST_ERR;

    if (newParent->isReadOnlyNode())
        return NO_MODIFICATION_ALLOWED_ERR;
    if (containsConsideringHostElements(newChild, newParent))
        return HIERARCHY_REQUEST_ERR;

    if (oldChild && newParent->isDocumentNode()) {
        if (!toDocument(newParent)->canReplaceChild(newChild, oldChild))
            return HIERARCHY_REQUEST_ERR;
    } else if (!isChildTypeAllowed(newParent, newChild))
        return HIERARCHY_REQUEST_ERR;

    return 0;
}

static inline bool checkAcceptChildGuaranteedNodeTypes(ContainerNode* newParent, Node* newChild, ExceptionCode& ec)
{
    ASSERT(!newParent->isReadOnlyNode());
    ASSERT(!newParent->isDocumentTypeNode());
    ASSERT(isChildTypeAllowed(newParent, newChild));
    if (newChild->contains(newParent)) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }

    return true;
}

static inline bool checkAddChild(ContainerNode* newParent, Node* newChild, ExceptionCode& ec)
{
    ec = checkAcceptChild(newParent, newChild, 0);
    if (ec)
        return false;

    return true;
}

static inline bool checkReplaceChild(ContainerNode* newParent, Node* newChild, Node* oldChild, ExceptionCode& ec)
{
    ec = checkAcceptChild(newParent, newChild, oldChild);
    if (ec)
        return false;

    return true;
}

bool ContainerNode::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild)
        return appendChild(newChild, ec);

    // Make sure adding the new child is OK.
    if (!checkAddChild(this, newChild.get(), ec))
        return false;

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    if (refChild->previousSibling() == newChild || refChild == newChild) // nothing to do
        return true;

    Ref<Node> next(*refChild);

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild.get(), targets, ec);
    if (ec)
        return false;
    if (targets.isEmpty())
        return true;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(this, newChild.get(), ec))
        return false;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    ChildListMutationScope mutation(*this);
    for (auto it = targets.begin(), end = targets.end(); it != end; ++it) {
        Node& child = it->get();

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(&child);

        insertBeforeCommon(next.get(), child);

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

void ContainerNode::notifyChildInserted(Node& child, ChildChangeSource source)
{
    ChildChange change;
    change.type = child.isElementNode() ? ElementInserted : child.isTextNode() ? TextInserted : NonContentsChildChanged;
    change.previousSiblingElement = ElementTraversal::previousSibling(&child);
    change.nextSiblingElement = ElementTraversal::nextSibling(&child);
    change.source = source;

    childrenChanged(change);
}

void ContainerNode::notifyChildRemoved(Node& child, Node* previousSibling, Node* nextSibling, ChildChangeSource source)
{
    ChildChange change;
    change.type = child.isElementNode() ? ElementRemoved : child.isTextNode() ? TextRemoved : NonContentsChildChanged;
    change.previousSiblingElement = (!previousSibling || previousSibling->isElementNode()) ? toElement(previousSibling) : ElementTraversal::previousSibling(previousSibling);
    change.nextSiblingElement = (!nextSibling || nextSibling->isElementNode()) ? toElement(nextSibling) : ElementTraversal::nextSibling(nextSibling);
    change.source = source;

    childrenChanged(change);
}

void ContainerNode::parserInsertBefore(PassRefPtr<Node> newChild, Node* nextChild)
{
    ASSERT(newChild);
    ASSERT(nextChild);
    ASSERT(nextChild->parentNode() == this);
    ASSERT(!newChild->isDocumentFragment());
#if ENABLE(TEMPLATE_ELEMENT)
    ASSERT(!hasTagName(HTMLNames::templateTag));
#endif

    if (nextChild->previousSibling() == newChild || nextChild == newChild) // nothing to do
        return;

    if (&document() != &newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    insertBeforeCommon(*nextChild, *newChild.get());

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(*this).childAdded(*newChild);

    notifyChildInserted(*newChild, ChildChangeSourceParser);

    ChildNodeInsertionNotifier(*this).notify(*newChild);

    newChild->setNeedsStyleRecalc(ReconstructRenderTree);
}

bool ContainerNode::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    if (oldChild == newChild) // nothing to do
        return true;

    if (!oldChild) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    // Make sure replacing the old child with the new is ok
    if (!checkReplaceChild(this, newChild.get(), oldChild, ec))
        return false;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (oldChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    ChildListMutationScope mutation(*this);

    RefPtr<Node> next = oldChild->nextSibling();

    // Remove the node we're replacing
    Ref<Node> removedChild(*oldChild);
    removeChild(oldChild, ec);
    if (ec)
        return false;

    if (next && (next->previousSibling() == newChild || next == newChild)) // nothing to do
        return true;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkReplaceChild(this, newChild.get(), oldChild, ec))
        return false;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild.get(), targets, ec);
    if (ec)
        return false;

    // Does this yet another check because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkReplaceChild(this, newChild.get(), oldChild, ec))
        return false;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    // Add the new child(ren)
    for (auto it = targets.begin(), end = targets.end(); it != end; ++it) {
        Node& child = it->get();

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next && next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(&child);

        // Add child before "next".
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            if (next)
                insertBeforeCommon(*next, child);
            else
                appendChildToContainer(&child, *this);
        }

        updateTreeAfterInsertion(child);
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

    child.document().nodeWillBeRemoved(&child); // e.g. mutation event listener can create a new range.
    if (child.isContainerNode())
        disconnectSubframesIfNeeded(toContainerNode(child), RootAndDescendants);
}

static void willRemoveChildren(ContainerNode& container)
{
    NodeVector children;
    getChildNodes(container, children);

    ChildListMutationScope mutation(container);
    for (auto it = children.begin(); it != children.end(); ++it) {
        Node& child = it->get();
        mutation.willRemoveChild(child);
        child.notifyMutationObserversNodeWillDetach();

        // fire removed from document mutation events.
        dispatchChildRemovalEvents(child);
    }

    container.document().nodeChildrenWillBeRemoved(container);

    disconnectSubframesIfNeeded(container, DescendantsOnly);
}

void ContainerNode::disconnectDescendantFrames()
{
    disconnectSubframesIfNeeded(*this, RootAndDescendants);
}

bool ContainerNode::removeChild(Node* oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    Ref<ContainerNode> protect(*this);

    ec = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return false;
    }

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    Ref<Node> child(*oldChild);

    document().removeFocusedNodeOfSubtree(&child.get());

#if ENABLE(FULLSCREEN_API)
    document().removeFullScreenElementOfSubtree(&child.get());
#endif

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    willRemoveChild(child.get());

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, child.get());

        notifyChildRemoved(child.get(), prev, next, ChildChangeSourceAPI);

        ChildNodeRemovalNotifier(*this).notify(child.get());
    }
    dispatchSubtreeModifiedEvent();

    return true;
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    InspectorInstrumentation::didRemoveDOMNode(&oldChild.document(), &oldChild);

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

    oldChild.setPreviousSibling(0);
    oldChild.setNextSibling(0);
    oldChild.setParentNode(0);

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

    ChildNodeRemovalNotifier(*this).notify(oldChild);
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

    NodeVector removedChildren;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            removedChildren.reserveInitialCapacity(childNodeCount());
            while (RefPtr<Node> n = m_firstChild) {
                removedChildren.append(*m_firstChild);
                removeBetween(0, m_firstChild->nextSibling(), *m_firstChild);
            }
        }

        ChildChange change = { AllChildrenRemoved, nullptr, nullptr, ChildChangeSourceAPI };
        childrenChanged(change);
        
        for (size_t i = 0; i < removedChildren.size(); ++i)
            ChildNodeRemovalNotifier(*this).notify(removedChildren[i].get());
    }

    dispatchSubtreeModifiedEvent();
}

bool ContainerNode::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec)
{
    Ref<ContainerNode> protect(*this);

    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());

    ec = 0;

    // Make sure adding the new child is ok
    if (!checkAddChild(this, newChild.get(), ec))
        return false;

    if (newChild == m_lastChild) // nothing to do
        return newChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild.get(), targets, ec);
    if (ec)
        return false;

    if (targets.isEmpty())
        return true;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(this, newChild.get(), ec))
        return false;

    InspectorInstrumentation::willInsertDOMNode(&document(), this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (auto it = targets.begin(), end = targets.end(); it != end; ++it) {
        Node& child = it->get();

        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(&child);

        // Append child to the end of the list
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            appendChildToContainer(&child, *this);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::parserAppendChild(PassRefPtr<Node> newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->isDocumentFragment());
#if ENABLE(TEMPLATE_ELEMENT)
    ASSERT(!hasTagName(HTMLNames::templateTag));
#endif

    if (&document() != &newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    {
        NoEventDispatchAssertion assertNoEventDispatch;
        // FIXME: This method should take a PassRefPtr.
        appendChildToContainer(newChild.get(), *this);
        treeScope().adoptIfNeeded(newChild.get());
    }

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(*this).childAdded(*newChild);

    notifyChildInserted(*newChild, ChildChangeSourceParser);

    ChildNodeInsertionNotifier(*this).notify(*newChild);

    newChild->setNeedsStyleRecalc(ReconstructRenderTree);
}

void ContainerNode::childrenChanged(const ChildChange& change)
{
    document().incDOMTreeVersion();
    if (change.source == ChildChangeSourceAPI && change.type != TextChanged)
        document().updateRangesAfterChildrenChanged(*this);
    invalidateNodeListAndCollectionCachesInAncestors();
}

inline static void cloneChildNodesAvoidingDeleteButton(ContainerNode* parent, ContainerNode* clonedParent, HTMLElement* deleteButtonContainerElement)
{
    ExceptionCode ec = 0;
    for (Node* child = parent->firstChild(); child && !ec; child = child->nextSibling()) {

#if ENABLE(DELETION_UI)
        if (child == deleteButtonContainerElement)
            continue;
#else
        UNUSED_PARAM(deleteButtonContainerElement);
#endif

        RefPtr<Node> clonedChild = child->cloneNode(false);
        clonedParent->appendChild(clonedChild, ec);

        if (!ec && child->isContainerNode())
            cloneChildNodesAvoidingDeleteButton(toContainerNode(child), toContainerNode(clonedChild.get()), deleteButtonContainerElement);
    }
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
#if ENABLE(DELETION_UI)
    HTMLElement* deleteButtonContainerElement = 0;
    if (Frame* frame = document().frame())
        deleteButtonContainerElement = frame->editor().deleteButtonController().containerElement();
    cloneChildNodesAvoidingDeleteButton(this, clone, deleteButtonContainerElement);
#else
    cloneChildNodesAvoidingDeleteButton(this, clone, 0);
#endif
}

bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject* o = renderer();
    RenderObject* p = o;

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute(FloatPoint(), UseTransforms);
        return true;
    }

    // find the next text/image child, to get a position
    while (o) {
        p = o;
        if (RenderObject* child = o->firstChildSlow())
            o = child;
        else if (o->nextSibling())
            o = o->nextSibling();
        else {
            RenderObject* next = 0;
            while (!next && o->parent()) {
                o = o->parent();
                next = o->nextSibling();
            }
            o = next;

            if (!o)
                break;
        }
        ASSERT(o);

        if (!o->isInline() || o->isReplaced()) {
            point = o->localToAbsolute(FloatPoint(), UseTransforms);
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !toRenderText(o)->firstTextBox()) {
            // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if (o->isText() || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText() && toRenderText(o)->firstTextBox()) {
                point.move(toRenderText(o)->linesBoundingBox().x(), toRenderText(o)->firstTextBox()->root().lineTop());
            } else if (o->isBox()) {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->location());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }
    
    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document. Scroll to the bottom. FIXME: who said anything about scrolling?
    if (!o && document().view()) {
        point = FloatPoint(0, document().view()->contentsHeight());
        return true;
    }
    return false;
}

bool ContainerNode::getLowerRightCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;

    RenderObject* o = renderer();
    if (!o->isInline() || o->isReplaced()) {
        RenderBox* box = toRenderBox(o);
        point = o->localToAbsolute(LayoutPoint(box->size()), UseTransforms);
        return true;
    }

    // find the last text/image child, to get a position
    while (o) {
        if (RenderObject* child = o->lastChildSlow())
            o = child;
        else if (o->previousSibling())
            o = o->previousSibling();
        else {
            RenderObject* prev = 0;
            while (!prev) {
                o = o->parent();
                if (!o)
                    return false;
                prev = o->previousSibling();
            }
            o = prev;
        }
        ASSERT(o);
        if (o->isText() || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText()) {
                RenderText* text = toRenderText(o);
                IntRect linesBox = text->linesBoundingBox();
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
            } else {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->frameRect().maxXMaxYCorner());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }
    return true;
}

LayoutRect ContainerNode::boundingBox() const
{
    FloatPoint upperLeft, lowerRight;
    bool foundUpperLeft = getUpperLeftCorner(upperLeft);
    bool foundLowerRight = getLowerRightCorner(lowerRight);
    
    // If we've found one corner, but not the other,
    // then we should just return a point at the corner that we did find.
    if (foundUpperLeft != foundLowerRight) {
        if (foundUpperLeft)
            lowerRight = upperLeft;
        else
            upperLeft = lowerRight;
    } 

    return enclosingLayoutRect(FloatRect(upperLeft, lowerRight.expandedTo(upperLeft) - upperLeft));
}

unsigned ContainerNode::childNodeCount() const
{
    unsigned count = 0;
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

Node *ContainerNode::childNode(unsigned index) const
{
    unsigned i;
    Node *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

static void dispatchChildInsertionEvents(Node& child)
{
    if (child.isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtr<Node> c = &child;
    Ref<Document> document(child.document());

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(c.get(), &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, false));
    }
}

static void dispatchChildRemovalEvents(Node& child)
{
    if (child.isInShadowTree()) {
        InspectorInstrumentation::willRemoveDOMNode(&child.document(), &child);
        return;
    }

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    willCreatePossiblyOrphanedTreeByRemoval(&child);
    InspectorInstrumentation::willRemoveDOMNode(&child.document(), &child);

    RefPtr<Node> c = &child;
    Ref<Document> document(child.document());

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, true, c->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(c.get(), &child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, false));
    }
}

void ContainerNode::updateTreeAfterInsertion(Node& child)
{
    ASSERT(child.refCount());

    ChildListMutationScope(*this).childAdded(child);

    notifyChildInserted(child, ChildChangeSourceAPI);

    ChildNodeInsertionNotifier(*this).notify(child);

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

PassRefPtr<NodeList> ContainerNode::getElementsByTagName(const AtomicString& localName)
{
    if (localName.isNull())
        return 0;

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCacheWithAtomicName<HTMLTagNodeList>(*this, LiveNodeList::Type::HTMLTagNodeListType, localName);
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<TagNodeList>(*this, LiveNodeList::Type::TagNodeListType, localName);
}

PassRefPtr<NodeList> ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (localName.isNull())
        return 0;

    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureRareData().ensureNodeLists().addCacheWithQualifiedName(*this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

PassRefPtr<NodeList> ContainerNode::getElementsByName(const String& elementName)
{
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<NameNodeList>(*this, LiveNodeList::Type::NameNodeListType, elementName);
}

PassRefPtr<NodeList> ContainerNode::getElementsByClassName(const String& classNames)
{
    return ensureRareData().ensureNodeLists().addCacheWithName<ClassNodeList>(*this, LiveNodeList::Type::ClassNodeListType, classNames);
}

PassRefPtr<RadioNodeList> ContainerNode::radioNodeList(const AtomicString& name)
{
    ASSERT(hasTagName(HTMLNames::formTag) || hasTagName(HTMLNames::fieldsetTag));
    return ensureRareData().ensureNodeLists().addCacheWithAtomicName<RadioNodeList>(*this, LiveNodeList::Type::RadioNodeListType, name);
}

} // namespace WebCore
