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

#include "config.h"
#include "CounterResetNode.h"

#include "RenderObject.h"

namespace WebCore {

CounterResetNode::CounterResetNode(RenderObject* o)
    : CounterNode(o)
    , m_total(0)
    , m_first(0)
    , m_last(0)
{
}

void CounterResetNode::insertAfter(CounterNode* newChild, CounterNode* refChild)
{
    ASSERT(newChild);
    ASSERT(!refChild || refChild->parent() == this);

    newChild->m_parent = this;
    newChild->m_previous = refChild;

    if (refChild) {
        newChild->m_next = refChild->m_next;
        refChild->m_next = newChild;
    } else {
        newChild->m_next = m_first;
        m_first = newChild;
    }

    if (newChild->m_next) {
        ASSERT(newChild->m_next->m_previous == refChild);
        newChild->m_next->m_previous = newChild;
    } else {
        ASSERT(m_last == refChild);
        m_last = newChild;
    }

    newChild->recountAndGetNext(false);
}

void CounterResetNode::removeChild(CounterNode* oldChild)
{
    ASSERT(oldChild);

    CounterNode* next = oldChild->m_next;
    CounterNode* prev = oldChild->m_previous;

    if (oldChild->firstChild()) {
        CounterNode* first = oldChild->firstChild();
        CounterNode* last = oldChild->lastChild();
        if (prev) {
            prev->m_next = first;
            first->m_previous = prev;
        } else
            m_first = first;

        if (next) {
            next->m_previous = last;
            last->m_next = next;
        } else
            m_last = last;

        next = first;
        while (next) {
            next->m_parent = this;
            next = next->m_next;
        }

        first->recountAndGetNext(true);
    } else {
        if (prev) 
            prev->m_next = next;
        else {
            assert ( m_first == oldChild );
            m_first = next;
        }
        
        if (next)
            next->m_previous = prev;
        else {
            assert ( m_last == oldChild );
            m_last = prev;
        }
        
        if (next)
            next->recountTree();
    }


    oldChild->m_next = 0;
    oldChild->m_previous = 0;
    oldChild->m_parent = 0;
}

CounterNode* CounterResetNode::recountAndGetNext(bool setDirty)
{
    int old_count = m_count;
    if (m_previous)
        m_count = m_previous->count();
    else if (m_parent)
        m_count = m_parent->value();
    else
        m_count = 0;

    updateTotal(m_value);
    
    if (setDirty)
        setSelfDirty();
    if (!setDirty || m_count != old_count)
        return m_next;
    
    return 0;
}

void CounterResetNode::setParentDirty()
{
    if (hasSeparator())
        setSelfDirty();
        
    for (CounterNode* n = firstChild(); n; n = n->nextSibling())
        if (n->hasSeparator())
            n->setSelfDirty();
}

void CounterResetNode::updateTotal(int value)
{
    if (value > m_total)
        m_total = value;
}

} // namespace
