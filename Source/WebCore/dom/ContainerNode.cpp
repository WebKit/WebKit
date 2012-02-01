/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "ChildListMutationScope.h"
#include "ContainerNodeAlgorithms.h"
#include "DeleteButtonController.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "InspectorInstrumentation.h"
#include "MemoryCache.h"
#include "MutationEvent.h"
#include "ResourceLoadScheduler.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "RootInlineBox.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

static void notifyChildInserted(Node*);
static void dispatchChildInsertionEvents(Node*);
static void dispatchChildRemovalEvents(Node*);

typedef pair<RefPtr<Node>, unsigned> CallbackParameters;
typedef pair<NodeCallback, CallbackParameters> CallbackInfo;
typedef Vector<CallbackInfo> NodeCallbackQueue;

typedef Vector<RefPtr<Node>, 1> NodeVector;
static NodeCallbackQueue* s_postAttachCallbackQueue;

static size_t s_attachDepth;
static bool s_shouldReEnableMemoryCacheCallsAfterAttach;

static inline void collectNodes(Node* node, NodeVector& nodes)
{
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        nodes.append(child);
}

static void collectTargetNodes(Node* node, NodeVector& nodes)
{
    if (node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE) {
        nodes.append(node);
        return;
    }
    collectNodes(node, nodes);
}

void ContainerNode::removeAllChildren()
{
    removeAllChildrenInContainer<Node, ContainerNode>(this);
}

void ContainerNode::takeAllChildrenFrom(ContainerNode* oldParent)
{
    NodeVector children;
    collectNodes(oldParent, children);
    oldParent->removeAllChildren();

    for (unsigned i = 0; i < children.size(); ++i) {
        ExceptionCode ec = 0;
        if (children[i]->attached())
            children[i]->detach();
        // FIXME: We need a no mutation event version of adoptNode.
        RefPtr<Node> child = document()->adoptNode(children[i].release(), ec);
        ASSERT(!ec);
        parserAddChild(child.get());
        // FIXME: Together with adoptNode above, the tree scope might get updated recursively twice
        // (if the document changed or oldParent was in a shadow tree, AND *this is in a shadow tree).
        // Can we do better?
        treeScope()->adoptIfNeeded(child.get());
        if (attached() && !child->attached())
            child->attach();
    }
}

ContainerNode::~ContainerNode()
{
    removeAllChildren();
}

bool ContainerNode::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrHostNode());

    ec = 0;

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild)
        return appendChild(newChild, ec, shouldLazyAttach);

    // Make sure adding the new child is OK.
    checkAddChild(newChild.get(), ec);
    if (ec)
        return false;

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    NodeVector targets;
    collectTargetNodes(newChild.get(), targets);
    if (targets.isEmpty())
        return true;

    // Now actually add the child(ren)
    if (refChild->previousSibling() == newChild || refChild == newChild) // nothing to do
        return true;

#if ENABLE(MUTATION_OBSERVERS)
    ChildListMutationScope mutation(this);
#endif

    RefPtr<Node> next = refChild;
    RefPtr<Node> refChildPreviousSibling = refChild->previousSibling();
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

        // If child is already present in the tree, first remove it from the old location.
        if (ContainerNode* oldParent = child->parentNode())
            oldParent->removeChild(child, ec);
        if (ec)
            return false;

        // FIXME: After sending the mutation events, "this" could be destroyed.
        // We can prevent that by doing a "ref", but first we have to make sure
        // that no callers call with ref count == 0 and parent = 0 (as of this
        // writing, there are definitely callers who call that way).

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child->parentNode())
            break;

#if ENABLE(INSPECTOR)
        InspectorInstrumentation::willInsertDOMNode(document(), child, this);
