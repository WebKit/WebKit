/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2002 (hyatt@apple.com)
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

#include "render_layer.h"
#include <kdebug.h>
#include <assert.h>
#include "khtmlview.h"

using namespace DOM;
using namespace khtml;

RenderLayer::RenderLayer(RenderObject* object)
: m_object( object ),
m_parent( 0 ),
m_previous( 0 ),
m_next( 0 ),
m_first( 0 ),
m_last( 0 ),
m_height( 0 ),
m_y( 0 ),
m_x( 0 ),
m_width( 0 )
{
}

RenderLayer::~RenderLayer()
{
    // Child layers will be deleted by their corresponding render objects, so
    // our destructor doesn't have to do anything.
}

void RenderLayer::addChild(RenderLayer *child, RenderLayer *beforeChild)
{
    if (!beforeChild)
        setLastChild(child);
        
    if(beforeChild == firstChild()) // Handles the null case too.
        setFirstChild(child);

    RenderLayer* prev = beforeChild ? beforeChild->previousSibling() : 0;
    child->setNextSibling(beforeChild);
    if (beforeChild) beforeChild->setPreviousSibling(child);
    if(prev) prev->setNextSibling(child);
    child->setPreviousSibling(prev);
    child->setParent(this);
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    return oldChild;
}


