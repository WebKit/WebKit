/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ContainerNodeImpl.h"

#include "DocumentImpl.h"
#include "EventNames.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "IntRect.h"
#include "RenderText.h"
#include "dom2_eventsimpl.h"
#include "dom_exception.h"
#include "dom_node.h"
#include "render_theme.h"
#include <qdatetime.h>

namespace WebCore {

using namespace EventNames;

ContainerNodeImpl::ContainerNodeImpl(DocumentImpl* doc)
    : NodeImpl(doc), m_firstChild(0), m_lastChild(0)
{
}

void ContainerNodeImpl::removeAllChildren()
{
    // Avoid deep recursion when destroying the node tree.
    static bool alreadyInsideDestructor; 
    bool topLevel = !alreadyInsideDestructor;
    if (topLevel)
        alreadyInsideDestructor = true;
    
    // List of nodes to be deleted.
    static NodeImpl *head;
    static NodeImpl *tail;
    
    // We have to tell all children that their parent has died.
    NodeImpl *n;
    NodeImpl *next;

    for (n = m_firstChild; n != 0; n = next ) {
        next = n->nextSibling();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        
        if ( !n->refCount() ) {
            // Add the node to the list of nodes to be deleted.
            // Reuse the nextSibling pointer for this purpose.
            if (tail)
                tail->setNextSibling(n);
            else
                head = n;
            tail = n;
        } else if (n->inDocument())
            n->removedFromDocument();
    }
    
    // Only for the top level call, do the actual deleting.
    if (topLevel) {
        while ((n = head) != 0) {
            next = n->nextSibling();
            n->setNextSibling(0);

            head = next;
            if (next == 0)
                tail = 0;
            
            delete n;
        }
        
        alreadyInsideDestructor = false;
        m_firstChild = 0;
        m_lastChild = 0;
    }
}

ContainerNodeImpl::~ContainerNodeImpl()
{
    removeAllChildren();
}


NodeImpl* ContainerNodeImpl::firstChild() const
{
    return m_firstChild;
}

NodeImpl* ContainerNodeImpl::lastChild() const
{
    return m_lastChild;
}

PassRefPtr<NodeImpl> ContainerNodeImpl::insertBefore(PassRefPtr<NodeImpl> newChild, NodeImpl* refChild, ExceptionCode& ec)
{
    ec = 0;

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild)
        return appendChild(newChild, ec);

    // Make sure adding the new child is OK.
    checkAddChild(newChild.get(), ec);
    if (ec)
        return 0;

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        ec = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;

    // If newChild is a DocumentFragment with no children; there's nothing to do.
    // Just return the document fragment.
    if (isFragment && !newChild->firstChild())
        return newChild;

    // Now actually add the child(ren)
    NodeImpl* nextChild;
    NodeImpl* child = isFragment ? newChild->firstChild() : newChild.get();

    NodeImpl* prev = refChild->previousSibling();
    if (prev == newChild || refChild == newChild) // nothing to do
        return newChild;
    
    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *newParent = child->parentNode();
        if (newParent)
            newParent->removeChild(child, ec);
        if (ec)
            return 0;

        // Add child in the correct position
        forbidEventDispatch();
        if (prev)
            prev->setNextSibling(child);
        else
            m_firstChild = child;
        refChild->setPreviousSibling(child);
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(refChild);
        allowEventDispatch();

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();
        
        // Dispatch the mutation events
        dispatchChildInsertedEvents(child, ec);

        prev = child;
        child = nextChild;
    }

    getDocument()->setDocumentChanged(true);
    dispatchSubtreeModifiedEvent();
    return newChild;
}

PassRefPtr<NodeImpl> ContainerNodeImpl::replaceChild(PassRefPtr<NodeImpl> newChild, NodeImpl* oldChild, ExceptionCode& ec)
{
    ec = 0;

    if (oldChild == newChild) // nothing to do
        return newChild;
    
    // Make sure adding the new child is ok
    checkAddChild(newChild.get(), ec);
    if (ec)
        return 0;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        ec = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
    RefPtr<NodeImpl> child = isFragment ? newChild->firstChild() : newChild.get();

    // Remove the old child
    RefPtr<NodeImpl> prev = oldChild->previousSibling();
    RefPtr<NodeImpl> next = oldChild->nextSibling();

    PassRefPtr<NodeImpl> removedChild = removeChild(oldChild, ec);
    if (ec)
        return 0;

    // If the new child was right before or right after the old child, nothing else needs to change
    if (prev == child || next == child)
        child = 0;
    // Add the new child(ren)
    while (child) {
        RefPtr<NodeImpl> nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl* newParent = child->parentNode();
        if (newParent)
            newParent->removeChild(child.get(), ec);
        if (ec)
            return 0;

        // Add child in the correct position
        forbidEventDispatch();
        if (prev)
            prev->setNextSibling(child.get());
        if (next)
            next->setPreviousSibling(child.get());
        if (!prev)
            m_firstChild = child.get();
        if (!next)
            m_lastChild = child.get();
        child->setParent(this);
        child->setPreviousSibling(prev.get());
        child->setNextSibling(next.get());
        allowEventDispatch();

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();

        // Dispatch the mutation events
        dispatchChildInsertedEvents(child.get(), ec);

        prev = child;
        child = nextChild;
    }

    // ### set style in case it's attached
    getDocument()->setDocumentChanged(true);
    dispatchSubtreeModifiedEvent();
    return removedChild;
}