#endif

        treeScope()->adoptIfNeeded(child);

        insertBeforeCommon(next.get(), child);

        // Send notification about the children change.
        childrenChanged(false, refChildPreviousSibling.get(), next.get(), 1);
        notifyChildInserted(child);

        // Add child to the rendering tree.
        if (attached() && !child->attached() && child->parentNode() == this) {
            if (shouldLazyAttach)
                child->lazyAttach();
            else
                child->attach();
        }

        // Now that the child is attached to the render tree, dispatch
        // the relevant mutation events.
        dispatchChildInsertionEvents(child);
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::insertBeforeCommon(Node* nextChild, Node* newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->nextSibling());
    ASSERT(!newChild->previousSibling());

    forbidEventDispatch();
    Node* prev = nextChild->previousSibling();
    ASSERT(m_lastChild != prev);
    nextChild->setPreviousSibling(newChild);
    if (prev) {
        ASSERT(m_firstChild != nextChild);
        ASSERT(prev->nextSibling() == nextChild);
        prev->setNextSibling(newChild);
    } else {
        ASSERT(m_firstChild == nextChild);
        m_firstChild = newChild;
    }
    newChild->setParent(this);
    newChild->setPreviousSibling(prev);
    newChild->setNextSibling(nextChild);
    allowEventDispatch();
}

void ContainerNode::parserInsertBefore(PassRefPtr<Node> newChild, Node* nextChild)
{
    ASSERT(newChild);
    ASSERT(nextChild);
    ASSERT(nextChild->parentNode() == this);

    NodeVector targets;
    collectTargetNodes(newChild.get(), targets);
    if (targets.isEmpty())
        return;

    if (nextChild->previousSibling() == newChild || nextChild == newChild) // nothing to do
        return;

    RefPtr<Node> next = nextChild;
    RefPtr<Node> nextChildPreviousSibling = nextChild->previousSibling();
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

#if ENABLE(INSPECTOR)
        InspectorInstrumentation::willInsertDOMNode(document(), child, this);
#endif

        insertBeforeCommon(next.get(), child);

        childrenChanged(true, nextChildPreviousSibling.get(), nextChild, 1);
        notifyChildInserted(child);
    }
}

bool ContainerNode::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrHostNode());

    ec = 0;

    if (oldChild == newChild) // nothing to do
        return true;
    
    // Make sure replacing the old child with the new is ok
    checkReplaceChild(newChild.get(), oldChild, ec);
    if (ec)
        return false;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

#if ENABLE(MUTATION_OBSERVERS)
    ChildListMutationScope mutation(this);
