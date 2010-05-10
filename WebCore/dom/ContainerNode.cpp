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

#include "BeforeLoadEvent.h"
#include "Cache.h"
#include "ContainerNodeAlgorithms.h"
#include "DeleteButtonController.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "InspectorController.h"
#include "MutationEvent.h"
#include "Page.h"
#include "RenderTheme.h"
#include "RootInlineBox.h"
#include "loader.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>

namespace WebCore {

static void notifyChildInserted(Node*);
static void dispatchChildInsertionEvents(Node*);
static void dispatchChildRemovalEvents(Node*);

typedef Vector<std::pair<NodeCallback, RefPtr<Node> > > NodeCallbackQueue;
typedef Vector<RefPtr<Node>, 1> NodeVector;
static NodeCallbackQueue* s_postAttachCallbackQueue;

static size_t s_attachDepth;
static bool s_shouldReEnableMemoryCacheCallsAfterAttach;

static void collectTargetNodes(Node* node, NodeVector& nodes)
{
    if (node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE) {
        nodes.append(node);
        return;
    }

    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        nodes.append(child);
}

void ContainerNode::removeAllChildren()
{
    removeAllChildrenInContainer<Node, ContainerNode>(this);
}

ContainerNode::~ContainerNode()
{
    removeAllChildren();
}

bool ContainerNode::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parent());

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

    RefPtr<Node> next = refChild;
    RefPtr<Node> refChildPreviousSibling = refChild->previousSibling();
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();

        // If child is already present in the tree, first remove it from the old location.
        if (Node* oldParent = child->parentNode())
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

        ASSERT(!child->nextSibling());
        ASSERT(!child->previousSibling());

        // Add child before "next".
        forbidEventDispatch();
        Node* prev = next->previousSibling();
        ASSERT(m_lastChild != prev);
        next->setPreviousSibling(child);
        if (prev) {
            ASSERT(m_firstChild != next);
            ASSERT(prev->nextSibling() == next);
            prev->setNextSibling(child);
        } else {
            ASSERT(m_firstChild == next);
            m_firstChild = child;
        }
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(next.get());
        allowEventDispatch();

        // Send notification about the children change.
        childrenChanged(false, refChildPreviousSibling.get(), next.get(), 1);
        notifyChildInserted(child);
                
        // Add child to the rendering tree.
        if (attached() && !child->attached() && child->parent() == this) {
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

bool ContainerNode::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parent());

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
    int childCountDelta = 0;
    RefPtr<Node> child = isFragment ? newChild->firstChild() : newChild;
    while (child) {
        // If the new child is already in the right place, we're done.
        if (prev && (prev == child || prev == child->previousSibling()))
            break;

        // For a fragment we have more children to do.
        RefPtr<Node> nextChild = isFragment ? child->nextSibling() : 0;

        // Remove child from its old position.
        if (Node* oldParent = child->parentNode())
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

        childCountDelta++;

        ASSERT(!child->nextSibling());
        ASSERT(!child->previousSibling());

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

        notifyChildInserted(child.get());
                
        // Add child to the rendering tree
        if (attached() && !child->attached() && child->parent() == this) {
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

    if (childCountDelta)
        childrenChanged(false, prev.get(), next.get(), childCountDelta);
    dispatchSubtreeModifiedEvent();
    return true;
}

void ContainerNode::willRemove()
{
    for (Node *n = m_firstChild; n != 0; n = n->nextSibling())
        n->willRemove();
    Node::willRemove();
}

static void willRemoveChild(Node* child)
{
    // update auxiliary doc info (e.g. iterators) to note that node is being removed
    child->document()->nodeWillBeRemoved(child);
    child->document()->incDOMTreeVersion();

    // fire removed from document mutation events.
    dispatchChildRemovalEvents(child);

    if (child->attached())
        child->willRemove();
}

static void willRemoveChildren(ContainerNode* container)
{
    container->document()->nodeChildrenWillBeRemoved(container);
    container->document()->incDOMTreeVersion();

    // FIXME: Adding new children from event handlers can cause an infinite loop here.
    for (RefPtr<Node> child = container->firstChild(); child; child = child->nextSibling()) {
        // fire removed from document mutation events.
        dispatchChildRemovalEvents(child.get());

        if (child->attached())
            child->willRemove();
    }
}

bool ContainerNode::removeChild(Node* oldChild, ExceptionCode& ec)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parent());

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

    forbidEventDispatch();

    // Remove from rendering tree
    if (child->attached())
        child->detach();

    // Remove the child
    Node *prev, *next;
    prev = child->previousSibling();
    next = child->nextSibling();

    if (next)
        next->setPreviousSibling(prev);
    if (prev)
        prev->setNextSibling(next);
    if (m_firstChild == child)
        m_firstChild = next;
    if (m_lastChild == child)
        m_lastChild = prev;

    child->setPreviousSibling(0);
    child->setNextSibling(0);
    child->setParent(0);

    allowEventDispatch();

    // Dispatch post-removal mutation events
    childrenChanged(false, prev, next, -1);
    dispatchSubtreeModifiedEvent();

    if (child->inDocument())
        child->removedFromDocument();
    else
        child->removedFromTree(true);

    return child;
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
bool ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return false;

    // The container node can be removed from event handlers.
    RefPtr<ContainerNode> protect(this);

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren(protect.get());

    // exclude this node when looking for removed focusedNode since only children will be removed
    document()->removeFocusedNodeOfSubtree(this, true);

    forbidEventDispatch();
    Vector<RefPtr<Node> > removedChildren;
    while (RefPtr<Node> n = m_firstChild) {
        Node* next = n->nextSibling();
        
        // Remove the node from the tree before calling detach or removedFromDocument (4427024, 4129744).
        // removeChild() does this after calling detach(). There is no explanation for
        // this discrepancy between removeChild() and its optimized version removeChildren().
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);

        m_firstChild = next;
        if (n == m_lastChild)
            m_lastChild = 0;

        if (n->attached())
            n->detach();

        removedChildren.append(n.release());
    }
    allowEventDispatch();

    size_t removedChildrenCount = removedChildren.size();

    // Dispatch a single post-removal mutation event denoting a modified subtree.
    childrenChanged(false, 0, 0, -static_cast<int>(removedChildrenCount));
    dispatchSubtreeModifiedEvent();

    for (size_t i = 0; i < removedChildrenCount; ++i) {
        Node* removedChild = removedChildren[i].get();
        if (removedChild->inDocument())
            removedChild->removedFromDocument();
        // removeChild() calls removedFromTree(true) if the child was not in the
        // document. There is no explanation for this discrepancy between removeChild()
        // and its optimized version removeChildren().
    }

    return true;
}

