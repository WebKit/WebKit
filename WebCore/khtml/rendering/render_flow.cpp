/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
// -------------------------------------------------------------------------

#include <kdebug.h>
#include <assert.h>
#include <qpainter.h>
#include <kglobal.h>

#include "rendering/render_flow.h"
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "rendering/render_root.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"
#include "render_inline.h"
#include "render_block.h"
#include "render_arena.h"
#include "render_line.h"

#include "khtmlview.h"
#include "htmltags.h"

using namespace DOM;
using namespace khtml;

RenderFlow* RenderFlow::createFlow(DOM::NodeImpl* node, RenderStyle* style, RenderArena* arena)
{
    RenderFlow* result;
    if (style->display() == INLINE)
        result = new (arena) RenderInline(node);
    else
        result = new (arena) RenderBlock(node);
    result->setStyle(style);
    return result;
}

RenderFlow* RenderFlow::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;
    
    RenderFlow* curr = continuation();
    RenderFlow* nextToLast = this;
    RenderFlow* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }
        
        nextToLast = last;
        last = curr;
        curr = curr->continuation();
    }
    
    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderFlow::addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderFlow* flow = continuationBefore(beforeChild);
    KHTMLAssert(!beforeChild || beforeChild->parent()->isRenderBlock() ||
                beforeChild->parent()->isRenderInline());
    RenderFlow* beforeChildParent = beforeChild ? static_cast<RenderFlow*>(beforeChild->parent()) : 
                                    (flow->continuation() ? flow->continuation() : flow);
    
    if (newChild->isSpecial())
        return beforeChildParent->addChildToFlow(newChild, beforeChild);
    
    // A continuation always consists of two potential candidates: an inline or an anonymous
    // block box holding block children.
    bool childInline = newChild->isInline();
    bool bcpInline = beforeChildParent->isInline();
    bool flowInline = flow->isInline();
    
    if (flow == beforeChildParent)
        return flow->addChildToFlow(newChild, beforeChild);
    else {
        // The goal here is to match up if we can, so that we can coalesce and create the
        // minimal # of continuations needed for the inline.
        if (childInline == bcpInline)
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
        else if (flowInline == childInline)
            return flow->addChildToFlow(newChild, 0); // Just treat like an append.
        else 
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
    }
}

void RenderFlow::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::addChild( " << newChild->renderName() <<
                       ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
    kdDebug( 6040 ) << "current height = " << m_height << endl;
#endif

    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && lastChild() && lastChild()->style()->styleType() == RenderStyle::AFTER)
        beforeChild = lastChild();
    
    if (continuation())
        return addChildWithContinuation(newChild, beforeChild);
    return addChildToFlow(newChild, beforeChild);
}

void RenderFlow::deleteLineBoxes(RenderArena* arena)
{
    if (m_firstLineBox) {
        if (!arena)
            arena = renderArena();
        InlineRunBox *curr=m_firstLineBox, *next=0;
        while (curr) {
            next = curr->nextLineBox();
            curr->detach(arena);
            curr = next;
        }
        m_firstLineBox = 0;
        m_lastLineBox = 0;
    }
}

void RenderFlow::detach(RenderArena* renderArena)
{
    deleteLineBoxes(renderArena);
    RenderBox::detach(renderArena);
}

InlineBox* RenderFlow::createInlineBox()
{
    InlineFlowBox* flowBox = new (renderArena()) InlineFlowBox(this);
    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = flowBox;
    else {
        m_lastLineBox->setNextLineBox(flowBox);
        flowBox->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = flowBox;
    }

    return flowBox;
}