#endif

    RefPtr<Node> prev = oldChild->previousSibling();
    RefPtr<Node> next = oldChild->nextSibling();

    // Remove the node we're replacing
    RefPtr<Node> removedChild = oldChild;
    removeChild(oldChild, ec);
    if (ec)
        return false;

    // FIXME: After sending the mutation events, "this" could be destroyed.
    // We can prevent that by doing a "ref", but first we have to make sure
    // that no callers call with ref count == 0 and parent = 0 (as of this
    // writing, there are definitely callers who call that way).

    bool isFragment = newChild->nodeType() == DOCUMENT_FRAGMENT_NODE;

    // Add the new child(ren)
    RefPtr<Node> child = isFragment ? newChild->firstChild() : newChild;
    while (child) {
        // If the new child is already in the right place, we're done.
        if (prev && (prev == child || prev == child->previousSibling()))
            break;

        // For a fragment we have more children to do.
        RefPtr<Node> nextChild = isFragment ? child->nextSibling() : 0;

        // Remove child from its old position.
        if (ContainerNode* oldParent = child->parentNode())
            oldParent->removeChild(child.get(), ec);
        if (ec)
            return false;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "prev" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (prev && prev->parentNode() != this)
            break;
        if (child->parentNode())
            break;

        ASSERT(!child->nextSibling());
        ASSERT(!child->previousSibling());

#if ENABLE(INSPECTOR)
        InspectorInstrumentation::willInsertDOMNode(document(), child.get(), this);
#endif

        treeScope()->adoptIfNeeded(child.get());

        // Add child after "prev".
        forbidEventDispatch();
        Node* next;
        if (prev) {
            next = prev->nextSibling();
            ASSERT(m_firstChild != next);
            prev->setNextSibling(child.get());
        } else {
            next = m_firstChild;
            m_firstChild = child.get();
        }
        if (next) {
            ASSERT(m_lastChild != prev);
            ASSERT(next->previousSibling() == prev);
            next->setPreviousSibling(child.get());
        } else {
            ASSERT(m_lastChild == prev);
            m_lastChild = child.get();
        }
        child->setParent(this);
        child->setPreviousSibling(prev.get());
        child->setNextSibling(next);
        allowEventDispatch();

        childrenChanged(false, prev.get(), next, 1);
        notifyChildInserted(child.get());
                
        // Add child to the rendering tree
        if (attached() && !child->attached() && child->parentNode() == this) {
            if (shouldLazyAttach)
                child->lazyAttach();
            else
                child->attach();
        }

        // Now that the child is attached to the render tree, dispatch
        // the relevant mutation events.
        dispatchChildInsertionEvents(child.get());

        prev = child;
        child = nextChild.release();
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::willRemove()
{
    RefPtr<Node> protect(this);

    for (RefPtr<Node> child = firstChild(); child; child = child->nextSibling()) {
        if (child->parentNode() != this) // Check for child being removed from subtree while removing.
            break;
        child->willRemove();
    }
    Node::willRemove();
}

static void willRemoveChild(Node* child)
{
    // update auxiliary doc info (e.g. iterators) to note that node is being removed
    child->document()->nodeWillBeRemoved(child);
    child->document()->incDOMTreeVersion();

    // fire removed from document mutation events.
    dispatchChildRemovalEvents(child);
    child->willRemove();
}

static void willRemoveChildren(ContainerNode* container)
{
    container->document()->nodeChildrenWillBeRemoved(container);
    container->document()->incDOMTreeVersion();

    NodeVector children;
    collectNodes(container, children);

#if ENABLE(MUTATION_OBSERVERS)
    ChildListMutationScope mutation(container);
#endif

    for (NodeVector::const_iterator it = children.begin(); it != children.end(); it++) {
        Node* child = it->get();
        // fire removed from document mutation events.
        dispatchChildRemovalEvents(child);
        child->willRemove();
    }
}

bool ContainerNode::removeChild(Node* oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrHostNode());

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

    RefPtr<Node> child = oldChild;
    willRemoveChild(child.get());

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    document()->removeFocusedNodeOfSubtree(child.get());

#if ENABLE(FULLSCREEN_API)
    document()->removeFullScreenElementOfSubtree(child.get());
#endif


    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    // FIXME: After sending the mutation events, "this" could be destroyed.
    // We can prevent that by doing a "ref", but first we have to make sure
    // that no callers call with ref count == 0 and parent = 0 (as of this
    // writing, there are definitely callers who call that way).

    Node* prev = child->previousSibling();
    Node* next = child->nextSibling();
    removeBetween(prev, next, child.get());

    // Dispatch post-removal mutation events
    childrenChanged(false, prev, next, -1);
    dispatchSubtreeModifiedEvent();

    if (child->inDocument())
        child->removedFromDocument();
    else
        child->removedFromTree(true);

    return child;
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node* oldChild)
{
    ASSERT(oldChild);
    ASSERT(oldChild->parentNode() == this);

    forbidEventDispatch();

    // Remove from rendering tree
    if (oldChild->attached())
        oldChild->detach();

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == oldChild)
        m_lastChild = previousChild;

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    document()->adoptIfNeeded(oldChild);

    allowEventDispatch();
}

