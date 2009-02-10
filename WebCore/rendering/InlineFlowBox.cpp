/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "InlineFlowBox.h"

#include "CachedImage.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "HitTestResult.h"
#include "RootInlineBox.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderListMarker.h"
#include "RenderTableCell.h"
#include "RootInlineBox.h"
#include "Text.h"

#include <math.h>

using namespace std;

namespace WebCore {

#ifndef NDEBUG

InlineFlowBox::~InlineFlowBox()
{
    if (!m_hasBadChildList)
        for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
            child->setHasBadParent();
}

#endif

int InlineFlowBox::height() const
{
    const Font& font = m_object->style(m_firstLine)->font();
    int result = font.height();
    bool strictMode = m_object->document()->inStrictMode();
    RenderBoxModelObject* box = boxModelObject();
    result += box->borderTop() + box->paddingTop() + box->borderBottom() + box->paddingBottom();
    if (!strictMode && !hasTextChildren() && !box->hasHorizontalBordersOrPadding()) {
        int bottomOverflow = root()->bottomOverflow();
        if (yPos() + result > bottomOverflow)
            result = bottomOverflow - yPos();
    }
    return result;
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

void InlineFlowBox::addToLine(InlineBox* child) 
{
    ASSERT(!child->parent());
    ASSERT(!child->nextOnLine());
    ASSERT(!child->prevOnLine());
    checkConsistency();

    child->setParent(this);
    if (!m_firstChild) {
        m_firstChild = child;
        m_lastChild = child;
    } else {
        m_lastChild->setNextOnLine(child);
        child->setPrevOnLine(m_lastChild);
        m_lastChild = child;
    }
    child->setFirstLineStyleBit(m_firstLine);
    if (child->isText())
        m_hasTextChildren = true;
    if (child->object()->selectionState() != RenderObject::SelectionNone)
        root()->setHasSelectedChildren(true);

    checkConsistency();
}

void InlineFlowBox::removeChild(InlineBox* child)
{
    checkConsistency();

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

    checkConsistency();
}

void InlineFlowBox::deleteLine(RenderArena* arena)
{
    InlineBox* child = firstChild();
    InlineBox* next = 0;
    while (child) {
        ASSERT(this == child->parent());
        next = child->nextOnLine();
#ifndef NDEBUG
        child->setParent(0);
#endif
        child->deleteLine(arena);
        child = next;
    }
#ifndef NDEBUG
    m_firstChild = 0;
    m_lastChild = 0;
#endif

    removeLineBoxFromRenderObject();
    destroy(arena);
}

void InlineFlowBox::removeLineBoxFromRenderObject()
{
    toRenderInline(m_object)->lineBoxes()->removeLineBox(this);
}

void InlineFlowBox::extractLine()
{
    if (!m_extracted)
        extractLineBoxFromRenderObject();
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->extractLine();
}

void InlineFlowBox::extractLineBoxFromRenderObject()
{
    toRenderInline(m_object)->lineBoxes()->extractLineBox(this);
}

void InlineFlowBox::attachLine()
{
    if (m_extracted)
        attachLineBoxToRenderObject();
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->attachLine();
}

void InlineFlowBox::attachLineBoxToRenderObject()
{
    toRenderInline(m_object)->lineBoxes()->attachLineBox(this);
}

void InlineFlowBox::adjustPosition(int dx, int dy)
{
    InlineRunBox::adjustPosition(dx, dy);
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
}

RenderLineBoxList* InlineFlowBox::rendererLineBoxes() const
{
    return toRenderInline(object())->lineBoxes();
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
    
    if (!object()->firstChild())
        includeLeftEdge = includeRightEdge = true; // Empty inlines never split across lines.
    else if (parent()) { // The root inline box never has borders/margins/padding.
        bool ltr = object()->style()->direction() == LTR;
        
        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line.
        RenderLineBoxList* lineBoxList = rendererLineBoxes();
        if (!lineBoxList->firstLineBox()->isConstructed()) {
            if (ltr && lineBoxList->firstLineBox() == this)
                includeLeftEdge = true;
            else if (!ltr && lineBoxList->lastLineBox() == this)
                includeRightEdge = true;
        }
    
        // In order to determine if the inline ends on this line, we check three things:
        // (1) If we are the last line and we don't have a continuation(), then we can
        // close up.
        // (2) If the last line box for the flow has an object following it on the line (ltr,
        // reverse for rtl), then the inline has closed.
        // (3) The line may end on the inline.  If we are the last child (climbing up
        // the end object's chain), then we just closed as well.
        if (!lineBoxList->lastLineBox()->isConstructed()) {
            RenderInline* inlineFlow = toRenderInline(object());
            if (ltr) {
                if (!nextLineBox() &&
                    ((lastLine && !inlineFlow->continuation()) || nextOnLineExists() || onEndChain(endObject)))
                    includeRightEdge = true;
            } else {
                if ((!prevLineBox() || prevLineBox()->isConstructed()) &&
                    ((lastLine && !inlineFlow->continuation()) || prevOnLineExists() || onEndChain(endObject)))
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

    int boxShadowLeft = 0;
    int boxShadowRight = 0;
    for (ShadowData* boxShadow = object()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
        boxShadowLeft = min(boxShadow->x - boxShadow->blur, boxShadowLeft);
        boxShadowRight = max(boxShadow->x + boxShadow->blur, boxShadowRight);
    }
    leftPosition = min(x + boxShadowLeft, leftPosition);

    int startX = x;
    x += borderLeft() + paddingLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            InlineTextBox* text = static_cast<InlineTextBox*>(curr);
            RenderText* rt = toRenderText(text->object());
            if (rt->textLength()) {
                if (needsWordSpacing && isSpaceOrNewline(rt->characters()[text->start()]))
                    x += rt->style(m_firstLine)->font().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt->characters()[text->end()]);
            }
            text->setXPos(x);
            
            int strokeOverflow = static_cast<int>(ceilf(rt->style()->textStrokeWidth() / 2.0f));
            
            // If letter-spacing is negative, we should factor that into right overflow. (Even in RTL, letter-spacing is
            // applied to the right, so this is not an issue with left overflow.
            int letterSpacing = min(0, (int)rt->style(m_firstLine)->font().letterSpacing());
            
            int leftGlyphOverflow = -strokeOverflow;
            int rightGlyphOverflow = strokeOverflow - letterSpacing;
            
            int visualOverflowLeft = leftGlyphOverflow;
            int visualOverflowRight = rightGlyphOverflow;
            for (ShadowData* shadow = rt->style()->textShadow(); shadow; shadow = shadow->next) {
                visualOverflowLeft = min(visualOverflowLeft, shadow->x - shadow->blur + leftGlyphOverflow);
                visualOverflowRight = max(visualOverflowRight, shadow->x + shadow->blur + rightGlyphOverflow);
            }
            
            leftPosition = min(x + visualOverflowLeft, leftPosition);
            rightPosition = max(x + text->width() + visualOverflowRight, rightPosition);
            m_maxHorizontalVisualOverflow = max(max(visualOverflowRight, -visualOverflowLeft), (int)m_maxHorizontalVisualOverflow);
            x += text->width();
        } else {
            if (curr->object()->isPositioned()) {
                if (curr->object()->parent()->style()->direction() == LTR)
                    curr->setXPos(x);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    curr->setXPos(root()->block()->width()-x);
                continue; // The positioned object has no effect on the width.
            }
            if (curr->object()->isRenderInline()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                x += flow->marginLeft();
                x = flow->placeBoxesHorizontally(x, leftPosition, rightPosition, needsWordSpacing);
                x += flow->marginRight();
            } else if (!curr->object()->isListMarker() || static_cast<RenderListMarker*>(curr->object())->isInside()) {
                x += curr->boxModelObject()->marginLeft();
                curr->setXPos(x);
                leftPosition = min(x + toRenderBox(curr->object())->overflowLeft(false), leftPosition);
                rightPosition = max(x + toRenderBox(curr->object())->overflowWidth(false), rightPosition);
                x += curr->width() + curr->boxModelObject()->marginRight();
            }
        }
    }

    x += borderRight() + paddingRight();
    setWidth(x - startX);
    rightPosition = max(xPos() + width() + boxShadowRight, rightPosition);

    return x;
}

int InlineFlowBox::verticallyAlignBoxes(int heightOfBlock)
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
    
    heightOfBlock += maxHeight;
    
    return heightOfBlock;
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
            int lineHeight = curr->object()->lineHeight(m_firstLine);
            if (curr->yPos() == PositionTop) {
                if (maxAscent + maxDescent < lineHeight)
                    maxDescent = lineHeight - maxAscent;
            }
            else {
                if (maxAscent + maxDescent < lineHeight)
                    maxAscent = lineHeight - maxDescent;
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
        int lineHeight = object()->lineHeight(m_firstLine, true);
        int baseline = object()->baselinePosition(m_firstLine, true);
        if (hasTextChildren() || strictMode) {
            int ascent = baseline;
            int descent = lineHeight - ascent;
            if (maxAscent < ascent)
                maxAscent = ascent;
            if (maxDescent < descent)
                maxDescent = descent;
        }
    }
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        bool isInlineFlow = curr->isInlineFlowBox();

        int lineHeight = curr->object()->lineHeight(m_firstLine);
        int baseline = curr->object()->baselinePosition(m_firstLine);
        curr->setYPos(curr->object()->verticalPositionHint(m_firstLine));
        if (curr->yPos() == PositionTop) {
            if (maxPositionTop < lineHeight)
                maxPositionTop = lineHeight;
        }
        else if (curr->yPos() == PositionBottom) {
            if (maxPositionBottom < lineHeight)
                maxPositionBottom = lineHeight;
        }
        else if ((!isInlineFlow || static_cast<InlineFlowBox*>(curr)->hasTextChildren()) || curr->boxModelObject()->hasHorizontalBordersOrPadding() || strictMode) {
            int ascent = baseline - curr->yPos();
            int descent = lineHeight - ascent;
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
        setYPos(y + max(0, maxAscent - object()->baselinePosition(m_firstLine, true))); // Place our root box.
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        bool isInlineFlow = curr->isInlineFlowBox();
        if (isInlineFlow)
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(y, maxHeight, maxAscent, strictMode, topPosition, bottomPosition, selectionTop, selectionBottom);

        bool childAffectsTopBottomPos = true;
        if (curr->yPos() == PositionTop)
            curr->setYPos(y);
        else if (curr->yPos() == PositionBottom)
            curr->setYPos(y + maxHeight - curr->object()->lineHeight(m_firstLine));
        else {
            if ((isInlineFlow && !static_cast<InlineFlowBox*>(curr)->hasTextChildren()) && !curr->boxModelObject()->hasHorizontalBordersOrPadding() && !strictMode)
                childAffectsTopBottomPos = false;
            int posAdjust = maxAscent - curr->object()->baselinePosition(m_firstLine);
            if (!childAffectsTopBottomPos)
                posAdjust = max(0, posAdjust);
            curr->setYPos(curr->yPos() + y + posAdjust);
        }
        
        int newY = curr->yPos();
        int overflowTop = 0;
        int overflowBottom = 0;
        if (curr->isText() || curr->isInlineFlowBox()) {
            const Font& font = curr->object()->style(m_firstLine)->font();
            newY += curr->object()->baselinePosition(m_firstLine) - font.ascent();
            for (ShadowData* shadow = curr->object()->style()->textShadow(); shadow; shadow = shadow->next) {
                overflowTop = min(overflowTop, shadow->y - shadow->blur);
                overflowBottom = max(overflowBottom, shadow->y + shadow->blur);
            }

            for (ShadowData* boxShadow = curr->object()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
                overflowTop = min(overflowTop, boxShadow->y - boxShadow->blur);
                overflowBottom = max(overflowBottom, boxShadow->y + boxShadow->blur);
            }

            for (ShadowData* textShadow = curr->object()->style(m_firstLine)->textShadow(); textShadow; textShadow = textShadow->next) {
                overflowTop = min(overflowTop, textShadow->y - textShadow->blur);
                overflowBottom = max(overflowBottom, textShadow->y + textShadow->blur);
            }

            if (curr->object()->hasReflection()) {
                RenderBox* box = toRenderBox(curr->object());
                overflowTop = min(overflowTop, box->reflectionBox().y());
                overflowBottom = max(overflowBottom, box->reflectionBox().bottom());
            }

            if (curr->isInlineFlowBox())
                newY -= curr->boxModelObject()->borderTop() + curr->boxModelObject()->paddingTop();
        } else if (!curr->object()->isBR()) {
            RenderBox* box = toRenderBox(curr->object());
            newY += box->marginTop();
            overflowTop = box->overflowTop(false);
            overflowBottom = box->overflowHeight(false) - box->height();
        }

        curr->setYPos(newY);

        if (childAffectsTopBottomPos) {
            selectionTop = min(selectionTop, newY);
            selectionBottom = max(selectionBottom, newY + curr->height());
            topPosition = min(topPosition, newY + overflowTop);
            bottomPosition = max(bottomPosition, newY + curr->height() + overflowBottom);
        }
    }

    if (isRootInlineBox()) {
        const Font& font = object()->style(m_firstLine)->font();
        setYPos(yPos() + object()->baselinePosition(m_firstLine, true) - font.ascent());
        if (hasTextChildren() || strictMode) {
            selectionTop = min(selectionTop, yPos());
            selectionBottom = max(selectionBottom, yPos() + height());
        }
    }
}

bool InlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    // Check children first.
    for (InlineBox* curr = lastChild(); curr; curr = curr->prevOnLine()) {
        if (!curr->object()->hasLayer() && curr->nodeAtPoint(request, result, x, y, tx, ty)) {
            object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    }

    // Now check ourselves.
    IntRect rect(tx + m_x, ty + m_y, m_width, height());
    if (visibleToHitTesting() && rect.contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty)); // Don't add in m_x or m_y here, we want coords in the containing block's space.
        return true;
    }
    
    return false;
}

void InlineFlowBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    int xPos = tx + m_x - object()->maximalOutlineSize(paintInfo.phase);
    int w = width() + 2 * object()->maximalOutlineSize(paintInfo.phase);
    int shadowLeft = 0;
    int shadowRight = 0;
    for (ShadowData* boxShadow = object()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
        shadowLeft = min(boxShadow->x - boxShadow->blur, shadowLeft);
        shadowRight = max(boxShadow->x + boxShadow->blur, shadowRight);
    }
    for (ShadowData* textShadow = object()->style(m_firstLine)->textShadow(); textShadow; textShadow = textShadow->next) {
        shadowLeft = min(textShadow->x - textShadow->blur, shadowLeft);
        shadowRight = max(textShadow->x + textShadow->blur, shadowRight);
    }
    xPos += shadowLeft;
    w += -shadowLeft + shadowRight;
    bool intersectsDamageRect = xPos < paintInfo.rect.right() && xPos + w > paintInfo.rect.x();

    if (intersectsDamageRect && paintInfo.phase != PaintPhaseChildOutlines) {
        if (paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) {
            // Add ourselves to the paint info struct's list of inlines that need to paint their
            // outlines.
            if (object()->style()->visibility() == VISIBLE && object()->hasOutline() && !isRootInlineBox()) {
                RenderInline* inlineFlow = toRenderInline(object());
                if ((inlineFlow->continuation() || inlineFlow->isInlineContinuation()) && !object()->hasLayer()) {
                    // Add ourselves to the containing block of the entire continuation so that it can
                    // paint us atomically.
                    RenderBlock* block = object()->containingBlock()->containingBlock();
                    block->addContinuationWithOutline(toRenderInline(object()->element()->renderer()));
                } else if (!inlineFlow->isInlineContinuation())
                    paintInfo.outlineObjects->add(inlineFlow);
            }
        } else if (paintInfo.phase == PaintPhaseMask) {
            paintMask(paintInfo, tx, ty);
            return;
        } else {
            // 1. Paint our background, border and box-shadow.
            paintBoxDecorations(paintInfo, tx, ty);

            // 2. Paint our underline and overline.
            paintTextDecorations(paintInfo, tx, ty, false);
        }
    }

    if (paintInfo.phase == PaintPhaseMask)
        return;

    PaintPhase paintPhase = paintInfo.phase == PaintPhaseChildOutlines ? PaintPhaseOutline : paintInfo.phase;
    RenderObject::PaintInfo childInfo(paintInfo);
    childInfo.phase = paintPhase;
    childInfo.paintingRoot = object()->paintingRootForChildren(paintInfo);
    
    // 3. Paint our children.
    if (paintPhase != PaintPhaseSelfOutline) {
        for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (!curr->object()->hasLayer())
                curr->paint(childInfo, tx, ty);
        }
    }

    // 4. Paint our strike-through
    if (intersectsDamageRect && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        paintTextDecorations(paintInfo, tx, ty, true);
}

