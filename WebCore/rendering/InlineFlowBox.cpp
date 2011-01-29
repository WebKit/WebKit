/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "CSSPropertyNames.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "HitTestResult.h"
#include "RootInlineBox.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
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
    InlineBox::adjustPosition(dx, dy);
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
    if (m_overflow)
        m_overflow->move(dx, dy);
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

int InlineFlowBox::placeBoxesHorizontally(int xPos, bool& needsWordSpacing)
{
    // Set our x position.
    setX(xPos);

    int leftLayoutOverflow = xPos;
    int rightLayoutOverflow = xPos;
    int leftVisualOverflow = xPos;
    int rightVisualOverflow = xPos;

    int boxShadowLeft;
    int boxShadowRight;
    renderer()->style(m_firstLine)->getBoxShadowHorizontalExtent(boxShadowLeft, boxShadowRight);

    leftVisualOverflow = min(xPos + boxShadowLeft, leftVisualOverflow);

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
            
            // If letter-spacing is negative, we should factor that into right layout overflow. (Even in RTL, letter-spacing is
            // applied to the right, so this is not an issue with left overflow.
            int letterSpacing = min(0, (int)rt->style(m_firstLine)->font().letterSpacing());
            rightLayoutOverflow = max(xPos + text->width() - letterSpacing, rightLayoutOverflow);

            GlyphOverflow* glyphOverflow = static_cast<InlineTextBox*>(curr)->glyphOverflow();

            int leftGlyphOverflow = -strokeOverflow - (glyphOverflow ? glyphOverflow->left : 0);
            int rightGlyphOverflow = strokeOverflow - letterSpacing + (glyphOverflow ? glyphOverflow->right : 0);
            
            int childOverflowLeft = leftGlyphOverflow;
            int childOverflowRight = rightGlyphOverflow;
            for (const ShadowData* shadow = rt->style()->textShadow(); shadow; shadow = shadow->next()) {
                childOverflowLeft = min(childOverflowLeft, shadow->x() - shadow->blur() + leftGlyphOverflow);
                childOverflowRight = max(childOverflowRight, shadow->x() + shadow->blur() + rightGlyphOverflow);
            }
            
            leftVisualOverflow = min(xPos + childOverflowLeft, leftVisualOverflow);
            rightVisualOverflow = max(xPos + text->width() + childOverflowRight, rightVisualOverflow);
            
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
                xPos = flow->placeBoxesHorizontally(xPos, needsWordSpacing);
                xPos += flow->marginRight();
                leftLayoutOverflow = min(leftLayoutOverflow, flow->leftLayoutOverflow());
                rightLayoutOverflow = max(rightLayoutOverflow, flow->rightLayoutOverflow());
                leftVisualOverflow = min(leftVisualOverflow, flow->leftVisualOverflow());
                rightVisualOverflow = max(rightVisualOverflow, flow->rightVisualOverflow());
            } else if (!curr->renderer()->isListMarker() || toRenderListMarker(curr->renderer())->isInside()) {
                xPos += curr->boxModelObject()->marginLeft();
                curr->setX(xPos);
                 
                RenderBox* box = toRenderBox(curr->renderer());
                int childLeftOverflow = box->hasOverflowClip() ? 0 : box->leftLayoutOverflow();
                int childRightOverflow = box->hasOverflowClip() ? curr->width() : box->rightLayoutOverflow();
                
                leftLayoutOverflow = min(xPos + childLeftOverflow, leftLayoutOverflow);
                rightLayoutOverflow = max(xPos + childRightOverflow, rightLayoutOverflow);

                leftVisualOverflow = min(xPos + box->leftVisualOverflow(), leftVisualOverflow);
                rightVisualOverflow = max(xPos + box->rightVisualOverflow(), rightVisualOverflow);
               
                xPos += curr->width() + curr->boxModelObject()->marginRight();
            }
        }
    }

    xPos += borderRight() + paddingRight();
    setWidth(xPos - startX);
    rightVisualOverflow = max(x() + width() + boxShadowRight, rightVisualOverflow);
    rightLayoutOverflow = max(x() + width(), rightLayoutOverflow);

    setHorizontalOverflowPositions(leftLayoutOverflow, rightLayoutOverflow, leftVisualOverflow, rightVisualOverflow);
    return xPos;
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
            int lineHeight = curr->lineHeight(false);
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
        int height = lineHeight(true);
        int baseline = baselinePosition(true);
        if (hasTextChildren() || strictMode) {
            int ascent = baseline;
            int descent = height - ascent;
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
        Vector<const SimpleFontData*>* usedFonts = 0;
        if (curr->isInlineTextBox())
            usedFonts = static_cast<InlineTextBox*>(curr)->fallbackFonts();

        if (usedFonts) {
            usedFonts->append(curr->renderer()->style(m_firstLine)->font().primaryFont());
            Length parentLineHeight = curr->renderer()->parent()->style()->lineHeight();
            if (parentLineHeight.isNegative()) {
                int baselineToBottom = 0;
                baseline = 0;
                for (size_t i = 0; i < usedFonts->size(); ++i) {
                    int halfLeading = (usedFonts->at(i)->lineSpacing() - usedFonts->at(i)->ascent() - usedFonts->at(i)->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts->at(i)->ascent());
                    baselineToBottom = max(baselineToBottom, usedFonts->at(i)->lineSpacing() - usedFonts->at(i)->ascent() - usedFonts->at(i)->descent() - halfLeading);
                }
                lineHeight = baseline + baselineToBottom;
            } else if (parentLineHeight.isPercent()) {
                lineHeight = parentLineHeight.calcMinValue(curr->renderer()->style()->fontSize());
                baseline = 0;
                for (size_t i = 0; i < usedFonts->size(); ++i) {
                    int halfLeading = (lineHeight - usedFonts->at(i)->ascent() - usedFonts->at(i)->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts->at(i)->ascent());
                }
            } else {
                lineHeight = parentLineHeight.value();
                baseline = 0;
                for (size_t i = 0; i < usedFonts->size(); ++i) {
                    int halfLeading = (lineHeight - usedFonts->at(i)->ascent() - usedFonts->at(i)->descent()) / 2;
                    baseline = max(baseline, halfLeading + usedFonts->at(i)->ascent());
                }
            }
        } else {
            lineHeight = curr->lineHeight(false);
            baseline = curr->baselinePosition(false);
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

void InlineFlowBox::placeBoxesVertically(int yPos, int maxHeight, int maxAscent, bool strictMode, int& selectionTop, int& selectionBottom)
{
    if (isRootInlineBox())
        setY(yPos + maxAscent - baselinePosition(true)); // Place our root box.

    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        bool isInlineFlow = curr->isInlineFlowBox();
        if (isInlineFlow)
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(yPos, maxHeight, maxAscent, strictMode, selectionTop, selectionBottom);

        bool childAffectsTopBottomPos = true;
        if (curr->y() == PositionTop)
            curr->setY(yPos);
        else if (curr->y() == PositionBottom)
            curr->setY(yPos + maxHeight - curr->lineHeight(false));
        else {
            if ((isInlineFlow && !static_cast<InlineFlowBox*>(curr)->hasTextChildren()) && !curr->boxModelObject()->hasHorizontalBordersOrPadding() && !strictMode)
                childAffectsTopBottomPos = false;
            int posAdjust = maxAscent - curr->baselinePosition(false);
            curr->setY(curr->y() + yPos + posAdjust);
        }
        
        int newY = curr->y();
        if (curr->isText() || curr->isInlineFlowBox()) {
            const Font& font = curr->renderer()->style(m_firstLine)->font();
            newY += curr->baselinePosition(false) - font.ascent();
            if (curr->isInlineFlowBox())
                newY -= curr->boxModelObject()->borderTop() + curr->boxModelObject()->paddingTop();
        } else if (!curr->renderer()->isBR()) {
            RenderBox* box = toRenderBox(curr->renderer());
            newY += box->marginTop();
        }

        curr->setY(newY);

        if (childAffectsTopBottomPos) {
            int boxHeight = curr->height();
            selectionTop = min(selectionTop, newY);
            selectionBottom = max(selectionBottom, newY + boxHeight);
        }
    }

    if (isRootInlineBox()) {
        const Font& font = renderer()->style(m_firstLine)->font();
        setY(y() + baselinePosition(true) - font.ascent());
        if (hasTextChildren() || strictMode) {
            selectionTop = min(selectionTop, y());
            selectionBottom = max(selectionBottom, y() + height());
        }
    }
}

void InlineFlowBox::computeVerticalOverflow(int lineTop, int lineBottom, bool strictMode)
{
    int boxHeight = height();

    // Any spillage outside of the line top and bottom is not considered overflow.  We just ignore this, since it only happens
    // from the "your ascent/descent don't affect the line" quirk.
    int topOverflow = max(y(), lineTop);
    int bottomOverflow = min(y() + boxHeight, lineBottom);
    
    int topLayoutOverflow = topOverflow;
    int bottomLayoutOverflow = bottomOverflow;
    
    int topVisualOverflow = topOverflow;
    int bottomVisualOverflow = bottomOverflow;
  
    // box-shadow on root line boxes is applying to the block and not to the lines.
    if (parent()) {
        int boxShadowTop;
        int boxShadowBottom;
        renderer()->style(m_firstLine)->getBoxShadowVerticalExtent(boxShadowTop, boxShadowBottom);
        
        topVisualOverflow = min(y() + boxShadowTop, topVisualOverflow);
        bottomVisualOverflow = max(y() + boxHeight + boxShadowBottom, bottomVisualOverflow);
    }

    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (curr->renderer()->isText()) {
            InlineTextBox* text = static_cast<InlineTextBox*>(curr);
            RenderText* rt = toRenderText(text->renderer());
            if (rt->isBR())
                continue;

            int strokeOverflow = static_cast<int>(ceilf(rt->style()->textStrokeWidth() / 2.0f));

            GlyphOverflow* glyphOverflow = static_cast<InlineTextBox*>(curr)->glyphOverflow();

            int topGlyphOverflow = -strokeOverflow - (glyphOverflow ? glyphOverflow->top : 0);
            int bottomGlyphOverflow = strokeOverflow + (glyphOverflow ? glyphOverflow->bottom : 0);

            int childOverflowTop = topGlyphOverflow;
            int childOverflowBottom = bottomGlyphOverflow;
            for (const ShadowData* shadow = rt->style()->textShadow(); shadow; shadow = shadow->next()) {
                childOverflowTop = min(childOverflowTop, shadow->y() - shadow->blur() + topGlyphOverflow);
                childOverflowBottom = max(childOverflowBottom, shadow->y() + shadow->blur() + bottomGlyphOverflow);
            }
            
            topVisualOverflow = min(curr->y() + childOverflowTop, topVisualOverflow);
            bottomVisualOverflow = max(curr->y() + text->height() + childOverflowBottom, bottomVisualOverflow);
        } else  if (curr->renderer()->isRenderInline()) {
            InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
            flow->computeVerticalOverflow(lineTop, lineBottom, strictMode);
            topLayoutOverflow = min(topLayoutOverflow, flow->topLayoutOverflow());
            bottomLayoutOverflow = max(bottomLayoutOverflow, flow->bottomLayoutOverflow());
            topVisualOverflow = min(topVisualOverflow, flow->topVisualOverflow());
            bottomVisualOverflow = max(bottomVisualOverflow, flow->bottomVisualOverflow());
        } else if (!curr->boxModelObject()->hasSelfPaintingLayer()){
            // Only include overflow from replaced inlines if they do not paint themselves.
            RenderBox* box = toRenderBox(curr->renderer());
            int boxY = curr->y();
            int childTopOverflow = box->hasOverflowClip() ? 0 : box->topLayoutOverflow();
            int childBottomOverflow = box->hasOverflowClip() ? curr->height() : box->bottomLayoutOverflow();
            topLayoutOverflow = min(boxY + childTopOverflow, topLayoutOverflow);
            bottomLayoutOverflow = max(boxY + childBottomOverflow, bottomLayoutOverflow);
            topVisualOverflow = min(boxY + box->topVisualOverflow(), topVisualOverflow);
            bottomVisualOverflow = max(boxY + box->bottomVisualOverflow(), bottomVisualOverflow);
        }
    }
    
    setVerticalOverflowPositions(topLayoutOverflow, bottomLayoutOverflow, topVisualOverflow, bottomVisualOverflow, boxHeight);
}