void ContainerNode::parserRemoveChild(Node* oldChild)
{
    ASSERT(oldChild);
    ASSERT(oldChild->parentNode() == this);

    Node* prev = oldChild->previousSibling();
    Node* next = oldChild->nextSibling();

    removeBetween(prev, next, oldChild);

    childrenChanged(true, prev, next, -1);
    if (oldChild->inDocument())
        oldChild->removedFromDocument();
    else
        oldChild->removedFromTree(true);
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    // The container node can be removed from event handlers.
    RefPtr<ContainerNode> protect(this);

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren(protect.get());

    // exclude this node when looking for removed focusedNode since only children will be removed
    document()->removeFocusedNodeOfSubtree(this, true);

#if ENABLE(FULLSCREEN_API)
    document()->removeFullScreenElementOfSubtree(this, true);
#endif

    forbidEventDispatch();
    Vector<RefPtr<Node>, 10> removedChildren;
    removedChildren.reserveInitialCapacity(childNodeCount());
    while (RefPtr<Node> n = m_firstChild) {
        Node* next = n->nextSibling();

        // Remove the node from the tree before calling detach or removedFromDocument (4427024, 4129744).
        // removeChild() does this after calling detach(). There is no explanation for
        // this discrepancy between removeChild() and its optimized version removeChildren().
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        document()->adoptIfNeeded(n.get());

        m_firstChild = next;
        if (n == m_lastChild)
            m_lastChild = 0;
        removedChildren.append(n.release());
    }

    size_t removedChildrenCount = removedChildren.size();
    size_t i;

    // Detach the nodes only after properly removed from the tree because
    // a. detaching requires a proper DOM tree (for counters and quotes for
    // example) and during the previous loop the next sibling still points to
    // the node being removed while the node being removed does not point back
    // and does not point to the same parent as its next sibling.
    // b. destroying Renderers of standalone nodes is sometimes faster.
    for (i = 0; i < removedChildrenCount; ++i) {
        Node* removedChild = removedChildren[i].get();
        if (removedChild->attached())
            removedChild->detach();
    }

    allowEventDispatch();

    // Dispatch a single post-removal mutation event denoting a modified subtree.
    childrenChanged(false, 0, 0, -static_cast<int>(removedChildrenCount));
    dispatchSubtreeModifiedEvent();

    for (i = 0; i < removedChildrenCount; ++i) {
        Node* removedChild = removedChildren[i].get();
        if (removedChild->inDocument())
            removedChild->removedFromDocument();
        // removeChild() calls removedFromTree(true) if the child was not in the
        // document. There is no explanation for this discrepancy between removeChild()
        // and its optimized version removeChildren().
    }
}

bool ContainerNode::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrHostNode());

    ec = 0;

    // Make sure adding the new child is ok
    checkAddChild(newChild.get(), ec);
    if (ec)
        return false;

    if (newChild == m_lastChild) // nothing to do
        return newChild;

    NodeVector targets;
    collectTargetNodes(newChild.get(), targets);
    if (targets.isEmpty())
        return true;

#if ENABLE(MUTATION_OBSERVERS)
    ChildListMutationScope mutation(this);
#endif

    // Now actually add the child(ren)
    RefPtr<Node> prev = lastChild();
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();
        // If child is already present in the tree, first remove it
        if (ContainerNode* oldParent = child->parentNode()) {
            oldParent->removeChild(child, ec);
            if (ec)
                return false;

            // If the child has a parent again, just stop what we're doing, because
            // that means someone is doing something with DOM mutation -- can't re-parent
            // a child that already has a parent.
            if (child->parentNode())
                break;
        }

#if ENABLE(INSPECTOR)
        InspectorInstrumentation::willInsertDOMNode(document(), child, this);
