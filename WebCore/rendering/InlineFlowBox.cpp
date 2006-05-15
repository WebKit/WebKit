/**
* This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "InlineFlowBox.h"

#include "CachedImage.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "RootInlineBox.h"
#include "RenderBlock.h"
#include "RenderFlow.h"
#include "RenderListMarker.h"
#include "RenderTableCell.h"

using namespace std;

namespace WebCore {

RenderFlow* InlineFlowBox::flowObject()
{
    return static_cast<RenderFlow*>(m_object);
}

int InlineFlowBox::marginLeft()
{
    if (!includeLeftEdge())
        return 0;
    
    Length margin = object()->style()->marginLeft();
    if (margin.isAuto())
        return 0;
    if (margin.isFixed())
        return margin.value();
    return object()->marginLeft();
}

int InlineFlowBox::marginRight()
{
    if (!includeRightEdge())
        return 0;
    
    Length margin = object()->style()->marginRight();
    if (margin.isAuto())
        return 0;
    if (margin.isFixed())
        return margin.value();
    return object()->marginRight();
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

void InlineFlowBox::addToLine(InlineBox* child) {
    if (!m_firstChild)
        m_firstChild = m_lastChild = child;
    else {
        m_lastChild->m_next = child;
        child->m_prev = m_lastChild;
        m_lastChild = child;
    }
    child->setFirstLineStyleBit(m_firstLine);
    child->setParent(this);
    if (child->isText())
        m_hasTextChildren = true;
    if (child->object()->selectionState() != RenderObject::SelectionNone)
        root()->setHasSelectedChildren(true);
}

void InlineFlowBox::removeChild(InlineBox* child)
{
    if (!m_dirty)
        dirtyLineBoxes();

    root()->childRemoved(child);

    if (child == m_firstChild)
        m_firstChild = child->nextOnLine();
    if (child == m_lastChild)
        m_lastChild = child->prevOnLine();
    if (child->nextOnLine())
        child->nextOnLine()->setPrevOnLine(child->prevOnLine());
    if (child->prevOnLine())
        child->prevOnLine()->setNextOnLine(child->nextOnLine());
    
    child->setParent(0);
}

void InlineFlowBox::deleteLine(RenderArena* arena)
{
    InlineBox* child = m_firstChild;
    InlineBox* next = 0;
    while (child) {
        next = child->nextOnLine();
        child->deleteLine(arena);
        child = next;
    }
    
    static_cast<RenderFlow*>(m_object)->removeLineBox(this);
    destroy(arena);
}

void InlineFlowBox::extractLine()
{
    if (!m_extracted)
        static_cast<RenderFlow*>(m_object)->extractLineBox(this);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->extractLine();
}

void InlineFlowBox::attachLine()
{
    if (m_extracted)
        static_cast<RenderFlow*>(m_object)->attachLineBox(this);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->attachLine();
}

void InlineFlowBox::adjustPosition(int dx, int dy)
{
    InlineRunBox::adjustPosition(dx, dy);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
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
        if (parent->lastChild() != curr || parent == object())
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
                    ((lastLine && !object()->continuation()) || nextOnLineExists() || onEndChain(endObject)))
                    includeRightEdge = true;
            } else {
                if ((!prevLineBox() || prevLineBox()->isConstructed()) &&
                    ((lastLine && !object()->continuation()) || prevOnLineExists() || onEndChain(endObject)))
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

int InlineFlowBox::placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool& needsWordSpacing)
{
    // Set our x position.
    setXPos(x);
    leftPosition = min(x, leftPosition);

    int startX = x;
    x += borderLeft() + paddingLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            InlineTextBox* text = static_cast<InlineTextBox*>(curr);
            RenderText* rt = static_cast<RenderText*>(text->object());
            if (rt->length()) {
                if (needsWordSpacing && QChar(rt->text()[text->start()]).isSpace())
                    x += rt->font(m_firstLine)->wordSpacing();
                needsWordSpacing = !QChar(rt->text()[text->end()]).isSpace();
            }
            text->setXPos(x);
            int shadowLeft = 0;
            int shadowRight = 0;
            for (ShadowData* shadow = rt->style()->textShadow(); shadow; shadow = shadow->next) {
                shadowLeft = min(shadowLeft, shadow->x - shadow->blur);
                shadowRight = max(shadowRight, shadow->x + shadow->blur);
            }
            leftPosition = min(x + shadowLeft, leftPosition);
            rightPosition = max(x + text->width() + shadowRight, rightPosition);
            m_maxHorizontalShadow = max(max(shadowRight, -shadowLeft), m_maxHorizontalShadow);
            x += text->width();
        } else {
            if (curr->object()->isPositioned()) {
                if (curr->object()->parent()->style()->direction() == LTR)
                    curr->setXPos(x);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    curr->setXPos(root()->object()->width()-x);
                continue; // The positioned object has no effect on the width.
            }
            if (curr->object()->isInlineFlow()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                if (curr->object()->isCompact()) {
                    int ignoredX = x;
                    flow->placeBoxesHorizontally(ignoredX, leftPosition, rightPosition, needsWordSpacing);
                } else {
                    x += flow->marginLeft();
                    x = flow->placeBoxesHorizontally(x, leftPosition, rightPosition, needsWordSpacing);
                    x += flow->marginRight();
                }
            } else if (!curr->object()->isCompact() && (!curr->object()->isListMarker() || static_cast<RenderListMarker*>(curr->object())->isInside())) {
                x += curr->object()->marginLeft();
                curr->setXPos(x);
                leftPosition = min(x, leftPosition);
                rightPosition = max(x + curr->width(), rightPosition);
                x += curr->width() + curr->object()->marginRight();
            }
        }
    }

    x += borderRight() + paddingRight();
    setWidth(x-startX);
    rightPosition = max(xPos() + width(), rightPosition);

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
    bool strictMode = (curr && curr->document()->inStrictMode());
    
    computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);

    if (maxAscent + maxDescent < max(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    int maxHeight = maxAscent + maxDescent;
    int topPosition = heightOfBlock;
    int bottomPosition = heightOfBlock;
    int selectionTop = heightOfBlock;
    int selectionBottom = heightOfBlock;
    placeBoxesVertically(heightOfBlock, maxHeight, maxAscent, strictMode, topPosition, bottomPosition, selectionTop, selectionBottom);

    setVerticalOverflowPositions(topPosition, bottomPosition);
    setVerticalSelectionPositions(selectionTop, selectionBottom);

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

            if (maxAscent + maxDescent >= max(maxPositionTop, maxPositionBottom))
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
                                         int& topPosition, int& bottomPosition, int& selectionTop, int& selectionBottom)
{
    if (isRootInlineBox())
        setYPos(y + maxAscent - baseline());// Place our root box.
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(y, maxHeight, maxAscent, strictMode, topPosition, bottomPosition, selectionTop, selectionBottom);

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
        int overflowTop = 0;
        int overflowBottom = 0;
        if (curr->isText() || curr->isInlineFlowBox()) {
            const Font& font = curr->object()->font(m_firstLine);
            newBaseline = font.ascent();
            newY += curr->baseline() - newBaseline;
            newHeight = newBaseline + font.descent();
            for (ShadowData* shadow = curr->object()->style()->textShadow(); shadow; shadow = shadow->next) {
                overflowTop = min(overflowTop, shadow->y - shadow->blur);
                overflowBottom = max(overflowBottom, shadow->y + shadow->blur);
            }
            if (curr->isInlineFlowBox()) {
                newHeight += curr->object()->borderTop() + curr->object()->paddingTop() +
                            curr->object()->borderBottom() + curr->object()->paddingBottom();
                newY -= curr->object()->borderTop() + curr->object()->paddingTop();
                newBaseline += curr->object()->borderTop() + curr->object()->paddingTop();
            }
        }
        else if (!curr->object()->isBR()) {
            newY += curr->object()->marginTop();
            newHeight = curr->height() - (curr->object()->marginTop() + curr->object()->marginBottom());
            overflowTop = curr->object()->overflowTop(false);
            overflowBottom = curr->object()->overflowHeight(false) - newHeight;
        }

        curr->setYPos(newY);
        curr->setHeight(newHeight);
        curr->setBaseline(newBaseline);

        if (childAffectsTopBottomPos) {
            selectionTop = min(selectionTop, newY);
            selectionBottom = max(selectionBottom, newY + newHeight);
            topPosition = min(topPosition, newY + overflowTop);
            bottomPosition = max(bottomPosition, newY + newHeight + overflowBottom);
        }
    }

    if (isRootInlineBox()) {
        const Font& font = object()->font(m_firstLine);
        setHeight(font.ascent() + font.descent());
        setYPos(yPos() + baseline() - font.ascent());
        setBaseline(font.ascent());
        if (hasTextChildren() || strictMode) {
            selectionTop = min(selectionTop, yPos());
            selectionBottom = max(selectionBottom, yPos() + height());
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

bool InlineFlowBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    // Check children first.
    for (InlineBox* curr = lastChild(); curr; curr = curr->prevOnLine()) {
        if (!curr->object()->layer() && curr->nodeAtPoint(i, x, y, tx, ty)) {
            object()->setInnerNode(i);
            return true;
        }
    }

    // Now check ourselves.
    IntRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->setInnerNode(i);
        return true;
    }
    
    return false;
}

void InlineFlowBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    int xPos = tx + m_x - object()->maximalOutlineSize(i.phase);
    int w = width() + 2 * object()->maximalOutlineSize(i.phase);
    bool intersectsDamageRect = xPos < i.r.right() && xPos + w > i.r.x();
    
    if (intersectsDamageRect && i.phase != PaintPhaseChildOutlines) {
        if (i.phase == PaintPhaseOutline || i.phase == PaintPhaseSelfOutline) {
            // Add ourselves to the paint info struct's list of inlines that need to paint their
            // outlines.
            if (object()->style()->visibility() == VISIBLE && object()->style()->outlineWidth() > 0 &&
                !object()->isInlineContinuation() && !isRootInlineBox()) {
                i.outlineObjects->add(flowObject());
            }
        }
        else {
            // 1. Paint our background and border.
            paintBackgroundAndBorder(i, tx, ty);
            
            // 2. Paint our underline and overline.
            paintDecorations(i, tx, ty, false);
        }
    }

    PaintPhase paintPhase = i.phase == PaintPhaseChildOutlines ? PaintPhaseOutline : i.phase;
    RenderObject::PaintInfo childInfo(i);
    childInfo.phase = paintPhase;

    // 3. Paint our children.
    if (paintPhase != PaintPhaseSelfOutline) {
        for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (!curr->object()->layer())
                curr->paint(childInfo, tx, ty);
        }
    }

    // 4. Paint our strike-through
    if (intersectsDamageRect && (i.phase == PaintPhaseForeground || i.phase == PaintPhaseSelection))
        paintDecorations(i, tx, ty, true);
}

void InlineFlowBox::paintBackgrounds(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer,
                                     int my, int mh, int _tx, int _ty, int w, int h)
{
    if (!bgLayer)
        return;
    paintBackgrounds(p, c, bgLayer->next(), my, mh, _tx, _ty, w, h);
    paintBackground(p, c, bgLayer, my, mh, _tx, _ty, w, h);
}

void InlineFlowBox::paintBackground(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer,
                                    int my, int mh, int _tx, int _ty, int w, int h)
{
    CachedImage* bg = bgLayer->backgroundImage();
    bool hasBackgroundImage = bg && bg->canRender();
    if (!hasBackgroundImage || (!prevLineBox() && !nextLineBox()) || !parent())
        object()->paintBackgroundExtended(p, c, bgLayer, my, mh, _tx, _ty, w, h, 
                                          borderLeft(), borderRight(), paddingLeft(), paddingRight());
    else {
        // We have a background image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        // Think of background painting on inlines as though you had one long line, a single continuous
        // strip.  Even though that strip has been broken up across multiple lines, you still paint it
        // as though you had one single line.  This means each line has to pick up the background where
        // the previous line left off.
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        int xOffsetOnLine = 0;
        for (InlineRunBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            xOffsetOnLine += curr->width();
        int startX = _tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
            totalWidth += curr->width();
        p->save();
        p->addClip(IntRect(_tx, _ty, width(), height()));
        object()->paintBackgroundExtended(p, c, bgLayer, my, mh, startX, _ty,
                                          totalWidth, h, borderLeft(), borderRight(), paddingLeft(), paddingRight());
        p->restore();
    }
}

void InlineFlowBox::paintBackgroundAndBorder(RenderObject::PaintInfo& i, int _tx, int _ty)
{
    if (!object()->shouldPaintWithinRoot(i) || object()->style()->visibility() != VISIBLE ||
        i.phase != PaintPhaseForeground)
        return;

    // Move x/y to our coordinates.
    _tx += m_x;
    _ty += m_y;
    
    int w = width();
    int h = height();

    int my = max(_ty, i.r.y());
    int mh;
    if (_ty < i.r.y())
        mh = max(0, h - (i.r.y() - _ty));
    else
        mh = min(i.r.height(), h);

    GraphicsContext* p = i.p;
    
    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    if ((!parent() && m_firstLine && styleToUse != object()->style()) || 
        (parent() && object()->shouldPaintBackgroundOrBorder())) {
        Color c = styleToUse->backgroundColor();
        paintBackgrounds(p, c, styleToUse->backgroundLayers(), my, mh, _tx, _ty, w, h);

        // :first-line cannot be used to put borders on a line. Always paint borders with our
        // non-first-line style.
        if (parent() && object()->style()->hasBorder()) {
            CachedImage* borderImage = object()->style()->borderImage().image();
            bool hasBorderImage = borderImage && borderImage->canRender();
            if (hasBorderImage && !borderImage->isLoaded())
                return; // Don't paint anything while we wait for the image to load.
            
            // The simple case is where we either have no border image or we are the only box for this object.  In those
            // cases only a single call to draw is required.
            if (!hasBorderImage || (!prevLineBox() && !nextLineBox()))
                object()->paintBorder(p, _tx, _ty, w, h, object()->style(), includeLeftEdge(), includeRightEdge());
            else {
                // We have a border image that spans multiple lines.
                // We need to adjust _tx and _ty by the width of all previous lines.
                // Think of border image painting on inlines as though you had one long line, a single continuous
                // strip.  Even though that strip has been broken up across multiple lines, you still paint it
                // as though you had one single line.  This means each line has to pick up the image where
                // the previous line left off.
                // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
                // but it isn't even clear how this should work at all.
                int xOffsetOnLine = 0;
                for (InlineRunBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
                    xOffsetOnLine += curr->width();
                int startX = _tx - xOffsetOnLine;
                int totalWidth = xOffsetOnLine;
                for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
                    totalWidth += curr->width();
                p->save();
                p->addClip(IntRect(_tx, _ty, width(), height()));
                object()->paintBorder(p, startX, _ty, totalWidth, h, object()->style());
                p->restore();
            }
        }
    }
}

static bool shouldDrawDecoration(RenderObject* obj)
{
    for (RenderObject* curr = obj->firstChild(); curr; curr = curr->nextSibling()) {
        if (curr->isInlineFlow())
            return true;
        if (curr->isText() && !curr->isBR()) {
            if (!curr->style()->collapseWhiteSpace())
                return true;
            Node* currElement = curr->element();
            if (!currElement)
                return true;
            if (!currElement->isTextNode())
                return true;
            if (!static_cast<Text*>(currElement)->containsOnlyWhitespace())
                return true;
        }
    }
    return false;
}

void InlineFlowBox::paintDecorations(RenderObject::PaintInfo& i, int _tx, int _ty, bool paintedChildren)
{
    // Paint text decorations like underlines/overlines. We only do this if we aren't in quirks mode (i.e., in
    // almost-strict mode or strict mode).
    if (object()->style()->htmlHacks() || !object()->shouldPaintWithinRoot(i) ||
        object()->style()->visibility() != VISIBLE)
        return;

    GraphicsContext* p = i.p;
    _tx += m_x;
    _ty += m_y;
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int deco = parent() ? styleToUse->textDecoration() : styleToUse->textDecorationsInEffect();
    if (deco != TDNONE && 
        ((!paintedChildren && ((deco & UNDERLINE) || (deco & OVERLINE))) || (paintedChildren && (deco & LINE_THROUGH))) &&
        shouldDrawDecoration(object())) {
        int x = m_x + borderLeft() + paddingLeft();
        int w = m_width - (borderLeft() + paddingLeft() + borderRight() + paddingRight());
        RootInlineBox* rootLine = root();
        if (rootLine->ellipsisBox()) {
            int ellipsisX = rootLine->ellipsisBox()->xPos();
            int ellipsisWidth = rootLine->ellipsisBox()->width();
            
            // FIXME: Will need to work with RTL
            if (rootLine == this) {
                if (x + w >= ellipsisX + ellipsisWidth)
                    w -= (x + w - ellipsisX - ellipsisWidth);
            }
            else {
                if (x >= ellipsisX)
                    return;
                if (x + w >= ellipsisX)
                    w -= (x + w - ellipsisX);
            }
        }
            
        // Set up the appropriate text-shadow effect for the decoration.
        // FIXME: Support multiple shadow effects.  Need more from the CG API before we can do this.
        bool setShadow = false;
        if (styleToUse->textShadow()) {
            p->setShadow(IntSize(styleToUse->textShadow()->x, styleToUse->textShadow()->y),
                         styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
            setShadow = true;
        }
        
        // We must have child boxes and have decorations defined.
        _tx += borderLeft() + paddingLeft();
        
        Color underline, overline, linethrough;
        underline = overline = linethrough = styleToUse->color();
        if (!parent())
            object()->getTextDecorationColors(deco, underline, overline, linethrough);

        if (styleToUse->font() != p->font())
            p->setFont(styleToUse->font());

        bool isPrinting = object()->document()->printing();
        if (deco & UNDERLINE && !paintedChildren) {
            p->setPen(underline);
            p->drawLineForText(IntPoint(_tx, _ty), m_baseline, w, isPrinting);
        }
        if (deco & OVERLINE && !paintedChildren) {
            p->setPen(overline);
            p->drawLineForText(IntPoint(_tx, _ty), 0, w, isPrinting);
        }
        if (deco & LINE_THROUGH && paintedChildren) {
            p->setPen(linethrough);
            p->drawLineForText(IntPoint(_tx, _ty), 2*m_baseline/3, w, isPrinting);
        }

        if (setShadow)
            p->clearShadow();
    }
}

InlineBox* InlineFlowBox::firstLeafChild()
{
    return firstLeafChildAfterBox();
}

InlineBox* InlineFlowBox::lastLeafChild()
{
    return lastLeafChildBeforeBox();
}

InlineBox* InlineFlowBox::firstLeafChildAfterBox(InlineBox* start)
{
    InlineBox* leaf = 0;
    for (InlineBox* box = start ? start->nextOnLine() : firstChild(); box && !leaf; box = box->nextOnLine())
        leaf = box->firstLeafChild();
    if (start && !leaf && parent())
        return parent()->firstLeafChildAfterBox(this);
    return leaf;
}

InlineBox* InlineFlowBox::lastLeafChildBeforeBox(InlineBox* start)
{
    InlineBox* leaf = 0;
    for (InlineBox* box = start ? start->prevOnLine() : lastChild(); box && !leaf; box = box->prevOnLine())
        leaf = box->lastLeafChild();
    if (start && !leaf && parent())
        return parent()->lastLeafChildBeforeBox(this);
    return leaf;
}

RenderObject::SelectionState InlineFlowBox::selectionState()
{
    return RenderObject::SelectionNone;
}

bool InlineFlowBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth)
{
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine()) {
        if (!box->canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth))
            return false;
    }
    return true;
}

int InlineFlowBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    int result = -1;
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine()) {
        int currResult = box->placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
        if (currResult != -1 && result == -1)
            result = currResult;
    }
    return result;
}

void InlineFlowBox::clearTruncation()
{
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine())
        box->clearTruncation();
}

} // namespace