bool ContainerNode::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec, bool shouldLazyAttach)
{
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parent());

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

    // Now actually add the child(ren)
    RefPtr<Node> prev = lastChild();
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        Node* child = it->get();
        // If child is already present in the tree, first remove it
        if (Node* oldParent = child->parentNode()) {
            oldParent->removeChild(child, ec);
            if (ec)
                return false;

            // If the child has a parent again, just stop what we're doing, because
            // that means someone is doing something with DOM mutation -- can't re-parent
            // a child that already has a parent.
            if (child->parentNode())
                break;
        }

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
        if (attached() && !child->attached() && child->parent() == this) {
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

ContainerNode* ContainerNode::addChild(PassRefPtr<Node> newChild)
{
    ASSERT(newChild);
    // This function is only used during parsing.
    // It does not send any DOM mutation events.

    // Check for consistency with DTD, but only when parsing HTML.
    if (document()->isHTMLDocument() && !childAllowed(newChild.get()))
        return 0;

    forbidEventDispatch();
    Node* last = m_lastChild;
    appendChildToContainer<Node, ContainerNode>(newChild.get(), this);
    allowEventDispatch();

    document()->incDOMTreeVersion();
    if (inDocument())
        newChild->insertedIntoDocument();
    childrenChanged(true, last, 0, 1);
    
    if (newChild->isElementNode())
        return static_cast<ContainerNode*>(newChild.get());
    return this;
}

void ContainerNode::suspendPostAttachCallbacks()
{
    if (!s_attachDepth) {
        ASSERT(!s_shouldReEnableMemoryCacheCallsAfterAttach);
        if (Page* page = document()->page()) {
            if (page->areMemoryCacheClientCallsEnabled()) {
                page->setMemoryCacheClientCallsEnabled(false);
                s_shouldReEnableMemoryCacheCallsAfterAttach = true;
            }
        }
        cache()->loader()->suspendPendingRequests();
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
        cache()->loader()->resumePendingRequests();
    }
    --s_attachDepth;
}

void ContainerNode::queuePostAttachCallback(NodeCallback callback, Node* node)
{
    if (!s_postAttachCallbackQueue)
        s_postAttachCallbackQueue = new NodeCallbackQueue;
    
    s_postAttachCallbackQueue->append(std::pair<NodeCallback, RefPtr<Node> >(callback, node));
}

void ContainerNode::dispatchPostAttachCallbacks()
{
    // We recalculate size() each time through the loop because a callback
    // can add more callbacks to the end of the queue.
    for (size_t i = 0; i < s_postAttachCallbackQueue->size(); ++i) {
        std::pair<NodeCallback, RefPtr<Node> >& pair = (*s_postAttachCallbackQueue)[i];
        NodeCallback callback = pair.first;
        Node* node = pair.second.get();

        callback(node);
    }
    s_postAttachCallbackQueue->clear();
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
    Node::insertedIntoDocument();
    insertedIntoTree(false);
    for (Node* child = m_firstChild; child; child = child->nextSibling())
        child->insertedIntoDocument();
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
        document()->nodeChildrenChanged(this);
    if (document()->hasNodeListCaches())
        notifyNodeListsChildrenChanged();
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
    // disable the delete button so it's elements are not serialized into the markup
    if (document()->frame())
        document()->frame()->editor()->deleteButtonController()->disable();
    ExceptionCode ec = 0;
    for (Node* n = firstChild(); n && !ec; n = n->nextSibling())
        clone->appendChild(n->cloneNode(true), ec);
    if (document()->frame())
        document()->frame()->editor()->deleteButtonController()->enable();
}

// FIXME: This doesn't work correctly with transforms.
bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject *o = renderer();
    RenderObject *p = o;

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute();
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
            point = o->localToAbsolute();
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !o->isBR() && !toRenderText(o)->firstTextBox()) {
                // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if ((o->isText() && !o->isBR()) || o->isReplaced()) {
            point = o->container()->localToAbsolute();
            if (o->isText() && toRenderText(o)->firstTextBox()) {
                point.move(toRenderText(o)->linesBoundingBox().x(),
                           toRenderText(o)->firstTextBox()->root()->lineTop());
            } else if (o->isBox()) {
                RenderBox* box = toRenderBox(o);
                point.move(box->x(), box->y());
            }
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
        point = o->localToAbsolute();
        point.move(box->width(), box->height());
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
            point = o->container()->localToAbsolute();
            if (o->isText()) {
                RenderText* text = toRenderText(o);
                IntRect linesBox = text->linesBoundingBox();
                point.move(linesBox.x() + linesBox.width(), linesBox.y() + linesBox.height());
            } else {
                RenderBox* box = toRenderBox(o);
                point.move(box->x() + box->width(), box->y() + box->height());
            }
            return true;
        }
    }
    return true;
}