#endif

        treeScope()->adoptIfNeeded(child);

        // Append child to the end of the list
        forbidEventDispatch();
        child->setParent(this);
        if (m_lastChild) {
            child->setPreviousSibling(m_lastChild);
            m_lastChild->setNextSibling(child);
        } else
            m_firstChild = child;
        m_lastChild = child;
        allowEventDispatch();

        // Send notification about the children change.
        childrenChanged(false, prev.get(), 0, 1);
        notifyChildInserted(child);

        // Add child to the rendering tree
        if (attached() && !child->attached() && child->parentNode() == this) {
            if (shouldLazyAttach)
                child->lazyAttach();
            else
                child->attach();
        }

        // Now that the child is attached to the render tree, dispatch
        // the relevant mutation events.
        dispatchChildInsertionEvents(child);
        prev = child;
    }

    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::parserAddChild(PassRefPtr<Node> newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::willInsertDOMNode(document(), newChild.get(), this);
#endif

    forbidEventDispatch();
    Node* last = m_lastChild;
    // FIXME: This method should take a PassRefPtr.
    appendChildToContainer<Node, ContainerNode>(newChild.get(), this);
    treeScope()->adoptIfNeeded(newChild.get());
    
    allowEventDispatch();

    // FIXME: Why doesn't this use notifyChildInserted(newChild) instead?
    document()->incDOMTreeVersion();
    if (inDocument())
        newChild->insertedIntoDocument();
    childrenChanged(true, last, 0, 1);
}

void ContainerNode::suspendPostAttachCallbacks()
{
    if (!s_attachDepth) {
        ASSERT(!s_shouldReEnableMemoryCacheCallsAfterAttach);
        if (Page* page = document()->page()) {
            // FIXME: How can this call be specific to one Page, while the
            // s_attachDepth is a global? Doesn't make sense.
            if (page->areMemoryCacheClientCallsEnabled()) {
                page->setMemoryCacheClientCallsEnabled(false);
                s_shouldReEnableMemoryCacheCallsAfterAttach = true;
            }
        }
        resourceLoadScheduler()->suspendPendingRequests();
    }
    ++s_attachDepth;
}

void ContainerNode::resumePostAttachCallbacks()
{
    if (s_attachDepth == 1) {
        if (s_postAttachCallbackQueue)
            dispatchPostAttachCallbacks();
        if (s_shouldReEnableMemoryCacheCallsAfterAttach) {
            s_shouldReEnableMemoryCacheCallsAfterAttach = false;
            if (Page* page = document()->page())
                page->setMemoryCacheClientCallsEnabled(true);
        }
        resourceLoadScheduler()->resumePendingRequests();
    }
    --s_attachDepth;
}

void ContainerNode::queuePostAttachCallback(NodeCallback callback, Node* node, unsigned callbackData)
{
    if (!s_postAttachCallbackQueue)
        s_postAttachCallbackQueue = new NodeCallbackQueue;
    
    s_postAttachCallbackQueue->append(CallbackInfo(callback, CallbackParameters(node, callbackData)));
}

bool ContainerNode::postAttachCallbacksAreSuspended()
{
    return s_attachDepth;
}

void ContainerNode::dispatchPostAttachCallbacks()
{
    // We recalculate size() each time through the loop because a callback
    // can add more callbacks to the end of the queue.
    for (size_t i = 0; i < s_postAttachCallbackQueue->size(); ++i) {
        const CallbackInfo& info = (*s_postAttachCallbackQueue)[i];
        NodeCallback callback = info.first;
        CallbackParameters params = info.second;

        callback(params.first.get(), params.second);
    }
    s_postAttachCallbackQueue->clear();
}

static void needsStyleRecalcCallback(Node* node, unsigned data)
{
    node->setNeedsStyleRecalc(static_cast<StyleChangeType>(data));
}

void ContainerNode::scheduleSetNeedsStyleRecalc(StyleChangeType changeType)
{
    if (postAttachCallbacksAreSuspended())
        queuePostAttachCallback(needsStyleRecalcCallback, this, static_cast<unsigned>(changeType));
    else
        setNeedsStyleRecalc(changeType);
}

void ContainerNode::attach()
{
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->attach();
    Node::attach();
}

