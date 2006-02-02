// -*- c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "FrameTreeNode.h"

#include <kxmlcore/Assertions.h>
#include "Frame.h"
#include "NodeImpl.h"

#include <algorithm>

using std::swap;

namespace WebCore {

FrameTreeNode::~FrameTreeNode()
{
    for (Frame* child = firstChild(); child; child = child->treeNode()->nextSibling())
        child->detachFromView();
}

void FrameTreeNode::setName(const DOMString& name) 
{ 
    DOMString n = name;
        
    // FIXME: is the blank rule needed or useful?
    if (parent() && (name.isEmpty() || parent()->frameExists(name.qstring()) || name == "_blank"))
        n = parent()->requestFrameName();
    
    m_name = n;
}

void FrameTreeNode::appendChild(PassRefPtr<Frame> child)
{
    child->treeNode()->m_parent = m_thisFrame;

    Frame* oldLast = m_lastChild;
    m_lastChild = child.get();

    if (oldLast) {
        child->treeNode()->m_previousSibling = oldLast;
        oldLast->treeNode()->m_nextSibling = child;
    } else
        m_firstChild = child;

    m_childCount++;

    ASSERT(!m_lastChild->treeNode()->m_nextSibling);
}

void FrameTreeNode::removeChild(Frame* child)
{
    child->treeNode()->m_parent = 0;
    child->detachFromView();

    // Slightly tricky way to prevent deleting the child until we are done with it, w/o
    // extra refs. These swaps leave the child in a circular list by itself. Clearing its
    // previous and next will then finally deref it.

    RefPtr<Frame>& newLocationForNext = m_firstChild == child ? m_firstChild : child->treeNode()->m_previousSibling->treeNode()->m_nextSibling;
    Frame*& newLocationForPrevious = m_lastChild == child ? m_lastChild : child->treeNode()->m_nextSibling->treeNode()->m_previousSibling;
    swap(newLocationForNext, child->treeNode()->m_nextSibling);
    swap(newLocationForPrevious, child->treeNode()->m_previousSibling);

    child->treeNode()->m_previousSibling = 0;
    child->treeNode()->m_nextSibling = 0;

    m_childCount--;
}

}