IntRect ContainerNode::getRect() const
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

    lowerRight.setX(max(upperLeft.x(), lowerRight.x()));
    lowerRight.setY(max(upperLeft.y(), lowerRight.y()));
    
    return enclosingIntRect(FloatRect(upperLeft, lowerRight - upperLeft));
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
    if (Page* page = child->document()->page()) {
        if (InspectorController* inspectorController = page->inspectorController())
            inspectorController->didInsertDOMNode(child);
    }
#endif

    RefPtr<Node> c = child;
    RefPtr<Document> document = child->document();

    if (c->parentNode() && c->parentNode()->inDocument())
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

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchEvent(MutationEvent::create(eventNames().DOMNodeInsertedEvent, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = c->traverseNextNode(child))
            c->dispatchEvent(MutationEvent::create(eventNames().DOMNodeInsertedIntoDocumentEvent, false));
    }
}

static void dispatchChildRemovalEvents(Node* child)
{
    ASSERT(!eventDispatchForbidden());

#if ENABLE(INSPECTOR)    
    if (Page* page = child->document()->page()) {
        if (InspectorController* inspectorController = page->inspectorController())
            inspectorController->didRemoveDOMNode(child);
    }
#endif

    RefPtr<Node> c = child;
    RefPtr<Document> document = child->document();

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER))
        c->dispatchEvent(MutationEvent::create(eventNames().DOMNodeRemovedEvent, true, c->parentNode()));

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        for (; c; c = c->traverseNextNode(child))
            c->dispatchEvent(MutationEvent::create(eventNames().DOMNodeRemovedFromDocumentEvent, false));
    }
}

bool ContainerNode::dispatchBeforeLoadEvent(const String& sourceURL)
{
    if (!document()->hasListenerType(Document::BEFORELOAD_LISTENER))
        return true;

    RefPtr<ContainerNode> protector(this);
    RefPtr<BeforeLoadEvent> beforeLoadEvent = BeforeLoadEvent::create(sourceURL);
    dispatchEvent(beforeLoadEvent.get());
    return !beforeLoadEvent->defaultPrevented();
}

} // namespace WebCore