void ContainerNode::detach()
{
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->detach();
    clearChildNeedsStyleRecalc();
    Node::detach();
}

void ContainerNode::insertedIntoDocument()
{
    RefPtr<Node> protect(this);

    Node::insertedIntoDocument();
    insertedIntoTree(false);

    for (RefPtr<Node> child = m_firstChild; child; child = child->nextSibling()) {
        // Guard against mutation during re-parenting.
        if (!inDocument()) // Check for self being removed from document while reparenting.
            break;
        if (child->parentNode() != this) // Check for child being removed from subtree while reparenting.
            break;
        child->insertedIntoDocument();
    }
}

void ContainerNode::removedFromDocument()
{
    Node::removedFromDocument();
    if (document()->cssTarget() == this) 
        document()->setCSSTarget(0); 
    clearInDocument();
    removedFromTree(false);
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->removedFromDocument();
}

void ContainerNode::insertedIntoTree(bool deep)
{
    if (!deep)
        return;
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->insertedIntoTree(true);
}

void ContainerNode::removedFromTree(bool deep)
{
    if (!deep)
        return;
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->removedFromTree(true);
}

void ContainerNode::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    Node::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (!changedByParser && childCountDelta)
        document()->updateRangesAfterChildrenChanged(this);
    invalidateNodeListsCacheAfterChildrenChanged();
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
    // disable the delete button so it's elements are not serialized into the markup
    bool isEditorEnabled = false;
    if (document()->frame() && document()->frame()->editor()->canEdit()) {
        FrameSelection* selection = document()->frame()->selection();
        Element* root = selection ? selection->rootEditableElement() : 0;
        isEditorEnabled = root && isDescendantOf(root);

        if (isEditorEnabled)
            document()->frame()->editor()->deleteButtonController()->disable();
    }
    
    ExceptionCode ec = 0;
    for (Node* n = firstChild(); n && !ec; n = n->nextSibling())
        clone->appendChild(n->cloneNode(true), ec);
    if (isEditorEnabled && document()->frame())
        document()->frame()->editor()->deleteButtonController()->enable();
}

bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject *o = renderer();
    RenderObject *p = o;

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute(FloatPoint(), false, true);
        return true;
    }

    // find the next text/image child, to get a position
    while (o) {
        p = o;
        if (o->firstChild())
            o = o->firstChild();
        else if (o->nextSibling())
            o = o->nextSibling();
        else {
            RenderObject *next = 0;
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
            point = o->localToAbsolute(FloatPoint(), false, true);
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !o->isBR() && !toRenderText(o)->firstTextBox()) {
                // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if ((o->isText() && !o->isBR()) || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText() && toRenderText(o)->firstTextBox()) {
                point.move(toRenderText(o)->linesBoundingBox().x(),
                           toRenderText(o)->firstTextBox()->root()->lineTop());
            } else if (o->isBox()) {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->location());
            }
            point = o->container()->localToAbsolute(point, false, true);
            return true;
        }
    }
    
    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document.  Scroll to the bottom. FIXME: who said anything about scrolling?
    if (!o && document()->view()) {
        point = FloatPoint(0, document()->view()->contentsHeight());
        return true;
    }
    return false;
}

// FIXME: This doesn't work correctly with transforms.
bool ContainerNode::getLowerRightCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;

    RenderObject* o = renderer();
    if (!o->isInline() || o->isReplaced()) {
        RenderBox* box = toRenderBox(o);
        point = o->localToAbsolute(FloatPoint(), false, true);
        point.move(box->size());
        return true;
    }

    // find the last text/image child, to get a position
    while (o) {
        if (o->lastChild())
            o = o->lastChild();
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
                LayoutRect linesBox = text->linesBoundingBox();
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
            } else {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->frameRect().maxXMaxYCorner());
            }
            point = o->container()->localToAbsolute(point, false, true);
            return true;
        }
    }
    return true;
}