bool InlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    IntRect overflowRect(visibleOverflowRect());
    overflowRect.move(tx, ty);
    if (!overflowRect.contains(x, y))
        return false;

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
    IntRect overflowRect(visibleOverflowRect());
    overflowRect.inflate(renderer()->maximalOutlineSize(paintInfo.phase));
    overflowRect.move(tx, ty);
    
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    if (paintInfo.phase != PaintPhaseChildOutlines) {
        if (paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) {
            // Add ourselves to the paint info struct's list of inlines that need to paint their
            // outlines.
            if (renderer()->style()->visibility() == VISIBLE && renderer()->hasOutline() && !isRootInlineBox()) {
                RenderInline* inlineFlow = toRenderInline(renderer());

                RenderBlock* cb = 0;
                bool containingBlockPaintsContinuationOutline = inlineFlow->continuation() || inlineFlow->isInlineContinuation();
                if (containingBlockPaintsContinuationOutline) {
                    cb = renderer()->containingBlock()->containingBlock();

                    for (RenderBoxModelObject* box = boxModelObject(); box != cb; box = box->parent()->enclosingBoxModelObject()) {
                        if (box->hasSelfPaintingLayer()) {
                            containingBlockPaintsContinuationOutline = false;
                            break;
                        }
                    }
                }

                if (containingBlockPaintsContinuationOutline) {
                    // Add ourselves to the containing block of the entire continuation so that it can
                    // paint us atomically.
                    cb->addContinuationWithOutline(toRenderInline(renderer()->node()->renderer()));
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
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)
        paintTextDecorations(paintInfo, tx, ty, true);
}

void InlineFlowBox::paintFillLayers(const RenderObject::PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, int _tx, int _ty, int w, int h, CompositeOperator op)
{
    if (!fillLayer)
        return;
    paintFillLayers(paintInfo, c, fillLayer->next(), _tx, _ty, w, h, op);
    paintFillLayer(paintInfo, c, fillLayer, _tx, _ty, w, h, op);
}

void InlineFlowBox::paintFillLayer(const RenderObject::PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, int tx, int ty, int w, int h, CompositeOperator op)
{
    StyleImage* img = fillLayer->image();
    bool hasFillImage = img && img->canRender(renderer()->style()->effectiveZoom());
    if ((!hasFillImage && !renderer()->style()->hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent())
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, tx, ty, w, h, this, op);
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
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            xOffsetOnLine += curr->width();
        int startX = tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
            totalWidth += curr->width();
        paintInfo.context->save();
        paintInfo.context->clip(IntRect(tx, ty, width(), height()));
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, startX, ty, totalWidth, h, this, op);
        paintInfo.context->restore();
    }
}

