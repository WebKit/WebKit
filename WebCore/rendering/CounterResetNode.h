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
#ifndef CounterResetNode_h
#define CounterResetNode_h

#include "CounterNode.h"

namespace WebCore {

// This file implements a counter-tree that is used for finding all parents in counters() lookup,
// and for propagating count-changes when nodes are added or removed.
// Please note that only counter-reset and root can be parents here, and that render-tree parents
// are just counter-tree siblings

// Implementation of counter-reset and root
class CounterResetNode : public CounterNode {
public:
    CounterResetNode(RenderObject*);

    virtual CounterNode* firstChild() const { return m_first; };
    virtual CounterNode* lastChild() const { return m_last; };
    virtual void insertAfter(CounterNode* newChild, CounterNode* refChild);
    virtual void removeChild(CounterNode*);

    virtual bool isReset() const { return true; };
    virtual CounterNode* recountAndGetNext(bool setDirty = true);
    virtual void setParentDirty();

    void updateTotal(int value);
    // The highest value among children
    int total() const { return m_total; };

private:
    int m_total;
    CounterNode* m_first;
    CounterNode* m_last;
};

} // namespace WebCore

#endif // CounterResetNode_h