LayoutRect ContainerNode::getRect() const
{
    FloatPoint  upperLeft, lowerRight;
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

void ContainerNode::setFocus(bool received)
{
    if (focused() == received)
        return;

    Node::setFocus(received);

    // note that we need to recalc the style
    setNeedsStyleRecalc();
}

void ContainerNode::setActive(bool down, bool pause)
{
    if (down == active()) return;

    Node::setActive(down);

    // note that we need to recalc the style
    // FIXME: Move to Element
    if (renderer()) {
        bool reactsToPress = renderer()->style()->affectedByActiveRules();
        if (reactsToPress)
            setNeedsStyleRecalc();
        if (renderer() && renderer()->style()->hasAppearance()) {
            if (renderer()->theme()->stateChanged(renderer(), PressedState))
                reactsToPress = true;
        }
        if (reactsToPress && pause) {
            // The delay here is subtle.  It relies on an assumption, namely that the amount of time it takes
            // to repaint the "down" state of the control is about the same time as it would take to repaint the
            // "up" state.  Once you assume this, you can just delay for 100ms - that time (assuming that after you
            // leave this method, it will be about that long before the flush of the up state happens again).
#ifdef HAVE_FUNC_USLEEP
            double startTime = currentTime();
#endif

            // Ensure there are no pending changes
            Document::updateStyleForAllDocuments();
            // Do an immediate repaint.
            if (renderer())
                renderer()->repaint(true);
            
            // FIXME: Find a substitute for usleep for Win32.
            // Better yet, come up with a way of doing this that doesn't use this sort of thing at all.            
#ifdef HAVE_FUNC_USLEEP
            // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
            double remainingTime = 0.1 - (currentTime() - startTime);
            if (remainingTime > 0)
                usleep(static_cast<useconds_t>(remainingTime * 1000000.0));
#endif
        }
    }
}

void ContainerNode::setHovered(bool over)
{
    if (over == hovered()) return;

    Node::setHovered(over);

    // note that we need to recalc the style
    // FIXME: Move to Element
    if (renderer()) {
        if (renderer()->style()->affectedByHoverRules())
            setNeedsStyleRecalc();
        if (renderer() && renderer()->style()->hasAppearance())
            renderer()->theme()->stateChanged(renderer(), HoverState);
    }
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

static void notifyChildInserted(Node* child)
{
    ASSERT(!eventDispatchForbidden());

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::didInsertDOMNode(child->document(), child);
#endif

    RefPtr<Node> c = child;
    RefPtr<Document> document = child->document();

    Node* parentOrHostNode = c->parentOrHostNode();
    if (parentOrHostNode && parentOrHostNode->inDocument())
        c->insertedIntoDocument();
    else
        c->insertedIntoTree(true);

    document->incDOMTreeVersion();
}

static void dispatchChildInsertionEvents(Node* child)
{
    ASSERT(!eventDispatchForbidden());

    RefPtr<Node> c = child;
    RefPtr<Document> document = child->document();

#if ENABLE(MUTATION_OBSERVERS)
    if (c->parentNode()) {
        ChildListMutationScope mutation(c->parentNode());
        mutation.childAdded(c.get());
    }
#endif

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = c->traverseNextNode(child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, false));
    }
}

static void dispatchChildRemovalEvents(Node* child)
{
    ASSERT(!eventDispatchForbidden());

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::willRemoveDOMNode(child->document(), child);
#endif

    RefPtr<Node> c = child;
    RefPtr<Document> document = child->document();

#if ENABLE(MUTATION_OBSERVERS)
    if (c->parentNode()) {
        ChildListMutationScope mutation(c->parentNode());
        mutation.willRemoveChild(c.get());
        c->notifyMutationObserversNodeWillDetach();
    }
#endif

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, true, c->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (; c; c = c->traverseNextNode(child))
            c->dispatchScopedEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, false));
    }
}

} // namespace WebCore
