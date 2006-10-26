/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
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
 *
*/
#ifndef CounterNode_h
#define CounterNode_h

#include "RenderObject.h"

namespace WebCore {

class CounterResetNode;

// This file implements a counter-tree that is used for finding all parents in counters() lookup,
// and for propagating count-changes when nodes are added or removed.
// Please note that only counter-reset and root can be parents here, and that render-tree parents
// are just counter-tree siblings

class CounterNode {
public:
    CounterNode(RenderObject*);
    virtual ~CounterNode() { }

    CounterResetNode* parent() const { return m_parent; }
    CounterNode* previousSibling() const { return m_previous; }
    virtual CounterNode* nextSibling() const { return m_next; }
    virtual CounterNode* firstChild() const { return 0; }
    virtual CounterNode* lastChild() const { return 0; }
    virtual void insertAfter(CounterNode* newChild, CounterNode* refChild);
    virtual void removeChild(CounterNode*);
    void remove();

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; }
    int count() const { return m_count; }
    void setCount(int c) { m_count = c; }
    void setHasSeparator() { m_hasSeparator = true; }

    virtual bool isReset() const { return false; }
    virtual CounterNode* recountAndGetNext(bool setDirty = true);
    virtual void recountTree(bool setDirty = true);
    virtual void setSelfDirty();
    virtual void setParentDirty();

    bool hasSeparator() const { return m_hasSeparator; }
    bool willNeedLayout() const { return m_willNeedLayout; }
    void setUsesSeparator();
    void setWillNeedLayout() { m_willNeedLayout = true; }
    bool isRoot() const { return m_renderer && m_renderer->isRoot(); }

    void setRenderer(RenderObject* o) { m_renderer = o; }
    RenderObject* renderer() const { return m_renderer; }

    friend class CounterResetNode;
protected:
    bool m_hasSeparator;
    bool m_willNeedLayout;
    int m_value;
    int m_count;
    CounterResetNode* m_parent;
    CounterNode* m_previous;
    CounterNode* m_next;
    RenderObject* m_renderer;
};

} // namespace WebCore

#endif // CounterNode_h