void InlineFlowBox::paintFillLayers(const RenderObject::PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer,
                                    int my, int mh, int _tx, int _ty, int w, int h, CompositeOperator op)
{
    if (!fillLayer)
        return;
    paintFillLayers(paintInfo, c, fillLayer->next(), my, mh, _tx, _ty, w, h, op);
    paintFillLayer(paintInfo, c, fillLayer, my, mh, _tx, _ty, w, h, op);
}

void InlineFlowBox::paintFillLayer(const RenderObject::PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer,
                                   int my, int mh, int tx, int ty, int w, int h, CompositeOperator op)
{
    StyleImage* img = fillLayer->image();
    bool hasFillImage = img && img->canRender(object()->style()->effectiveZoom());
    if ((!hasFillImage && !object()->style()->hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent())
        object()->paintFillLayerExtended(paintInfo, c, fillLayer, my, mh, tx, ty, w, h, this, op);
    else {
        // We have a fill image that spans multiple lines.
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
        int startX = tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
            totalWidth += curr->width();
        paintInfo.context->save();
        paintInfo.context->clip(IntRect(tx, ty, width(), height()));
        object()->paintFillLayerExtended(paintInfo, c, fillLayer, my, mh, startX, ty, totalWidth, h, this, op);
        paintInfo.context->restore();
    }
}

void InlineFlowBox::paintBoxShadow(GraphicsContext* context, RenderStyle* s, int tx, int ty, int w, int h)
{
    if ((!prevLineBox() && !nextLineBox()) || !parent())
        object()->paintBoxShadow(context, tx, ty, w, h, s);
    else {
        // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
        // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
        object()->paintBoxShadow(context, tx, ty, w, h, s, includeLeftEdge(), includeRightEdge());
    }
}

void InlineFlowBox::paintBoxDecorations(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!object()->shouldPaintWithinRoot(paintInfo) || object()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
        return;

    // Move x/y to our coordinates.
    tx += m_x;
    ty += m_y;
    
    int w = width();
    int h = height();

    int my = max(ty, paintInfo.rect.y());
    int mh;
    if (ty < paintInfo.rect.y())
        mh = max(0, h - (paintInfo.rect.y() - ty));
    else
        mh = min(paintInfo.rect.height(), h);

    GraphicsContext* context = paintInfo.context;
    
    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    if ((!parent() && m_firstLine && styleToUse != object()->style()) || (parent() && object()->hasBoxDecorations())) {
        // Shadow comes first and is behind the background and border.
        if (styleToUse->boxShadow())
            paintBoxShadow(context, styleToUse, tx, ty, w, h);

        Color c = styleToUse->backgroundColor();
        paintFillLayers(paintInfo, c, styleToUse->backgroundLayers(), my, mh, tx, ty, w, h);

        // :first-line cannot be used to put borders on a line. Always paint borders with our
        // non-first-line style.
        if (parent() && object()->style()->hasBorder()) {
            StyleImage* borderImage = object()->style()->borderImage().image();
            bool hasBorderImage = borderImage && borderImage->canRender(styleToUse->effectiveZoom());
            if (hasBorderImage && !borderImage->isLoaded())
                return; // Don't paint anything while we wait for the image to load.

            // The simple case is where we either have no border image or we are the only box for this object.  In those
            // cases only a single call to draw is required.
            if (!hasBorderImage || (!prevLineBox() && !nextLineBox()))
                object()->paintBorder(context, tx, ty, w, h, object()->style(), includeLeftEdge(), includeRightEdge());
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
                int startX = tx - xOffsetOnLine;
                int totalWidth = xOffsetOnLine;
                for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
                    totalWidth += curr->width();
                context->save();
                context->clip(IntRect(tx, ty, width(), height()));
                object()->paintBorder(context, startX, ty, totalWidth, h, object()->style());
                context->restore();
            }
        }
    }
}

