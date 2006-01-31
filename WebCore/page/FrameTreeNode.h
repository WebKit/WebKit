// -*- mode: c++; c-basic-offset: 4 -*-
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

#ifndef FRAME_TREE_NODE_H
#define FRAME_TREE_NODE_H

#include <kxmlcore/RefPtr.h>
#include <kxmlcore/PassRefPtr.h>

namespace WebCore {

class Frame;

class FrameTreeNode
{
public:
    FrameTreeNode(Frame* thisFrame) 
        : m_thisFrame(thisFrame)
        , m_previousSibling(0)
        , m_lastChild(0)
        , m_childCount(0)
    {
    }
    ~FrameTreeNode();

    Frame* nextSibling() { return m_nextSibling.get(); }
    Frame* previousSibling() { return m_previousSibling; }
    Frame* firstChild() { return m_firstChild.get(); }
    Frame* lastChild() { return m_lastChild; }
    int childCount() { return m_childCount; }

    void appendChild(PassRefPtr<Frame> child);
    void removeChild(Frame *child);

 private:
    Frame* m_thisFrame;

    // FIXME: use ListRefPtr?
    RefPtr<Frame> m_nextSibling;
    Frame* m_previousSibling;
    RefPtr<Frame> m_firstChild;
    Frame* m_lastChild;
    int m_childCount;

    // uncopyable
    FrameTreeNode(const FrameTreeNode&);
    FrameTreeNode& operator=(const FrameTreeNode&);
};

}

#endif // FRAME_TREE_NODE_H