void ContainerNodeImpl::willRemove()
{
    for (NodeImpl *n = m_firstChild; n != 0; n = n->nextSibling())
        n->willRemove();
}

int ContainerNodeImpl::willRemoveChild(NodeImpl *child)
{
    int exceptionCode = 0;

    // fire removed from document mutation events.
    dispatchChildRemovalEvents(child, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    if (child->attached())
        child->willRemove();
    
    return 0;
}

PassRefPtr<NodeImpl> ContainerNodeImpl::removeChild(NodeImpl* oldChild, ExceptionCode& ec)
{
    ec = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
        ec = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        ec = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    PassRefPtr<NodeImpl> child = oldChild;
    
    // dispatch pre-removal mutation events
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
        child->dispatchEvent(new MutationEventImpl(DOMNodeRemovedEvent, true, false,
            this, DOMString(), DOMString(), DOMString(), 0), ec, true);
        if (ec)
            return 0;
    }

    ec = willRemoveChild(child.get());
    if (ec)
        return 0;

    forbidEventDispatch();

    // Remove from rendering tree
    if (child->attached())
        child->detach();

    // Remove the child
    NodeImpl *prev, *next;
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

    getDocument()->setDocumentChanged(true);

    // Dispatch post-removal mutation events
    dispatchSubtreeModifiedEvent();

    if (child->inDocument())
        child->removedFromDocument();
    else
        child->removedFromTree(true);

    return child;
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNodeImpl::removeChildren()
{
    NodeImpl *n;
    
    if (!m_firstChild)
        return;

    // do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events
    for (n = m_firstChild; n; n = n->nextSibling())
        willRemoveChild(n);
    
    forbidEventDispatch();
    while ((n = m_firstChild) != 0) {
        NodeImpl *next = n->nextSibling();
        
        n->ref();

        if (n->attached())
            n->detach();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        
        if (n->inDocument())
            n->removedFromDocument();

        n->deref();

        m_firstChild = next;
    }
    m_lastChild = 0;
    allowEventDispatch();
    
    // Dispatch a single post-removal mutation event denoting a modified subtree.
    dispatchSubtreeModifiedEvent();
}


PassRefPtr<NodeImpl> ContainerNodeImpl::appendChild(PassRefPtr<NodeImpl> newChild, ExceptionCode& ec)
{
    ec = 0;

    // Make sure adding the new child is ok
    checkAddChild(newChild.get(), ec);
    if (ec)
        return 0;
    
    if (newChild == m_lastChild) // nothing to do
        return newChild;

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;

    // If newChild is a DocumentFragment with no children.... there's nothing to do.
    // Just return the document fragment
    if (isFragment && !newChild->firstChild())
        return newChild;

    // Now actually add the child(ren)
    RefPtr<NodeImpl> child = newChild.get();
    if (isFragment)
        child = newChild->firstChild();
    while (child) {
        RefPtr<NodeImpl> nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        if (NodeImpl* oldParent = child->parentNode()) {
            oldParent->removeChild(child.get(), ec);
            if (ec)
                return 0;
        }

        // Append child to the end of the list
        forbidEventDispatch();
        child->setParent(this);
        if (m_lastChild) {
            child->setPreviousSibling(m_lastChild);
            m_lastChild->setNextSibling(child.get());
        } else
            m_firstChild = child.get();
        m_lastChild = child.get();
        allowEventDispatch();

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();
        
        // Dispatch the mutation events
        dispatchChildInsertedEvents(child.get(), ec);

        child = nextChild;
    }

    getDocument()->setDocumentChanged(true);
    // ### set style in case it's attached
    dispatchSubtreeModifiedEvent();
    return newChild;
}

bool ContainerNodeImpl::hasChildNodes() const
{
    return m_firstChild;
}

ContainerNodeImpl* ContainerNodeImpl::addChild(PassRefPtr<NodeImpl> newChild)
{
    // This function is only used during parsing.
    // It does not send any DOM mutation events.

    // Check for consistency with DTD, but only when parsing HTML.
    if (getDocument()->isHTMLDocument() && !childAllowed(newChild.get()))
        return 0;

    forbidEventDispatch();
    newChild->setParent(this);
    if (m_lastChild) {
        newChild->setPreviousSibling(m_lastChild);
        m_lastChild->setNextSibling(newChild.get());
    } else
        m_firstChild = newChild.get();
    m_lastChild = newChild.get();
    allowEventDispatch();

    if (inDocument())
        newChild->insertedIntoDocument();
    childrenChanged();
    
    if (newChild->nodeType() == Node::ELEMENT_NODE)
        return static_cast<ContainerNodeImpl*>(newChild.get());
    return this;
}

void ContainerNodeImpl::attach()
{
    for (NodeImpl* child = m_firstChild; child; child = child->nextSibling())
        child->attach();
    NodeImpl::attach();
}

void ContainerNodeImpl::detach()
{
    for (NodeImpl* child = m_firstChild; child; child = child->nextSibling())
        child->detach();
    NodeImpl::detach();
}

void ContainerNodeImpl::insertedIntoDocument()
{
    NodeImpl::insertedIntoDocument();
    for (NodeImpl *child = m_firstChild; child; child = child->nextSibling())
        child->insertedIntoDocument();
}

void ContainerNodeImpl::removedFromDocument()
{
    NodeImpl::removedFromDocument();
    for (NodeImpl *child = m_firstChild; child; child = child->nextSibling())
        child->removedFromDocument();
}

void ContainerNodeImpl::insertedIntoTree(bool deep)
{
    NodeImpl::insertedIntoTree(deep);
    if (deep) {
        for (NodeImpl *child = m_firstChild; child; child = child->nextSibling())
            child->insertedIntoTree(deep);
    }
}

void ContainerNodeImpl::removedFromTree(bool deep)
{
    NodeImpl::removedFromTree(deep);
    if (deep) {
        for (NodeImpl *child = m_firstChild; child; child = child->nextSibling())
            child->removedFromTree(deep);
    }
}

void ContainerNodeImpl::cloneChildNodes(NodeImpl *clone)
{
    int exceptioncode = 0;
    NodeImpl *n;
    for(n = firstChild(); n && !exceptioncode; n = n->nextSibling())
    {
        clone->appendChild(n->cloneNode(true),exceptioncode);
    }
}

bool ContainerNodeImpl::getUpperLeftCorner(int &xPos, int &yPos) const
{
    if (!renderer())
        return false;
    RenderObject *o = renderer();
    RenderObject *p = o;

    xPos = yPos = 0;
    if (!o->isInline() || o->isReplaced()) {
        o->absolutePosition( xPos, yPos );
        return true;
    }

    // find the next text/image child, to get a position
    while(o) {
        p = o;
        if(o->firstChild())
            o = o->firstChild();
        else if(o->nextSibling())
            o = o->nextSibling();
        else {
            o = o->parent();
            if (o) 
                o = o->nextSibling();
            if (!o)
                break;
        }

        if (!o->isInline() || o->isReplaced()) {
            o->absolutePosition( xPos, yPos );
            return true;
        }

        if (p->element() && p->element() == this && o->isText() && !o->isBR() && !static_cast<RenderText*>(o)->firstTextBox()) {
                // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        }
        else if((o->isText() && !o->isBR()) || o->isReplaced()) {
            o->container()->absolutePosition( xPos, yPos );
            if (o->isText() && static_cast<RenderText *>(o)->firstTextBox()) {
                xPos += static_cast<RenderText *>(o)->minXPos();
                yPos += static_cast<RenderText *>(o)->firstTextBox()->root()->topOverflow();
            }
            else {
                xPos += o->xPos();
                yPos += o->yPos();
            }
            return true;
        }
    }
    
    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document.  Scroll to the bottom.
    if (!o) {
        yPos += getDocument()->view()->contentsHeight();
        return true;
    }
    return false;
}

bool ContainerNodeImpl::getLowerRightCorner(int &xPos, int &yPos) const
{
    if (!renderer())
        return false;

    RenderObject *o = renderer();
    xPos = yPos = 0;
    if (!o->isInline() || o->isReplaced())
    {
        o->absolutePosition( xPos, yPos );
        xPos += o->width();
        yPos += o->height();
        return true;
    }
    // find the last text/image child, to get a position
    while(o) {
        if(o->lastChild())
            o = o->lastChild();
        else if(o->previousSibling())
            o = o->previousSibling();
        else {
            RenderObject *prev = 0;
            while(!prev) {
                o = o->parent();
                if(!o) return false;
                prev = o->previousSibling();
            }
            o = prev;
        }
        if(o->isText() || o->isReplaced()) {
            o->container()->absolutePosition(xPos, yPos);
            if (o->isText())
                xPos += static_cast<RenderText *>(o)->minXPos() + o->width();
            else
                xPos += o->xPos()+o->width();
            yPos += o->yPos()+o->height();
            return true;
        }
    }
    return true;
}

IntRect ContainerNodeImpl::getRect() const
{
    int xPos = 0, yPos = 0, xEnd = 0, yEnd = 0;
    bool foundUpperLeft = getUpperLeftCorner(xPos,yPos);
    bool foundLowerRight = getLowerRightCorner(xEnd,yEnd);
    
    // If we've found one corner, but not the other,
    // then we should just return a point at the corner that we did find.
    if (foundUpperLeft != foundLowerRight)
    {
        if (foundUpperLeft) {
            xEnd = xPos;
            yEnd = yPos;
        } else {
            xPos = xEnd;
            yPos = yEnd;
        }
    } 

    if (xEnd < xPos)
        xEnd = xPos;
    if (yEnd < yPos)
        yEnd = yPos;
        
    return IntRect(xPos, yPos, xEnd - xPos, yEnd - yPos);
}

void ContainerNodeImpl::setFocus(bool received)
{
    if (m_focused == received) return;

    NodeImpl::setFocus(received);

    // note that we need to recalc the style
    setChanged();
}

void ContainerNodeImpl::setActive(bool down, bool pause)
{
    if (down == active()) return;

    NodeImpl::setActive(down);

    // note that we need to recalc the style
    // FIXME: Move to ElementImpl
    if (renderer()) {
        bool reactsToPress = renderer()->style()->affectedByActiveRules();
        if (reactsToPress)
            setChanged();
        if (renderer() && renderer()->style()->hasAppearance()) {
            if (theme()->stateChanged(renderer(), PressedState))
                reactsToPress = true;
        }
        if (reactsToPress && pause) {
            // The delay here is subtle.  It relies on an assumption, namely that the amount of time it takes
            // to repaint the "down" state of the control is about the same time as it would take to repaint the
            // "up" state.  Once you assume this, you can just delay for 100ms - that time (assuming that after you
            // leave this method, it will be about that long before the flush of the up state happens again).
            QTime startTime;
            startTime.restart();

            // Do an immediate repaint.
            renderer()->repaint(true);
            
#if !WIN32
            // FIXME: Find a substitute for usleep.  Better yet, come up with a way of doing this that 
            // doesn't use usleep at all.
            int remainingTime = 100 - startTime.elapsed();
            
            // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
            if (remainingTime > 0)
                usleep(remainingTime * 1000);
#endif
        }
    }
}

void ContainerNodeImpl::setHovered(bool over)
{
    if (over == hovered()) return;

    NodeImpl::setHovered(over);

    // note that we need to recalc the style
    // FIXME: Move to ElementImpl
    if (renderer()) {
        if (renderer()->style()->affectedByHoverRules())
            setChanged();
        if (renderer() && renderer()->style()->hasAppearance())
            theme()->stateChanged(renderer(), HoverState);
    }
}

unsigned ContainerNodeImpl::childNodeCount() const
{
    unsigned count = 0;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

NodeImpl *ContainerNodeImpl::childNode(unsigned index)
{
    unsigned i;
    NodeImpl *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

void ContainerNodeImpl::dispatchChildInsertedEvents( NodeImpl *child, int &exceptioncode )
{
    assert(!eventDispatchForbidden());
    if (inDocument())
        child->insertedIntoDocument();
    else
        child->insertedIntoTree(true);

    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTED_LISTENER)) {
        child->dispatchEvent(new MutationEventImpl(DOMNodeInsertedEvent,
                                                   true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
        if (exceptioncode)
            return;
    }

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    bool hasInsertedListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER);

    if (hasInsertedListeners && inDocument()) {
        for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
            c->dispatchEvent(new MutationEventImpl(DOMNodeInsertedIntoDocumentEvent,
                                                   false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
            if (exceptioncode)
                return;
        }
    }
}

// base class for nodes that may have children
void ContainerNodeImpl::dispatchChildRemovalEvents( NodeImpl *child, int &exceptioncode )
{
    // update auxiliary doc info (e.g. iterators) to note that node is being removed
    getDocument()->notifyBeforeNodeRemoval(child); // ### use events instead

    // dispatch pre-removal mutation events
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
        child->dispatchEvent(new MutationEventImpl(DOMNodeRemovedEvent,
                             true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
        if (exceptioncode)
            return;
    }

    bool hasRemovalListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER);

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (inDocument()) {
        for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
            if (hasRemovalListeners) {
                c->dispatchEvent(new MutationEventImpl(DOMNodeRemovedFromDocumentEvent,
                                 false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
                if (exceptioncode)
                    return;
            }
        }
    }
}
}
