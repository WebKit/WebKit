/**
* This file is part of the html renderer for KDE.
 *
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

#ifndef NDEBUG
static bool inInlineBoxDetach;
#endif

void InlineBox::detach(RenderArena* renderArena)
{
#ifndef NDEBUG
    inInlineBoxDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inInlineBoxDetach = false;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* InlineBox::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}


void InlineBox::operator delete(void* ptr, size_t sz)
{
    assert(inInlineBoxDetach);

    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

int InlineFlowBox::marginLeft()
{
    if (!includeLeftEdge())
        return 0;
    
    RenderStyle* cstyle = object()->style();
    Length margin = cstyle->marginLeft();
    if (margin.type != Variable)
        return (margin.type == Fixed ? margin.value : object()->marginLeft());
    return 0;
}

int InlineFlowBox::marginRight()
{
    if (!includeRightEdge())
        return 0;
    
    RenderStyle* cstyle = object()->style();
    Length margin = cstyle->marginRight();
    if (margin.type != Variable)
        return (margin.type == Fixed ? margin.value : object()->marginRight());
    return 0;
}

int InlineFlowBox::marginBorderPaddingLeft()
{
    return marginLeft() + borderLeft() + paddingLeft();
}

int InlineFlowBox::marginBorderPaddingRight()
{
    return marginRight() + borderRight() + paddingRight();
}

int InlineFlowBox::getFlowSpacingWidth()
{
    int totWidth = marginBorderPaddingLeft() + marginBorderPaddingRight();
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->isInlineFlowBox())
            totWidth += static_cast<InlineFlowBox*>(curr)->getFlowSpacingWidth();
    }
    return totWidth;
}

bool InlineFlowBox::nextOnLineExists()
{
    if (!parent())
        return false;

    if (nextOnLine())
        return true;

    return parent()->nextOnLineExists();
}

bool InlineFlowBox::prevOnLineExists()
{
    if (!parent())
        return false;

    if (prevOnLine())
        return true;

    return parent()->prevOnLineExists();
}

bool InlineFlowBox::onEndChain(RenderObject* endObject)
{
    if (!endObject)
        return false;
    
    if (endObject == object())
        return true;

    RenderObject* curr = endObject;
    RenderObject* parent = curr->parent();
    while (parent && !parent->isRenderBlock()) {
        if (parent->lastChild() != curr)
            return false;
        curr = parent;
        parent = curr->parent();
    }

    return true;
}

void InlineFlowBox::determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject)
{
    // All boxes start off open.  They will not apply any margins/border/padding on
    // any side.
    bool includeLeftEdge = false;
    bool includeRightEdge = false;

    RenderFlow* flow = static_cast<RenderFlow*>(object());
    
    if (!flow->firstChild())
        includeLeftEdge = includeRightEdge = true; // Empty inlines never split across lines.
    else if (parent()) { // The root inline box never has borders/margins/padding.
        bool ltr = flow->style()->direction() == LTR;
        
        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line.
        if (!flow->firstLineBox()->isConstructed()) {
            if (ltr && flow->firstLineBox() == this)
                includeLeftEdge = true;
            else if (!ltr && flow->lastLineBox() == this)
                includeRightEdge = true;
        }
    
        // In order to determine if the inline ends on this line, we check three things:
        // (1) If we are the last line and we don't have a continuation(), then we can
        // close up.
        // (2) If the last line box for the flow has an object following it on the line (ltr,
        // reverse for rtl), then the inline has closed.
        // (3) The line may end on the inline.  If we are the last child (climbing up
        // the end object's chain), then we just closed as well.
        if (!flow->lastLineBox()->isConstructed()) {
            if (ltr) {
                if (!nextLineBox() &&
                    ((lastLine && !object()->continuation()) || nextOnLineExists()
                     || onEndChain(endObject)))
                    includeRightEdge = true;
            }
            else {
                if ((!prevLineBox() || !prevLineBox()->isConstructed()) &&
                    ((lastLine && !object()->continuation()) ||
                     prevOnLineExists() || onEndChain(endObject)))
                    includeLeftEdge = true;
            }
            
        }
    }

    setEdges(includeLeftEdge, includeRightEdge);

    // Recur into our children.
    for (InlineBox* currChild = firstChild(); currChild; currChild = currChild->nextOnLine()) {
        if (currChild->isInlineFlowBox()) {
            InlineFlowBox* currFlow = static_cast<InlineFlowBox*>(currChild);
            currFlow->determineSpacingForFlowBoxes(lastLine, endObject);
        }
    }
}

int InlineFlowBox::placeBoxesHorizontally(int x)
{
    // Set our x position.
    setXPos(x);

    int startX = x;
    x += borderLeft() + paddingLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            TextRun* text = static_cast<TextRun*>(curr);
            text->setXPos(x);
            x += text->width();
        }
        else {
            if (curr->object()->isInlineFlow()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                x += flow->marginLeft();
                x = flow->placeBoxesHorizontally(x);
                x += flow->marginRight();
            }
            else {
                x += curr->object()->marginLeft();
                curr->setXPos(x);
                x += curr->width() + curr->object()->marginRight();
            }
        }
    }

    x += borderRight() + paddingRight();
    setWidth(x-startX);
    return x;
}

void InlineFlowBox::verticallyAlignBoxes(int& heightOfBlock)
{
    int maxPositionTop = 0;
    int maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;

    // Figure out if we're in strict mode.
    RenderObject* curr = object();
    while (curr && !curr->element())
        curr = curr->container();
    bool strictMode = (curr && curr->element()->getDocument()->inStrictMode());
    
    computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);

    if (maxAscent + maxDescent < QMAX(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    int maxHeight = maxAscent + maxDescent;
    placeBoxesVertically(heightOfBlock, maxHeight, maxAscent, strictMode);
    heightOfBlock += maxHeight;
}

void InlineFlowBox::adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                              int maxPositionTop, int maxPositionBottom)
{
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        // The computed lineheight needs to be extended for the
        // positioned elements
        if (curr->yPos() == PositionTop || curr->yPos() == PositionBottom) {
            if (curr->yPos() == PositionTop) {
                if (maxAscent + maxDescent < curr->height())
                    maxDescent = curr->height() - maxAscent;
            }
            else {
                if (maxAscent + maxDescent < curr->height())
                    maxAscent = curr->height() - maxDescent;
            }

            if ( maxAscent + maxDescent >= QMAX( maxPositionTop, maxPositionBottom ) )
                break;
        }

        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);
    }
}

void InlineFlowBox::computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                             int& maxAscent, int& maxDescent, bool strictMode)
{
    if (!parent()) {
        // Examine our root box.
        setHeight(object()->lineHeight(m_firstLine));
        bool isTableCell = object()->isTableCell();
        if (isTableCell) {
            RenderTableCell* tableCell = static_cast<RenderTableCell*>(object());
            setBaseline(tableCell->RenderBlock::baselinePosition(m_firstLine));
        }
        else
            setBaseline(object()->baselinePosition(m_firstLine));
        if (hasTextChildren() || strictMode) {
            int ascent = baseline();
            int descent = height() - ascent;
            if (maxAscent < ascent)
                maxAscent = ascent;
            if (maxDescent < descent)
                maxDescent = descent;
        }
    }
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        curr->setHeight(curr->object()->lineHeight(m_firstLine));
        curr->setBaseline(curr->object()->baselinePosition(m_firstLine));
        curr->setYPos(curr->object()->verticalPositionHint(m_firstLine));
        if (curr->yPos() == PositionTop) {
            if (maxPositionTop < curr->height())
                maxPositionTop = curr->height();
        }
        else if (curr->yPos() == PositionBottom) {
            if (maxPositionBottom < curr->height())
                maxPositionBottom = curr->height();
        }
        else if (curr->hasTextChildren() || strictMode) {
            int ascent = curr->baseline() - curr->yPos();
            int descent = curr->height() - ascent;
            if (maxAscent < ascent)
                maxAscent = ascent;
            if (maxDescent < descent)
                maxDescent = descent;
        }

        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);
    }
}

void InlineFlowBox::placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode)
{
    if (!parent()) {
        if (hasTextChildren() || strictMode)
            setYPos(y + maxAscent - baseline());// Place our root box.
        // FIXME: Use the tallest y/height out of our children. 
    }
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->yPos() == PositionTop)
            curr->setYPos(y);
        else if (curr->yPos() == PositionBottom)
            curr->setYPos(y + maxHeight - curr->height());
        else if (curr->hasTextChildren() || strictMode)
            // FIXME: Use the tallest y/height out of our children. 
            curr->setYPos(curr->yPos() + y + maxAscent - curr->baseline());

        // FIXME: Adjust the y position and heights of replaced elements for margins.
        // Adjust inline flows to use the font box height and not the logical height.
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(y, maxHeight, maxAscent,
                                                                    strictMode);
    }
}