void InlineFlowBox::paintBoxShadow(GraphicsContext* context, RenderStyle* s, ShadowStyle shadowStyle, int tx, int ty, int w, int h)
{
    if ((!prevLineBox() && !nextLineBox()) || !parent())
        boxModelObject()->paintBoxShadow(context, tx, ty, w, h, s, shadowStyle);
    else {
        // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
        // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
        boxModelObject()->paintBoxShadow(context, tx, ty, w, h, s, shadowStyle, includeLeftEdge(), includeRightEdge());
    }
}

void InlineFlowBox::paintBoxDecorations(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!renderer()->shouldPaintWithinRoot(paintInfo) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
        return;

    int x = m_x;
    int y = m_y;
    int w = width();
    int h = height();

    // Constrain our background/border painting to the line top and bottom if necessary.
    bool strictMode = renderer()->document()->inStrictMode();
    if (!hasTextChildren() && !strictMode) {
        RootInlineBox* rootBox = root();
        int bottom = min(rootBox->lineBottom(), y + h);
        y = max(rootBox->lineTop(), y);
        h = bottom - y;
    }
    
    // Move x/y to our coordinates.
    tx += x;
    ty += y;
    
    GraphicsContext* context = paintInfo.context;
    
    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    RenderStyle* styleToUse = renderer()->style(m_firstLine);
    if ((!parent() && m_firstLine && styleToUse != renderer()->style()) || (parent() && renderer()->hasBoxDecorations())) {
        // Shadow comes first and is behind the background and border.
        if (styleToUse->boxShadow())
            paintBoxShadow(context, styleToUse, Normal, tx, ty, w, h);

        Color c = styleToUse->visitedDependentColor(CSSPropertyBackgroundColor);
        paintFillLayers(paintInfo, c, styleToUse->backgroundLayers(), tx, ty, w, h);

        if (styleToUse->boxShadow())
            paintBoxShadow(context, styleToUse, Inset, tx, ty, w, h);

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
                for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
                    xOffsetOnLine += curr->width();
                int startX = tx - xOffsetOnLine;
                int totalWidth = xOffsetOnLine;
                for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
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

    int x = m_x;
    int y = m_y;
    int w = width();
    int h = height();

    // Constrain our background/border painting to the line top and bottom if necessary.
    bool strictMode = renderer()->document()->inStrictMode();
    if (!hasTextChildren() && !strictMode) {
        RootInlineBox* rootBox = root();
        int bottom = min(rootBox->lineBottom(), y + h);
        y = max(rootBox->lineTop(), y);
        h = bottom - y;
    }
    
    // Move x/y to our coordinates.
    tx += x;
    ty += y;

    const NinePieceImage& maskNinePieceImage = renderer()->style()->maskBoxImage();
    StyleImage* maskBoxImage = renderer()->style()->maskBoxImage().image();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = renderer()->hasLayer() && boxModelObject()->layer()->hasCompositedMask();
    CompositeOperator compositeOp = CompositeSourceOver;
    if (!compositedMask) {
        if ((maskBoxImage && renderer()->style()->maskLayers()->hasImage()) || renderer()->style()->maskLayers()->next())
            pushTransparencyLayer = true;
        
        compositeOp = CompositeDestinationIn;
        if (pushTransparencyLayer) {
            paintInfo.context->setCompositeOperation(CompositeDestinationIn);
            paintInfo.context->beginTransparencyLayer(1.0f);
            compositeOp = CompositeSourceOver;
        }
    }

    paintFillLayers(paintInfo, Color(), renderer()->style()->maskLayers(), tx, ty, w, h, compositeOp);
    
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
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            xOffsetOnLine += curr->width();
        int startX = tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
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
        underline = overline = linethrough = styleToUse->visitedDependentColor(CSSPropertyColor);
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
        const ShadowData* shadow = styleToUse->textShadow();
        if (!linesAreOpaque && shadow && shadow->next()) {
            IntRect clipRect(tx, ty, w, baselinePos + 2);
            for (const ShadowData* s = shadow; s; s = s->next()) {
                IntRect shadowRect(tx, ty, w, baselinePos + 2);
                shadowRect.inflate(s->blur());
                shadowRect.move(s->x(), s->y());
                clipRect.unite(shadowRect);
                extraOffset = max(extraOffset, max(0, s->y()) + s->blur());
            }
            context->save();
            context->clip(clipRect);
            extraOffset += baselinePos + 2;
            ty += extraOffset;
            setClip = true;
        }

        ColorSpace colorSpace = renderer()->style()->colorSpace();
        bool setShadow = false;
        do {
            if (shadow) {
                if (!shadow->next()) {
                    // The last set of lines paints normally inside the clip.
                    ty -= extraOffset;
                    extraOffset = 0;
                }
                context->setShadow(IntSize(shadow->x(), shadow->y() - extraOffset), shadow->blur(), shadow->color(), colorSpace);
                setShadow = true;
                shadow = shadow->next();
            }

            if (paintUnderline) {
                context->setStrokeColor(underline, colorSpace);
                context->setStrokeStyle(SolidStroke);
                // Leave one pixel of white between the baseline and the underline.
                context->drawLineForText(IntPoint(tx, ty + baselinePos + 1), w, isPrinting);
            }
            if (paintOverline) {
                context->setStrokeColor(overline, colorSpace);
                context->setStrokeStyle(SolidStroke);
                context->drawLineForText(IntPoint(tx, ty), w, isPrinting);
            }
            if (paintLineThrough) {
                context->setStrokeColor(linethrough, colorSpace);
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

InlineBox* InlineFlowBox::firstLeafChild() const
{
    InlineBox* leaf = 0;
    for (InlineBox* child = firstChild(); child && !leaf; child = child->nextOnLine())
        leaf = child->isLeaf() ? child : static_cast<InlineFlowBox*>(child)->firstLeafChild();
    return leaf;
}

InlineBox* InlineFlowBox::lastLeafChild() const
{
    InlineBox* leaf = 0;
    for (InlineBox* child = lastChild(); child && !leaf; child = child->prevOnLine())
        leaf = child->isLeaf() ? child : static_cast<InlineFlowBox*>(child)->lastLeafChild();
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

    while (box) {
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