void InlineFlowBox::paintMask(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!object()->shouldPaintWithinRoot(paintInfo) || object()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    // Move x/y to our coordinates.
    tx += m_x;
    ty += m_y;
    
    int w = width();
    int h = height();

    int my = max(ty, paintInfo.rect.y());
    int mh;
    if (ty < paintInfo.rect.y())
        mh = max(0, h - (paintInfo.rect.y() - ty));
    else
        mh = min(paintInfo.rect.height(), h);

    
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    const NinePieceImage& maskNinePieceImage = object()->style()->maskBoxImage();
    StyleImage* maskBoxImage = object()->style()->maskBoxImage().image();
    if ((maskBoxImage && object()->style()->maskLayers()->hasImage()) || object()->style()->maskLayers()->next())
        pushTransparencyLayer = true;
    
    CompositeOperator compositeOp = CompositeDestinationIn;
    if (pushTransparencyLayer) {
        paintInfo.context->setCompositeOperation(CompositeDestinationIn);
        paintInfo.context->beginTransparencyLayer(1.0f);
        compositeOp = CompositeSourceOver;
    }

    paintFillLayers(paintInfo, Color(), object()->style()->maskLayers(), my, mh, tx, ty, w, h, compositeOp);
    
    bool hasBoxImage = maskBoxImage && maskBoxImage->canRender(object()->style()->effectiveZoom());
    if (!hasBoxImage || !maskBoxImage->isLoaded())
        return; // Don't paint anything while we wait for the image to load.

    // The simple case is where we are the only box for this object.  In those
    // cases only a single call to draw is required.
    if (!prevLineBox() && !nextLineBox()) {
        object()->paintNinePieceImage(paintInfo.context, tx, ty, w, h, object()->style(), maskNinePieceImage, compositeOp);
    } else {
        // We have a mask image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        int xOffsetOnLine = 0;
        for (InlineRunBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            xOffsetOnLine += curr->width();
        int startX = tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
            totalWidth += curr->width();
        paintInfo.context->save();
        paintInfo.context->clip(IntRect(tx, ty, width(), height()));
        object()->paintNinePieceImage(paintInfo.context, startX, ty, totalWidth, h, object()->style(), maskNinePieceImage, compositeOp);
        paintInfo.context->restore();
    }
    
    if (pushTransparencyLayer)
        paintInfo.context->endTransparencyLayer();
}

static bool shouldDrawTextDecoration(RenderObject* obj)
{
    for (RenderObject* curr = obj->firstChild(); curr; curr = curr->nextSibling()) {
        if (curr->isRenderInline())
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

void InlineFlowBox::paintTextDecorations(RenderObject::PaintInfo& paintInfo, int tx, int ty, bool paintedChildren)
{
    // Paint text decorations like underlines/overlines. We only do this if we aren't in quirks mode (i.e., in
    // almost-strict mode or strict mode).
    if (object()->style()->htmlHacks() || !object()->shouldPaintWithinRoot(paintInfo) ||
        object()->style()->visibility() != VISIBLE)
        return;
    
    // We don't want underlines or other decorations when we're trying to draw nothing but the selection as white text.
    if (paintInfo.phase == PaintPhaseSelection && paintInfo.forceBlackText)
        return;

    GraphicsContext* context = paintInfo.context;
    tx += m_x;
    ty += m_y;
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int deco = parent() ? styleToUse->textDecoration() : styleToUse->textDecorationsInEffect();
    if (deco != TDNONE && 
        ((!paintedChildren && ((deco & UNDERLINE) || (deco & OVERLINE))) || (paintedChildren && (deco & LINE_THROUGH))) &&
        shouldDrawTextDecoration(object())) {
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
            } else {
                if (x >= ellipsisX)
                    return;
                if (x + w >= ellipsisX)
                    w -= (x + w - ellipsisX);
            }
        }

        // We must have child boxes and have decorations defined.
        tx += borderLeft() + paddingLeft();

        Color underline, overline, linethrough;
        underline = overline = linethrough = styleToUse->color();
        if (!parent())
            object()->getTextDecorationColors(deco, underline, overline, linethrough);

        bool isPrinting = object()->document()->printing();
        context->setStrokeThickness(1.0f); // FIXME: We should improve this rule and not always just assume 1.

        bool paintUnderline = deco & UNDERLINE && !paintedChildren;
        bool paintOverline = deco & OVERLINE && !paintedChildren;
        bool paintLineThrough = deco & LINE_THROUGH && paintedChildren;

        bool linesAreOpaque = !isPrinting && (!paintUnderline || underline.alpha() == 255) && (!paintOverline || overline.alpha() == 255) && (!paintLineThrough || linethrough.alpha() == 255);

        int baselinePos = baseline();
        
        bool setClip = false;
        int extraOffset = 0;
        ShadowData* shadow = styleToUse->textShadow();
        if (!linesAreOpaque && shadow && shadow->next) {
            IntRect clipRect(tx, ty, w, baselinePos + 2);
            for (ShadowData* s = shadow; s; s = s->next) {
                IntRect shadowRect(tx, ty, w, baselinePos + 2);
                shadowRect.inflate(s->blur);
                shadowRect.move(s->x, s->y);
                clipRect.unite(shadowRect);
                extraOffset = max(extraOffset, max(0, s->y) + s->blur);
            }
            context->save();
            context->clip(clipRect);
            extraOffset += baselinePos + 2;
            ty += extraOffset;
            setClip = true;
        }

        bool setShadow = false;
        do {
            if (shadow) {
                if (!shadow->next) {
                    // The last set of lines paints normally inside the clip.
                    ty -= extraOffset;
                    extraOffset = 0;
                }
                context->setShadow(IntSize(shadow->x, shadow->y - extraOffset), shadow->blur, shadow->color);
                setShadow = true;
                shadow = shadow->next;
            }

            if (paintUnderline) {
                context->setStrokeColor(underline);
                // Leave one pixel of white between the baseline and the underline.
                context->drawLineForText(IntPoint(tx, ty + baselinePos + 1), w, isPrinting);
            }
            if (paintOverline) {
                context->setStrokeColor(overline);
                context->drawLineForText(IntPoint(tx, ty), w, isPrinting);
            }
            if (paintLineThrough) {
                context->setStrokeColor(linethrough);
                context->drawLineForText(IntPoint(tx, ty + 2 * baselinePos / 3), w, isPrinting);
            }
        } while (shadow);

        if (setClip)
            context->restore();
        else if (setShadow)
            context->clearShadow();
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

#ifndef NDEBUG

void InlineFlowBox::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    ASSERT(!m_hasBadChildList);
    const InlineBox* prev = 0;
    for (const InlineBox* child = m_firstChild; child; child = child->nextOnLine()) {
        ASSERT(child->parent() == this);
        ASSERT(child->prevOnLine() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastChild);
#endif
}

#endif

} // namespace WebCore
