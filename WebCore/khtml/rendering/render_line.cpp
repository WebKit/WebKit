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
            if (curr->object()->isPositioned()) {
                if (curr->object()->parent()->style()->direction() == LTR)
                    curr->setXPos(x);
                else {
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained by walking up to the root line box).
                    InlineBox* root = this;
                    while (!root->isRootInlineBox())
                        root = root->parent();
                    curr->setXPos(root->object()->width()-x);
                }
                continue; // The positioned object has no effect on the width.
            }
            if (curr->object()->isInlineFlow()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                if (curr->object()->isCompact()) {
                    int ignoredX = x;
                    flow->placeBoxesHorizontally(ignoredX);
                }
                else {
                    x += flow->marginLeft();
                    x = flow->placeBoxesHorizontally(x);
                    x += flow->marginRight();
                }
            }
            else if (!curr->object()->isCompact()) {
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

    // Figure out if we're in strict mode.  Note that we can't simply use !style()->htmlHacks(),
    // because that would match almost strict mode as well.
    RenderObject* curr = object();
    while (curr && !curr->element())
        curr = curr->container();
    bool strictMode = (curr && curr->element()->getDocument()->inStrictMode());
    
    computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);

    if (maxAscent + maxDescent < QMAX(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    int maxHeight = maxAscent + maxDescent;
    int topPosition = heightOfBlock;
    int bottomPosition = heightOfBlock;
    placeBoxesVertically(heightOfBlock, maxHeight, maxAscent, strictMode, topPosition, bottomPosition);

    setOverflowPositions(topPosition, bottomPosition);

    // Shrink boxes with no text children in quirks and almost strict mode.
    if (!strictMode)
        shrinkBoxesWithNoTextChildren(topPosition, bottomPosition);
    
    heightOfBlock += maxHeight;
}

void InlineFlowBox::adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                              int maxPositionTop, int maxPositionBottom)
{
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        // The computed lineheight needs to be extended for the
        // positioned elements
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
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
    if (isRootInlineBox()) {
        // Examine our root box.
        setHeight(object()->lineHeight(m_firstLine, true));
        bool isTableCell = object()->isTableCell();
        if (isTableCell) {
            RenderTableCell* tableCell = static_cast<RenderTableCell*>(object());
            setBaseline(tableCell->RenderBlock::baselinePosition(m_firstLine, true));
        }
        else
            setBaseline(object()->baselinePosition(m_firstLine, true));
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
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
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

void InlineFlowBox::placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode,
                                         int& topPosition, int& bottomPosition)
{
    if (isRootInlineBox())
        setYPos(y + maxAscent - baseline());// Place our root box.
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(y, maxHeight, maxAscent, strictMode,
                                                                    topPosition, bottomPosition);

        bool childAffectsTopBottomPos = true;
        if (curr->yPos() == PositionTop)
            curr->setYPos(y);
        else if (curr->yPos() == PositionBottom)
            curr->setYPos(y + maxHeight - curr->height());
        else {
            if (!curr->hasTextChildren() && !strictMode)
                childAffectsTopBottomPos = false;
            curr->setYPos(curr->yPos() + y + maxAscent - curr->baseline());
        }
        
        int newY = curr->yPos();
        int newHeight = curr->height();
        int newBaseline = curr->baseline();
        if (curr->isTextRun() || curr->isInlineFlowBox()) {
            const QFontMetrics &fm = curr->object()->fontMetrics( m_firstLine );
            newBaseline = fm.ascent();
            newY += curr->baseline() - newBaseline;
            newHeight = newBaseline+fm.descent();
            if (curr->isInlineFlowBox()) {
                newHeight += curr->object()->borderTop() + curr->object()->paddingTop() +
                            curr->object()->borderBottom() + curr->object()->paddingBottom();
                newY -= curr->object()->borderTop() + curr->object()->paddingTop();
                newBaseline += curr->object()->borderTop() + curr->object()->paddingTop();
            }	
        }
        else {
            newY += curr->object()->marginTop();
            newHeight = curr->height() - (curr->object()->marginTop() + curr->object()->marginBottom());
        }

        curr->setYPos(newY);
        curr->setHeight(newHeight);
        curr->setBaseline(newBaseline);

        if (childAffectsTopBottomPos) {
            if (newY < topPosition)
                topPosition = newY;
            if (newY + newHeight > bottomPosition)
                bottomPosition = newY + newHeight;
        }
    }

    if (isRootInlineBox()) {
        const QFontMetrics &fm = object()->fontMetrics( m_firstLine );
        setHeight(fm.ascent()+fm.descent());
        setYPos(yPos() + baseline() - fm.ascent());
        setBaseline(fm.ascent());
        if (hasTextChildren() || strictMode) {
            if (yPos() < topPosition)
                topPosition = yPos();
            if (yPos() + height() > bottomPosition)
                bottomPosition = yPos() + height();
        }
    }
}

