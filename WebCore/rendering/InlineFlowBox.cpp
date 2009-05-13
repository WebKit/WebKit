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
    if (child->renderer()->selectionState() != RenderObject::SelectionNone)
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
    toRenderInline(renderer())->lineBoxes()->removeLineBox(this);
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
    toRenderInline(renderer())->lineBoxes()->extractLineBox(this);
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
    toRenderInline(renderer())->lineBoxes()->attachLineBox(this);
}

void InlineFlowBox::adjustPosition(int dx, int dy)
{
    InlineRunBox::adjustPosition(dx, dy);
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
}

RenderLineBoxList* InlineFlowBox::rendererLineBoxes() const
{
    return toRenderInline(renderer())->lineBoxes();
}

bool InlineFlowBox::onEndChain(RenderObject* endObject)
{
    if (!endObject)
        return false;
    
    if (endObject == renderer())
        return true;

    RenderObject* curr = endObject;
    RenderObject* parent = curr->parent();
    while (parent && !parent->isRenderBlock()) {
        if (parent->lastChild() != curr || parent == renderer())
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

    // The root inline box never has borders/margins/padding.
    if (parent()) {
        bool ltr = renderer()->style()->direction() == LTR;

        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line (unless we are a continuation).
        RenderLineBoxList* lineBoxList = rendererLineBoxes();
        if (!lineBoxList->firstLineBox()->isConstructed() && !renderer()->isInlineContinuation()) {
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
            RenderInline* inlineFlow = toRenderInline(renderer());
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

int InlineFlowBox::placeBoxesHorizontally(int xPos, int& leftPosition, int& rightPosition, bool& needsWordSpacing)
{
    // Set our x position.
    setX(xPos);

    int boxShadowLeft = 0;
    int boxShadowRight = 0;
    for (ShadowData* boxShadow = renderer()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
        boxShadowLeft = min(boxShadow->x - boxShadow->blur, boxShadowLeft);
        boxShadowRight = max(boxShadow->x + boxShadow->blur, boxShadowRight);
    }
    leftPosition = min(xPos + boxShadowLeft, leftPosition);

    int startX = xPos;
    xPos += borderLeft() + paddingLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isText()) {
            InlineTextBox* text = static_cast<InlineTextBox*>(curr);
            RenderText* rt = toRenderText(text->renderer());
            if (rt->textLength()) {
                if (needsWordSpacing && isSpaceOrNewline(rt->characters()[text->start()]))
                    xPos += rt->style(m_firstLine)->font().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt->characters()[text->end()]);
            }
            text->setX(xPos);
            
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
            
            leftPosition = min(xPos + visualOverflowLeft, leftPosition);
            rightPosition = max(xPos + text->width() + visualOverflowRight, rightPosition);
            m_maxHorizontalVisualOverflow = max(max(visualOverflowRight, -visualOverflowLeft), (int)m_maxHorizontalVisualOverflow);
            xPos += text->width();
        } else {
            if (curr->renderer()->isPositioned()) {
                if (curr->renderer()->parent()->style()->direction() == LTR)
                    curr->setX(xPos);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    curr->setX(root()->block()->width() - xPos);
                continue; // The positioned object has no effect on the width.
            }
            if (curr->renderer()->isRenderInline()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                xPos += flow->marginLeft();
                xPos = flow->placeBoxesHorizontally(xPos, leftPosition, rightPosition, needsWordSpacing);
                xPos += flow->marginRight();
            } else if (!curr->renderer()->isListMarker() || static_cast<RenderListMarker*>(curr->renderer())->isInside()) {
                xPos += curr->boxModelObject()->marginLeft();
                curr->setX(xPos);
                leftPosition = min(xPos + toRenderBox(curr->renderer())->overflowLeft(false), leftPosition);
                rightPosition = max(xPos + toRenderBox(curr->renderer())->overflowWidth(false), rightPosition);
                xPos += curr->width() + curr->boxModelObject()->marginRight();
            }
        }
    }

    xPos += borderRight() + paddingRight();
    setWidth(xPos - startX);
    rightPosition = max(x() + width() + boxShadowRight, rightPosition);

    return xPos;
}

int InlineFlowBox::verticallyAlignBoxes(int heightOfBlock)
{
    int maxPositionTop = 0;
    int maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;

    // Figure out if we're in strict mode.  Note that we can't simply use !style()->htmlHacks(),
    // because that would match almost strict mode as well.
    RenderObject* curr = renderer();
    while (curr && !curr->node())
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
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        if (curr->y() == PositionTop || curr->y() == PositionBottom) {
            int lineHeight = curr->renderer()->lineHeight(m_firstLine);
            if (curr->y() == PositionTop) {
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

static int verticalPositionForBox(InlineBox* curr, bool firstLine)
{
    if (curr->renderer()->isText())
        return curr->parent()->y();
    if (curr->renderer()->isBox())
        return toRenderBox(curr->renderer())->verticalPosition(firstLine);
    return toRenderInline(curr->renderer())->verticalPositionFromCache(firstLine);
}

void InlineFlowBox::computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                             int& maxAscent, int& maxDescent, bool strictMode)
{
    if (isRootInlineBox()) {
        // Examine our root box.
        int lineHeight = renderer()->lineHeight(m_firstLine, true);
        int baseline = renderer()->baselinePosition(m_firstLine, true);
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
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        bool isInlineFlow = curr->isInlineFlowBox();

        int lineHeight;
        int baseline;
        Vector<const SimpleFontData*> usedFonts;
        if (curr->isInlineTextBox())
            static_cast<InlineTextBox*>(curr)->takeFallbackFonts(usedFonts);

        if (!usedFonts.isEmpty()) {
            usedFonts.append(curr->renderer()->style(m_firstLine)->font().primaryFont());
            Length parentLineHeight = curr->renderer()->parent()->style()->lineHeight();
            if (parentLineHeight.isNegative()) {
                int baselineToBottom = 0;
                baseline = 0;
                for (size_t i = 0; i < usedFonts.size(); ++i) {
                    int halfLeading = (usedFonts[i]->lineSpacing() - usedFonts[i]->ascent() - usedFonts[i]->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts[i]->ascent());
                    baselineToBottom = max(baselineToBottom, usedFonts[i]->lineSpacing() - usedFonts[i]->ascent() - usedFonts[i]->descent() - halfLeading);
                }
                lineHeight = baseline + baselineToBottom;
            } else if (parentLineHeight.isPercent()) {
                lineHeight = parentLineHeight.calcMinValue(curr->renderer()->style()->fontSize());
                baseline = 0;
                for (size_t i = 0; i < usedFonts.size(); ++i) {
                    int halfLeading = (lineHeight - usedFonts[i]->ascent() - usedFonts[i]->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts[i]->ascent());
                }
            } else {
                lineHeight = parentLineHeight.value();
                baseline = 0;
                for (size_t i = 0; i < usedFonts.size(); ++i) {
                    int halfLeading = (lineHeight - usedFonts[i]->ascent() - usedFonts[i]->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts[i]->ascent());
                }
            }
        } else {
            lineHeight = curr->renderer()->lineHeight(m_firstLine);
            baseline = curr->renderer()->baselinePosition(m_firstLine);
        }

        curr->setY(verticalPositionForBox(curr, m_firstLine));
        if (curr->y() == PositionTop) {
            if (maxPositionTop < lineHeight)
                maxPositionTop = lineHeight;
        } else if (curr->y() == PositionBottom) {
            if (maxPositionBottom < lineHeight)
                maxPositionBottom = lineHeight;
        } else if ((!isInlineFlow || static_cast<InlineFlowBox*>(curr)->hasTextChildren()) || curr->boxModelObject()->hasHorizontalBordersOrPadding() || strictMode) {
            int ascent = baseline - curr->y();
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

void InlineFlowBox::placeBoxesVertically(int yPos, int maxHeight, int maxAscent, bool strictMode,
                                         int& topPosition, int& bottomPosition, int& selectionTop, int& selectionBottom)
{
    if (isRootInlineBox())
        setY(yPos + max(0, maxAscent - renderer()->baselinePosition(m_firstLine, true))); // Place our root box.
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        bool isInlineFlow = curr->isInlineFlowBox();
        if (isInlineFlow)
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(yPos, maxHeight, maxAscent, strictMode, topPosition, bottomPosition, selectionTop, selectionBottom);

        bool childAffectsTopBottomPos = true;
        if (curr->y() == PositionTop)
            curr->setY(yPos);
        else if (curr->y() == PositionBottom)
            curr->setY(yPos + maxHeight - curr->renderer()->lineHeight(m_firstLine));
        else {
            if ((isInlineFlow && !static_cast<InlineFlowBox*>(curr)->hasTextChildren()) && !curr->boxModelObject()->hasHorizontalBordersOrPadding() && !strictMode)
                childAffectsTopBottomPos = false;
            int posAdjust = maxAscent - curr->renderer()->baselinePosition(m_firstLine);
            if (!childAffectsTopBottomPos)
                posAdjust = max(0, posAdjust);
            curr->setY(curr->y() + yPos + posAdjust);
        }
        
        // FIXME: By only considering overflow as part of the root line box, we can't get an accurate picture regarding what the line
        // actually needs to paint.  A line box that is part of a self-painting layer technically shouldn't contribute to the overflow
        // of the line, but in order to not do this and paint accurately, we have to track the overflow somewhere else (either by storing overflow
        // in each InlineFlowBox up the chain or in the layer itself).  Relative positioned objects on a line will cause scrollbars
        // to appear when they shouldn't until we fix this issue.
        int newY = curr->y();
        int overflowTop = 0;
        int overflowBottom = 0;
        if (curr->isText() || curr->isInlineFlowBox()) {
            const Font& font = curr->renderer()->style(m_firstLine)->font();
            newY += curr->renderer()->baselinePosition(m_firstLine) - font.ascent();
            for (ShadowData* shadow = curr->renderer()->style()->textShadow(); shadow; shadow = shadow->next) {
                overflowTop = min(overflowTop, shadow->y - shadow->blur);
                overflowBottom = max(overflowBottom, shadow->y + shadow->blur);
            }

            for (ShadowData* boxShadow = curr->renderer()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
                overflowTop = min(overflowTop, boxShadow->y - boxShadow->blur);
                overflowBottom = max(overflowBottom, boxShadow->y + boxShadow->blur);
            }

            for (ShadowData* textShadow = curr->renderer()->style(m_firstLine)->textShadow(); textShadow; textShadow = textShadow->next) {
                overflowTop = min(overflowTop, textShadow->y - textShadow->blur);
                overflowBottom = max(overflowBottom, textShadow->y + textShadow->blur);
            }

            if (curr->renderer()->hasReflection()) {
                RenderBox* box = toRenderBox(curr->renderer());
                overflowTop = min(overflowTop, box->reflectionBox().y());
                overflowBottom = max(overflowBottom, box->reflectionBox().bottom());
            }

            if (curr->isInlineFlowBox())
                newY -= curr->boxModelObject()->borderTop() + curr->boxModelObject()->paddingTop();
        } else if (!curr->renderer()->isBR()) {
            RenderBox* box = toRenderBox(curr->renderer());
            newY += box->marginTop();
            overflowTop = box->overflowTop(false);
            overflowBottom = box->overflowHeight(false) - box->height();
        }

        curr->setY(newY);

        if (childAffectsTopBottomPos) {
            int boxHeight = curr->height();
            selectionTop = min(selectionTop, newY);
            selectionBottom = max(selectionBottom, newY + boxHeight);
            topPosition = min(topPosition, newY + overflowTop);
            bottomPosition = max(bottomPosition, newY + boxHeight + overflowBottom);
        }
    }

    if (isRootInlineBox()) {
        const Font& font = renderer()->style(m_firstLine)->font();
        setY(y() + renderer()->baselinePosition(m_firstLine, true) - font.ascent());
        if (hasTextChildren() || strictMode) {
            selectionTop = min(selectionTop, y());
            selectionBottom = max(selectionBottom, y() + height());
        }
    }
}

bool InlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    // Check children first.
    for (InlineBox* curr = lastChild(); curr; curr = curr->prevOnLine()) {
        if ((curr->renderer()->isText() || !curr->boxModelObject()->hasSelfPaintingLayer()) && curr->nodeAtPoint(request, result, x, y, tx, ty)) {
            renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    }

    // Now check ourselves.
    IntRect rect(tx + m_x, ty + m_y, m_width, height());
    if (visibleToHitTesting() && rect.contains(x, y)) {
        renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty)); // Don't add in m_x or m_y here, we want coords in the containing block's space.
        return true;
    }
    
    return false;
}

void InlineFlowBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    int xPos = tx + m_x - renderer()->maximalOutlineSize(paintInfo.phase);
    int w = width() + 2 * renderer()->maximalOutlineSize(paintInfo.phase);
    int shadowLeft = 0;
    int shadowRight = 0;
    for (ShadowData* boxShadow = renderer()->style(m_firstLine)->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
        shadowLeft = min(boxShadow->x - boxShadow->blur, shadowLeft);
        shadowRight = max(boxShadow->x + boxShadow->blur, shadowRight);
    }
    for (ShadowData* textShadow = renderer()->style(m_firstLine)->textShadow(); textShadow; textShadow = textShadow->next) {
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
            if (renderer()->style()->visibility() == VISIBLE && renderer()->hasOutline() && !isRootInlineBox()) {
                RenderInline* inlineFlow = toRenderInline(renderer());
                if ((inlineFlow->continuation() || inlineFlow->isInlineContinuation()) && !boxModelObject()->hasSelfPaintingLayer()) {
                    // Add ourselves to the containing block of the entire continuation so that it can
                    // paint us atomically.
                    RenderBlock* block = renderer()->containingBlock()->containingBlock();
                    block->addContinuationWithOutline(toRenderInline(renderer()->node()->renderer()));
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
    childInfo.paintingRoot = renderer()->paintingRootForChildren(paintInfo);
    
    // 3. Paint our children.
    if (paintPhase != PaintPhaseSelfOutline) {
        for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (curr->renderer()->isText() || !curr->boxModelObject()->hasSelfPaintingLayer())
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
    bool hasFillImage = img && img->canRender(renderer()->style()->effectiveZoom());
    if ((!hasFillImage && !renderer()->style()->hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent())
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, my, mh, tx, ty, w, h, this, op);
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
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, my, mh, startX, ty, totalWidth, h, this, op);
        paintInfo.context->restore();
    }
}

void InlineFlowBox::paintBoxShadow(GraphicsContext* context, RenderStyle* s, int tx, int ty, int w, int h)
{
    if ((!prevLineBox() && !nextLineBox()) || !parent())
        boxModelObject()->paintBoxShadow(context, tx, ty, w, h, s);
    else {
        // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
        // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
        boxModelObject()->paintBoxShadow(context, tx, ty, w, h, s, includeLeftEdge(), includeRightEdge());
    }
}

void InlineFlowBox::paintBoxDecorations(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!renderer()->shouldPaintWithinRoot(paintInfo) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
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
    RenderStyle* styleToUse = renderer()->style(m_firstLine);
    if ((!parent() && m_firstLine && styleToUse != renderer()->style()) || (parent() && renderer()->hasBoxDecorations())) {
        // Shadow comes first and is behind the background and border.
        if (styleToUse->boxShadow())
            paintBoxShadow(context, styleToUse, tx, ty, w, h);

        Color c = styleToUse->backgroundColor();
        paintFillLayers(paintInfo, c, styleToUse->backgroundLayers(), my, mh, tx, ty, w, h);

        // :first-line cannot be used to put borders on a line. Always paint borders with our
        // non-first-line style.
        if (parent() && renderer()->style()->hasBorder()) {
            StyleImage* borderImage = renderer()->style()->borderImage().image();
            bool hasBorderImage = borderImage && borderImage->canRender(styleToUse->effectiveZoom());
            if (hasBorderImage && !borderImage->isLoaded())
                return; // Don't paint anything while we wait for the image to load.

            // The simple case is where we either have no border image or we are the only box for this object.  In those
            // cases only a single call to draw is required.
            if (!hasBorderImage || (!prevLineBox() && !nextLineBox()))
                boxModelObject()->paintBorder(context, tx, ty, w, h, renderer()->style(), includeLeftEdge(), includeRightEdge());
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
                context->clip(IntRect(tx, ty, w, h));
                boxModelObject()->paintBorder(context, startX, ty, totalWidth, h, renderer()->style());
                context->restore();
            }
        }
    }
}

void InlineFlowBox::paintMask(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!renderer()->shouldPaintWithinRoot(paintInfo) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
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
    const NinePieceImage& maskNinePieceImage = renderer()->style()->maskBoxImage();
    StyleImage* maskBoxImage = renderer()->style()->maskBoxImage().image();
    if ((maskBoxImage && renderer()->style()->maskLayers()->hasImage()) || renderer()->style()->maskLayers()->next())
        pushTransparencyLayer = true;
    
    CompositeOperator compositeOp = CompositeDestinationIn;
    if (pushTransparencyLayer) {
        paintInfo.context->setCompositeOperation(CompositeDestinationIn);
        paintInfo.context->beginTransparencyLayer(1.0f);
        compositeOp = CompositeSourceOver;
    }

    paintFillLayers(paintInfo, Color(), renderer()->style()->maskLayers(), my, mh, tx, ty, w, h, compositeOp);
    
    bool hasBoxImage = maskBoxImage && maskBoxImage->canRender(renderer()->style()->effectiveZoom());
    if (!hasBoxImage || !maskBoxImage->isLoaded())
        return; // Don't paint anything while we wait for the image to load.

    // The simple case is where we are the only box for this object.  In those
    // cases only a single call to draw is required.
    if (!prevLineBox() && !nextLineBox()) {
        boxModelObject()->paintNinePieceImage(paintInfo.context, tx, ty, w, h, renderer()->style(), maskNinePieceImage, compositeOp);
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
        paintInfo.context->clip(IntRect(tx, ty, w, h));
        boxModelObject()->paintNinePieceImage(paintInfo.context, startX, ty, totalWidth, h, renderer()->style(), maskNinePieceImage, compositeOp);
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
            Node* currElement = curr->node();
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
    if (renderer()->style()->htmlHacks() || !renderer()->shouldPaintWithinRoot(paintInfo) ||
        renderer()->style()->visibility() != VISIBLE)
        return;
    
    // We don't want underlines or other decorations when we're trying to draw nothing but the selection as white text.
    if (paintInfo.phase == PaintPhaseSelection && paintInfo.forceBlackText)
        return;

    GraphicsContext* context = paintInfo.context;
    tx += m_x;
    ty += m_y;
    RenderStyle* styleToUse = renderer()->style(m_firstLine);
    int deco = parent() ? styleToUse->textDecoration() : styleToUse->textDecorationsInEffect();
    if (deco != TDNONE && 
        ((!paintedChildren && ((deco & UNDERLINE) || (deco & OVERLINE))) || (paintedChildren && (deco & LINE_THROUGH))) &&
        shouldDrawTextDecoration(renderer())) {
        int x = m_x + borderLeft() + paddingLeft();
        int w = m_width - (borderLeft() + paddingLeft() + borderRight() + paddingRight());
        RootInlineBox* rootLine = root();
        if (rootLine->ellipsisBox()) {
            int ellipsisX = m_x + rootLine->ellipsisBox()->x();
            int ellipsisWidth = rootLine->ellipsisBox()->width();
            bool ltr = renderer()->style()->direction() == LTR;
            if (rootLine == this) {
                // Trim w and x so that the underline isn't drawn underneath the ellipsis.
                // ltr: is our right edge farther right than the right edge of the ellipsis.
                // rtl: is the left edge of our box farther left than the left edge of the ellipsis.
                bool ltrTruncation = ltr && (x + w >= ellipsisX + ellipsisWidth);
                bool rtlTruncation = !ltr && (x <= ellipsisX + ellipsisWidth);
                if (ltrTruncation)
                    w -= (x + w) - (ellipsisX + ellipsisWidth);
                else if (rtlTruncation) {
                    int dx = m_x - ((ellipsisX - m_x) + ellipsisWidth);
                    tx -= dx;
                    w += dx;
                }
            } else {
                bool ltrPastEllipsis = ltr && x >= ellipsisX;
                bool rtlPastEllipsis = !ltr && (x + w) <= (ellipsisX + ellipsisWidth);
                if (ltrPastEllipsis || rtlPastEllipsis)
                    return;

                bool ltrTruncation = ltr && x + w >= ellipsisX;
                bool rtlTruncation = !ltr && x <= ellipsisX;
                if (ltrTruncation)
                    w -= (x + w - ellipsisX);
                else if (rtlTruncation) {
                    int dx = m_x - ((ellipsisX - m_x) + ellipsisWidth);
                    tx -= dx;
                    w += dx;
                }
            }
        }

        // We must have child boxes and have decorations defined.
        tx += borderLeft() + paddingLeft();

        Color underline, overline, linethrough;
        underline = overline = linethrough = styleToUse->color();
        if (!parent())
            renderer()->getTextDecorationColors(deco, underline, overline, linethrough);

        bool isPrinting = renderer()->document()->printing();
        context->setStrokeThickness(1.0f); // FIXME: We should improve this rule and not always just assume 1.

        bool paintUnderline = deco & UNDERLINE && !paintedChildren;
        bool paintOverline = deco & OVERLINE && !paintedChildren;
        bool paintLineThrough = deco & LINE_THROUGH && paintedChildren;

        bool linesAreOpaque = !isPrinting && (!paintUnderline || underline.alpha() == 255) && (!paintOverline || overline.alpha() == 255) && (!paintLineThrough || linethrough.alpha() == 255);

        int baselinePos = renderer()->style(m_firstLine)->font().ascent();
        if (!isRootInlineBox())
            baselinePos += borderTop() + paddingTop();

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
                context->setStrokeStyle(SolidStroke);
                // Leave one pixel of white between the baseline and the underline.
                context->drawLineForText(IntPoint(tx, ty + baselinePos + 1), w, isPrinting);
            }
            if (paintOverline) {
                context->setStrokeColor(overline);
                context->setStrokeStyle(SolidStroke);
                context->drawLineForText(IntPoint(tx, ty), w, isPrinting);
            }
            if (paintLineThrough) {
                context->setStrokeColor(linethrough);
                context->setStrokeStyle(SolidStroke);
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

int InlineFlowBox::placeEllipsisBox(bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, bool& foundBox)
{
    int result = -1;
    // We iterate over all children, the foundBox variable tells us when we've found the
    // box containing the ellipsis.  All boxes after that one in the flow are hidden.
    // If our flow is ltr then iterate over the boxes from left to right, otherwise iterate
    // from right to left. Varying the order allows us to correctly hide the boxes following the ellipsis.
    InlineBox *box = ltr ? firstChild() : lastChild();

    // NOTE: these will cross after foundBox = true.
    int visibleLeftEdge = blockLeftEdge;
    int visibleRightEdge = blockRightEdge;

    while(box) {
        int currResult = box->placeEllipsisBox(ltr, visibleLeftEdge, visibleRightEdge, ellipsisWidth, foundBox);
        if (currResult != -1 && result == -1)
            result = currResult;

        if (ltr) {
            visibleLeftEdge += box->width();
            box = box->nextOnLine();
        }
        else {
            visibleRightEdge -= box->width();
            box = box->prevOnLine();
        }
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
