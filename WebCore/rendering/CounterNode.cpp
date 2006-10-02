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
#include "CounterNode.h"

#include "CounterResetNode.h"
#include "RenderObject.h"

namespace WebCore {

CounterNode::CounterNode(RenderObject* o)
    : m_hasSeparator(false)
    , m_willNeedLayout(false)
    , m_value(0)
    , m_count (0)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_renderer(o)
{   
}

void CounterNode::insertAfter(CounterNode*, CounterNode*)
{
    ASSERT(false);
}

void CounterNode::removeChild(CounterNode*)
{
    ASSERT(false);
}

void CounterNode::remove()
{
    if (m_parent)
        m_parent->removeChild(this);
    else {
        ASSERT(isReset());
        ASSERT(!firstChild());
        ASSERT(!lastChild());
    }
}

void CounterNode::setUsesSeparator()
{
    for (CounterNode* c = this; c; c = c->parent())
        c->setHasSeparator();
}

CounterNode* CounterNode::recountAndGetNext(bool setDirty)
{
    int old_count = m_count;
    if (m_previous)
        m_count = m_previous->count() + m_value;
    else {
        assert(m_parent->firstChild() == this);
        m_count = m_parent->value() + m_value;
    }
    
    if (old_count != m_count && setDirty)
        setSelfDirty();
    if (old_count != m_count || !setDirty) {
        if (m_parent)
            m_parent->updateTotal(m_count);
        return m_next;
    }
    
    return 0;
}

void CounterNode::recountTree(bool setDirty)
{
    CounterNode* counterNode = this;
    while (counterNode)
        counterNode = counterNode->recountAndGetNext(setDirty);
}

void CounterNode::setSelfDirty()
{
    if (m_renderer && m_willNeedLayout)
        m_renderer->setNeedsLayoutAndMinMaxRecalc();
}

void CounterNode::setParentDirty()
{
    if (m_renderer && m_willNeedLayout && m_hasSeparator)
        m_renderer->setNeedsLayoutAndMinMaxRecalc();
}

} // namespace