void InlineFlowBox::shrinkBoxesWithNoTextChildren(int topPos, int bottomPos)
{
    // First shrink our kids.
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->shrinkBoxesWithNoTextChildren(topPos, bottomPos);
    }

    // See if we have text children. If not, then we need to shrink ourselves to fit on the line.
    if (!hasTextChildren()) {
        if (yPos() < topPos)
            setYPos(topPos);
        if (yPos() + height() > bottomPos)
            setHeight(bottomPos - yPos());
        if (baseline() > height())
            setBaseline(height());
    }
}

void InlineFlowBox::paintBackgroundAndBorder(QPainter *p, int _x, int _y,
                                             int _w, int _h, int _tx, int _ty, int xOffsetOnLine)
{
    // Move x/y to our coordinates.
    _tx += m_x;
    _ty += m_y;
    
    int w = width();
    int h = height();

    int my = QMAX(_ty,_y);
    int mh;
    if (_ty<_y)
        mh= QMAX(0,h-(_y-_ty));
    else
        mh = QMIN(_h,h);

    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    if (object()->hasFirstLine() || (parent() && object()->shouldPaintBackgroundOrBorder())) {
        CachedImage* bg = styleToUse->backgroundImage();
        bool hasBackgroundImage = bg && (bg->pixmap_size() == bg->valid_rect().size()) &&
                                  !bg->isTransparent() && !bg->isErrorImage();
        if (!hasBackgroundImage || (!prevLineBox() && !nextLineBox()) || !parent())
            object()->paintBackgroundExtended(p, styleToUse->backgroundColor(),
                                              bg, my, mh, _tx, _ty, w, h,
                                              borderLeft(), borderRight());
        else {
            // We have a background image that spans multiple lines.
            // We need to adjust _tx and _ty by the width of all previous lines.
            // Think of background painting on inlines as though you had one long line, a single continuous
            // strip.  Even though that strip has been broken up across multiple lines, you still paint it
            // as though you had one single line.  This means each line has to pick up the background where
            // the previous line left off.
            int startX = _tx - xOffsetOnLine;
            int totalWidth = xOffsetOnLine;
            for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
                totalWidth += curr->width();
            QRect clipRect(_tx, _ty, width(), height());
            clipRect = p->xForm(clipRect);
            p->save();
            p->addClip(clipRect);
            object()->paintBackgroundExtended(p, object()->style()->backgroundColor(),
                                              object()->style()->backgroundImage(), my, mh, startX, _ty,
                                              totalWidth, h,
                                              borderLeft(), borderRight());
            p->restore();
        }

        // :first-line cannot be used to put borders on a line. Always paint borders with our
        // non-first-line style.
        if (parent() && object()->style()->hasBorder())
            object()->paintBorder(p, _tx, _ty, w, h, object()->style(), includeLeftEdge(), includeRightEdge());
    }
}

static bool shouldDrawDecoration(RenderObject* obj)
{
    bool shouldDraw = false;
    for (RenderObject* curr = obj->firstChild();
         curr; curr = curr->nextSibling()) {
        if (curr->isInlineFlow()) {
            shouldDraw = true;
            break;
        }
        else if (curr->isText() && !curr->isBR() && (curr->style()->whiteSpace() == PRE ||
                 !curr->element() || !curr->element()->containsOnlyWhitespace())) {
            shouldDraw = true;
            break;
        }	
    }
    return shouldDraw;
}

void InlineFlowBox::paintDecorations(QPainter *p, int _x, int _y,
                                     int _w, int _h, int _tx, int _ty)
{
    // Now paint our text decorations. We only do this if we aren't in quirks mode (i.e., in
    // almost-strict mode or strict mode).
    _tx += m_x;
    _ty += m_y;
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int deco = parent() ? styleToUse->textDecoration() : styleToUse->textDecorationsInEffect();
    if (deco != TDNONE && shouldDrawDecoration(object())) {
#if APPLE_CHANGES
        // Set up the appropriate text-shadow effect for the decoration.
        // FIXME: Support multiple shadow effects.  Need more from the CG API before we can do this.
        bool setShadow = false;
        if (styleToUse->textShadow()) {
            p->setShadow(styleToUse->textShadow()->x, styleToUse->textShadow()->y,
                         styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
            setShadow = true;
        }
#endif
        
        // We must have child boxes and have decorations defined.
        _tx += borderLeft() + paddingLeft();
        int w = m_width - (borderLeft() + paddingLeft() + borderRight() + paddingRight());
        QColor underline, overline, linethrough;
        underline = overline = linethrough = styleToUse->color();
        if (!parent())
            object()->getTextDecorationColors(deco, underline, overline, linethrough);
        if (deco & UNDERLINE) {
            p->setPen(underline);
            p->drawLineForText(_tx, _ty, m_baseline, w);
        }
        if (deco & OVERLINE) {
            p->setPen(overline);
            p->drawLineForText(_tx, _ty, 0, w);
        }
        if (deco & LINE_THROUGH) {
            p->setPen(linethrough);
            p->drawLineForText(_tx, _ty, 2*m_baseline/3, w);
        }

#if APPLE_CHANGES
        if (setShadow)
            p->clearShadow();
#endif
    }
}
