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
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderTableCell.h"
#include "RootInlineBox.h"
#include "Text.h"
#include "VerticalPositionCache.h"

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

int InlineFlowBox::getFlowSpacingLogicalWidth()
{
    int totWidth = marginBorderPaddingLogicalLeft() + marginBorderPaddingLogicalRight();
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->isInlineFlowBox())
            totWidth += static_cast<InlineFlowBox*>(curr)->getFlowSpacingLogicalWidth();
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
    child->setIsHorizontal(isHorizontal());
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
        bool ltr = renderer()->style()->isLeftToRightDirection();

        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line (unless we are a continuation).
        RenderLineBoxList* lineBoxList = rendererLineBoxes();
        if (!lineBoxList->firstLineBox()->isConstructed() && !renderer()->isInlineElementContinuation()) {
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

int InlineFlowBox::placeBoxesInInlineDirection(int logicalLeft, bool& needsWordSpacing, GlyphOverflowAndFallbackFontsMap& textBoxDataMap)
{
    // Set our x position.
    setLogicalLeft(logicalLeft);

    int logicalLeftLayoutOverflow = logicalLeft;
    int logicalRightLayoutOverflow = logicalLeft;
    int logicalLeftVisualOverflow = logicalLeft;
    int logicalRightVisualOverflow = logicalLeft;

    int boxShadowLogicalLeft;
    int boxShadowLogicalRight;
    renderer()->style(m_firstLine)->getBoxShadowInlineDirectionExtent(boxShadowLogicalLeft, boxShadowLogicalRight);

    logicalLeftVisualOverflow = min(logicalLeft + boxShadowLogicalLeft, logicalLeftVisualOverflow);

    int startLogicalLeft = logicalLeft;
    logicalLeft += borderLogicalLeft() + paddingLogicalLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isText()) {
            InlineTextBox* text = static_cast<InlineTextBox*>(curr);
            RenderText* rt = toRenderText(text->renderer());
            if (rt->textLength()) {
                if (needsWordSpacing && isSpaceOrNewline(rt->characters()[text->start()]))
                    logicalLeft += rt->style(m_firstLine)->font().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt->characters()[text->end()]);
            }
            text->setLogicalLeft(logicalLeft);
            
            int strokeOverflow = static_cast<int>(ceilf(rt->style()->textStrokeWidth() / 2.0f));
            
            // If letter-spacing is negative, we should factor that into right layout overflow. (Even in RTL, letter-spacing is
            // applied to the right, so this is not an issue with left overflow.
            int letterSpacing = min(0, (int)rt->style(m_firstLine)->font().letterSpacing());
            logicalRightLayoutOverflow = max(logicalLeft + text->logicalWidth() - letterSpacing, logicalRightLayoutOverflow);

            GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(static_cast<InlineTextBox*>(curr));
            GlyphOverflow* glyphOverflow = it == textBoxDataMap.end() ? 0 : &it->second.second;

            int logicalLeftGlyphOverflow = -strokeOverflow - (glyphOverflow ? glyphOverflow->left : 0);
            int logicalRightGlyphOverflow = strokeOverflow - letterSpacing + (glyphOverflow ? glyphOverflow->right : 0);
            
            int childOverflowLogicalLeft = logicalLeftGlyphOverflow;
            int childOverflowLogicalRight = logicalRightGlyphOverflow;
            int textShadowLogicalLeft;
            int textShadowLogicalRight;
            rt->style(m_firstLine)->getTextShadowInlineDirectionExtent(textShadowLogicalLeft, textShadowLogicalRight);
            childOverflowLogicalLeft = min(childOverflowLogicalLeft, textShadowLogicalLeft + logicalLeftGlyphOverflow);
            childOverflowLogicalRight = max(childOverflowLogicalRight, textShadowLogicalRight + logicalRightGlyphOverflow);
            logicalLeftVisualOverflow = min(logicalLeft + childOverflowLogicalLeft, logicalLeftVisualOverflow);
            logicalRightVisualOverflow = max(logicalLeft + text->logicalWidth() + childOverflowLogicalRight, logicalRightVisualOverflow);
            
            logicalLeft += text->logicalWidth();
        } else {
            if (curr->renderer()->isPositioned()) {
                if (curr->renderer()->parent()->style()->isLeftToRightDirection())
                    curr->setLogicalLeft(logicalLeft);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    curr->setLogicalLeft(root()->block()->logicalWidth() - logicalLeft);
                continue; // The positioned object has no effect on the width.
            }
            if (curr->renderer()->isRenderInline()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                logicalLeft += flow->marginLogicalLeft();
                logicalLeft = flow->placeBoxesInInlineDirection(logicalLeft, needsWordSpacing, textBoxDataMap);
                logicalLeft += flow->marginLogicalRight();
                logicalLeftLayoutOverflow = min(logicalLeftLayoutOverflow, flow->logicalLeftLayoutOverflow());
                logicalRightLayoutOverflow = max(logicalRightLayoutOverflow, flow->logicalRightLayoutOverflow());
                logicalLeftVisualOverflow = min(logicalLeftVisualOverflow, flow->logicalLeftVisualOverflow());
                logicalRightVisualOverflow = max(logicalRightVisualOverflow, flow->logicalRightVisualOverflow());
            } else if (!curr->renderer()->isListMarker() || toRenderListMarker(curr->renderer())->isInside()) {
                // The box can have a different writing-mode than the overall line, so this is a bit complicated.
                // Just get all the physical margin and overflow values by hand based off |isVertical|.
                int logicalLeftMargin = isHorizontal() ? curr->boxModelObject()->marginLeft() : curr->boxModelObject()->marginTop();
                int logicalRightMargin = isHorizontal() ? curr->boxModelObject()->marginRight() : curr->boxModelObject()->marginBottom();
                
                logicalLeft += logicalLeftMargin;
                curr->setLogicalLeft(logicalLeft);
                       
                RenderBox* box = toRenderBox(curr->renderer());

                int childOverflowLogicalLeft = box->hasOverflowClip() ? 0 : (isHorizontal() ? box->leftLayoutOverflow() : box->topLayoutOverflow());
                int childOverflowLogicalRight = box->hasOverflowClip() ? curr->logicalWidth() : (isHorizontal() ? box->rightLayoutOverflow() : box->bottomLayoutOverflow());
                
                logicalLeftLayoutOverflow = min(logicalLeft + childOverflowLogicalLeft, logicalLeftLayoutOverflow);
                logicalRightLayoutOverflow = max(logicalLeft + childOverflowLogicalRight, logicalRightLayoutOverflow);

                logicalLeftVisualOverflow = min(logicalLeft + (isHorizontal() ? box->leftVisualOverflow() : box->topVisualOverflow()), logicalLeftVisualOverflow);
                logicalRightVisualOverflow = max(logicalLeft + (isHorizontal() ? box->rightVisualOverflow() : box->bottomVisualOverflow()), logicalRightVisualOverflow);
               
                logicalLeft += curr->logicalWidth() + logicalRightMargin;
            }
        }
    }

    logicalLeft += borderLogicalRight() + paddingLogicalRight();
    setLogicalWidth(logicalLeft - startLogicalLeft);
    logicalRightVisualOverflow = max(logicalLeft + boxShadowLogicalRight, logicalRightVisualOverflow);
    logicalRightLayoutOverflow = max(logicalLeft, logicalRightLayoutOverflow);

    setInlineDirectionOverflowPositions(logicalLeftLayoutOverflow, logicalRightLayoutOverflow, logicalLeftVisualOverflow, logicalRightVisualOverflow);
    return logicalLeft;
}

bool InlineFlowBox::requiresIdeographicBaseline(const GlyphOverflowAndFallbackFontsMap& textBoxDataMap) const
{
    if (isHorizontal())
        return false;
    
    if (renderer()->style(m_firstLine)->font().primaryFont()->orientation() == Vertical)
        return true;

    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (curr->isInlineFlowBox()) {
            if (static_cast<InlineFlowBox*>(curr)->requiresIdeographicBaseline(textBoxDataMap))
                return true;
        } else {
            if (curr->renderer()->style(m_firstLine)->font().primaryFont()->orientation() == Vertical)
                return true;
            
            const Vector<const SimpleFontData*>* usedFonts = 0;
            if (curr->isInlineTextBox()) {
                GlyphOverflowAndFallbackFontsMap::const_iterator it = textBoxDataMap.find(static_cast<InlineTextBox*>(curr));
                usedFonts = it == textBoxDataMap.end() ? 0 : &it->second.first;
            }

            if (usedFonts) {
                for (size_t i = 0; i < usedFonts->size(); ++i) {
                    if (usedFonts->at(i)->orientation() == Vertical)
                        return true;
                }
            }
        }
    }
    
    return false;
}

void InlineFlowBox::adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                              int maxPositionTop, int maxPositionBottom)
{
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        // The computed lineheight needs to be extended for the
        // positioned elements
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        if (curr->logicalTop() == PositionTop || curr->logicalTop() == PositionBottom) {
            int lineHeight = curr->lineHeight();
            if (curr->logicalTop() == PositionTop) {
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

static int verticalPositionForBox(InlineBox* box, FontBaseline baselineType, bool firstLine, VerticalPositionCache& verticalPositionCache)
{
    if (box->renderer()->isText())
        return box->parent()->logicalTop();
    
    RenderBoxModelObject* renderer = box->boxModelObject();
    ASSERT(renderer->isInline());
    if (!renderer->isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    if (firstLine && !renderer->document()->usesFirstLineRules())
        firstLine = false;

    // Check the cache.
    bool isRenderInline = renderer->isRenderInline();
    if (isRenderInline && !firstLine) {
        int verticalPosition = verticalPositionCache.get(renderer, baselineType);
        if (verticalPosition != PositionUndefined)
            return verticalPosition;
    }

    int verticalPosition = 0;
    EVerticalAlign verticalAlign = renderer->style()->verticalAlign();
    if (verticalAlign == TOP)
        verticalPosition = PositionTop;
    else if (verticalAlign == BOTTOM)
        verticalPosition = PositionBottom;
    else {
        RenderObject* parent = renderer->parent();
        if (parent->isRenderInline() && parent->style()->verticalAlign() != TOP && parent->style()->verticalAlign() != BOTTOM)
            verticalPosition = box->parent()->logicalTop();
        
        if (verticalAlign != BASELINE) {
            const Font& font = parent->style(firstLine)->font();
            int fontSize = font.pixelSize();

            LineDirectionMode lineDirection = parent->style()->isHorizontalWritingMode() ? HorizontalLine : VerticalLine;

            if (verticalAlign == SUB)
                verticalPosition += fontSize / 5 + 1;
            else if (verticalAlign == SUPER)
                verticalPosition -= fontSize / 3 + 1;
            else if (verticalAlign == TEXT_TOP)
                verticalPosition += renderer->baselinePosition(baselineType, firstLine, lineDirection) - font.ascent(baselineType);
            else if (verticalAlign == MIDDLE)
                verticalPosition += -static_cast<int>(font.xHeight() / 2) - renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType, firstLine, lineDirection);
            else if (verticalAlign == TEXT_BOTTOM) {
                verticalPosition += font.descent(baselineType);
                // lineHeight - baselinePosition is always 0 for replaced elements (except inline blocks), so don't bother wasting time in that case.
                if (!renderer->isReplaced() || renderer->isInlineBlockOrInlineTable())
                    verticalPosition -= (renderer->lineHeight(firstLine, lineDirection) - renderer->baselinePosition(baselineType, firstLine, lineDirection));
            } else if (verticalAlign == BASELINE_MIDDLE)
                verticalPosition += -renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType, firstLine, lineDirection);
            else if (verticalAlign == LENGTH)
                verticalPosition -= renderer->style()->verticalAlignLength().calcValue(renderer->lineHeight(firstLine, lineDirection));
        }
    }

    // Store the cached value.
    if (isRenderInline && !firstLine)
        verticalPositionCache.set(renderer, baselineType, verticalPosition);

    return verticalPosition;
}

void InlineFlowBox::computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                             int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent,
                                             bool strictMode, GlyphOverflowAndFallbackFontsMap& textBoxDataMap,
                                             FontBaseline baselineType, VerticalPositionCache& verticalPositionCache)
{
    // The primary purpose of this function is to compute the maximal ascent and descent values for
    // a line.
    //
    // The maxAscent value represents the distance of the highest point of any box (including line-height) from
    // the root box's baseline.  The maxDescent value represents the distance of the lowest point of any box
    // (also including line-height) from the root box baseline.  These values can be negative.
    //
    // A secondary purpose of this function is to store the offset of very box's baseline from the root box's
    // baseline.  This information is cached in the logicalTop() of every box. We're effectively just using
    // the logicalTop() as scratch space. 
    if (isRootInlineBox()) {
        // Examine our root box.
        int height = lineHeight();
        int baseline = baselinePosition(baselineType);
        if (hasTextChildren() || strictMode) {
            int ascent = baseline;
            int descent = height - ascent;
            if (maxAscent < ascent || !setMaxAscent) {
                maxAscent = ascent;
                setMaxAscent = true;
            }
            if (maxDescent < descent || !setMaxDescent) {
                maxDescent = descent;
                setMaxDescent = true;
            }
        }
    }

    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        bool isInlineFlow = curr->isInlineFlowBox();
        
        // Because a box can be positioned such that it ends up fully above or fully below the
        // root line box, we only consider it to affect the maxAscent and maxDescent values if some
        // part of the box (EXCLUDING line-height) is above (for ascent) or below (for descent) the root box's baseline.
        bool affectsAscent = false;
        bool affectsDescent = false;
        
        // The verticalPositionForBox function returns the distance between the child box's baseline
        // and the root box's baseline.  The value is negative if the child box's baseline is above the
        // root box's baseline, and it is positive if the child box's baseline is below the root box's baseline.
        curr->setLogicalTop(verticalPositionForBox(curr, baselineType, m_firstLine, verticalPositionCache));
        
        int lineHeight;
        int baseline;
        Vector<const SimpleFontData*>* usedFonts = 0;
        if (curr->isInlineTextBox()) {
            GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(static_cast<InlineTextBox*>(curr));
            usedFonts = it == textBoxDataMap.end() ? 0 : &it->second.first;
        }

        if (usedFonts && curr->renderer()->style(m_firstLine)->lineHeight().isNegative()) {
            usedFonts->append(curr->renderer()->style(m_firstLine)->font().primaryFont());
            Length parentLineHeight = curr->renderer()->parent()->style()->lineHeight();
            bool baselineSet = false;
            baseline = 0;
            int baselineToBottom = 0;
            for (size_t i = 0; i < usedFonts->size(); ++i) {
                int halfLeading = (usedFonts->at(i)->lineSpacing() - usedFonts->at(i)->height()) / 2;
                int usedFontAscent = halfLeading + usedFonts->at(i)->ascent(baselineType);
                int usedFontDescent = usedFonts->at(i)->lineSpacing() - usedFontAscent;
                if (!baselineSet) {
                    baselineSet = true;
                    baseline = usedFontAscent;
                    baselineToBottom = usedFontDescent;
                } else {
                    baseline = max(baseline, usedFontAscent);
                    baselineToBottom = max(baselineToBottom, usedFontDescent);
                }
            }
            lineHeight = baseline + baselineToBottom;
        } else {
            lineHeight = curr->lineHeight();
            baseline = curr->baselinePosition(baselineType);
            if (curr->isText() || isInlineFlow) {
                // Examine the font box for inline flows and text boxes to see if any part of it is above the baseline.
                // If the top of our font box relative to the root box baseline is above the root box baseline, then
                // we are contributing to the maxAscent value.
                affectsAscent = curr->renderer()->style(m_firstLine)->font().ascent(baselineType) - curr->logicalTop() > 0;
                
                // Descent is similar.  If any part of our font box is below the root box's baseline, then
                // we contribute to the maxDescent value.
                affectsDescent = curr->renderer()->style(m_firstLine)->font().descent(baselineType) + curr->logicalTop() > 0;
            } else {
                // Replaced elements always affect both the ascent and descent.
                affectsAscent = true;
                affectsDescent = true;
            }
        }

        if (curr->logicalTop() == PositionTop) {
            if (maxPositionTop < lineHeight)
                maxPositionTop = lineHeight;
        } else if (curr->logicalTop() == PositionBottom) {
            if (maxPositionBottom < lineHeight)
                maxPositionBottom = lineHeight;
        } else if ((!isInlineFlow || static_cast<InlineFlowBox*>(curr)->hasTextChildren()) || curr->boxModelObject()->hasInlineDirectionBordersOrPadding() || strictMode) {
            // Note that these values can be negative.  Even though we only affect the maxAscent and maxDescent values
            // if our box (excluding line-height) was above (for ascent) or below (for descent) the root baseline, once you factor in line-height
            // the final box can end up being fully above or fully below the root box's baseline!  This is ok, but what it
            // means is that ascent and descent (including leading), can end up being negative.  The setMaxAscent and
            // setMaxDescent booleans are used to ensure that we're willing to initially set maxAscent/Descent to negative
            // values.
            int ascent = baseline - curr->logicalTop();
            int descent = lineHeight - ascent;
            if (affectsAscent && (maxAscent < ascent || !setMaxAscent)) {
                maxAscent = ascent;
                setMaxAscent = true;
            }
            if (affectsDescent && (maxDescent < descent || !setMaxDescent)) {
                maxDescent = descent;
                setMaxDescent = true;
            }
        }

        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent,
                                                                        setMaxAscent, setMaxDescent, strictMode, textBoxDataMap,
                                                                        baselineType, verticalPositionCache);
    }
}

void InlineFlowBox::placeBoxesInBlockDirection(int top, int maxHeight, int maxAscent, bool strictMode, int& lineTop, int& lineBottom, bool& setLineTop,
                                               int& lineTopIncludingMargins, int& lineBottomIncludingMargins, bool& containsRuby, FontBaseline baselineType)
{
    if (isRootInlineBox())
        setLogicalTop(top + maxAscent - baselinePosition(baselineType)); // Place our root box.

    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        bool isInlineFlow = curr->isInlineFlowBox();
        if (isInlineFlow)
            static_cast<InlineFlowBox*>(curr)->placeBoxesInBlockDirection(top, maxHeight, maxAscent, strictMode, lineTop, lineBottom, setLineTop,
                                                                          lineTopIncludingMargins, lineBottomIncludingMargins, containsRuby, baselineType);

        bool childAffectsTopBottomPos = true;
        if (curr->logicalTop() == PositionTop)
            curr->setLogicalTop(top);
        else if (curr->logicalTop() == PositionBottom)
            curr->setLogicalTop(top + maxHeight - curr->lineHeight());
        else {
            if ((isInlineFlow && !static_cast<InlineFlowBox*>(curr)->hasTextChildren()) && !curr->boxModelObject()->hasInlineDirectionBordersOrPadding() && !strictMode)
                childAffectsTopBottomPos = false;
            int posAdjust = maxAscent - curr->baselinePosition(baselineType);
            curr->setLogicalTop(curr->logicalTop() + top + posAdjust);
        }
        
        int newLogicalTop = curr->logicalTop();
        int newLogicalTopIncludingMargins;
        int boxHeight = curr->logicalHeight();
        int boxHeightIncludingMargins = boxHeight;
            
        if (curr->isText() || curr->isInlineFlowBox()) {
            const Font& font = curr->renderer()->style(m_firstLine)->font();
            newLogicalTop += curr->baselinePosition(baselineType) - font.ascent(baselineType);
            if (curr->isInlineFlowBox()) {
                RenderBoxModelObject* boxObject = toRenderBoxModelObject(curr->renderer());
                newLogicalTop -= boxObject->style(m_firstLine)->isHorizontalWritingMode() ? boxObject->borderTop() + boxObject->paddingTop() : 
                                 boxObject->borderRight() + boxObject->paddingRight();
            }
            newLogicalTopIncludingMargins = newLogicalTop;
        } else if (!curr->renderer()->isBR()) {
            RenderBox* box = toRenderBox(curr->renderer());
            newLogicalTopIncludingMargins = newLogicalTop;
            int overSideMargin = curr->isHorizontal() ? box->marginTop() : box->marginRight();
            int underSideMargin = curr->isHorizontal() ? box->marginBottom() : box->marginLeft();
            newLogicalTop += overSideMargin;
            boxHeightIncludingMargins += overSideMargin + underSideMargin;
        }

        curr->setLogicalTop(newLogicalTop);

        if (childAffectsTopBottomPos) {
            if (curr->renderer()->isRubyRun()) {
                // Treat the leading on the first and last lines of ruby runs as not being part of the overall lineTop/lineBottom.
                // Really this is a workaround hack for the fact that ruby should have been done as line layout and not done using
                // inline-block.
                containsRuby = true;
                RenderRubyRun* rubyRun = static_cast<RenderRubyRun*>(curr->renderer());
                if (RenderRubyBase* rubyBase = rubyRun->rubyBase()) {
                    int bottomRubyBaseLeading = (curr->logicalHeight() - rubyBase->logicalBottom()) + rubyBase->logicalHeight() - (rubyBase->lastRootBox() ? rubyBase->lastRootBox()->lineBottom() : 0);
                    int topRubyBaseLeading = rubyBase->logicalTop() + (rubyBase->firstRootBox() ? rubyBase->firstRootBox()->lineTop() : 0);
                    newLogicalTop += !renderer()->style()->isFlippedLinesWritingMode() ? topRubyBaseLeading : bottomRubyBaseLeading;
                    boxHeight -= (topRubyBaseLeading + bottomRubyBaseLeading);
                }
            }
            
            if (!setLineTop) {
                setLineTop = true;
                lineTop = newLogicalTop;
                lineTopIncludingMargins = min(lineTop, newLogicalTopIncludingMargins);
            } else {
                lineTop = min(lineTop, newLogicalTop);
                lineTopIncludingMargins = min(lineTop, min(lineTopIncludingMargins, newLogicalTopIncludingMargins));
            }
            lineBottom = max(lineBottom, newLogicalTop + boxHeight);
            lineBottomIncludingMargins = max(lineBottom, max(lineBottomIncludingMargins, newLogicalTopIncludingMargins + boxHeightIncludingMargins));
        }
    }

    if (isRootInlineBox()) {
        const Font& font = renderer()->style(m_firstLine)->font();
        setLogicalTop(logicalTop() + baselinePosition(baselineType) - font.ascent(baselineType));
        
        if (hasTextChildren() || strictMode) {
            if (!setLineTop) {
                setLineTop = true;
                lineTop = logicalTop();
                lineTopIncludingMargins = lineTop;
            } else {
                lineTop = min(lineTop, logicalTop());
                lineTopIncludingMargins = min(lineTop, lineTopIncludingMargins);
            }
            lineBottom = max(lineBottom, logicalTop() + logicalHeight());
            lineBottomIncludingMargins = max(lineBottom, lineBottomIncludingMargins);
        }
        
        if (renderer()->style()->isFlippedLinesWritingMode())
            flipLinesInBlockDirection(lineTopIncludingMargins, lineBottomIncludingMargins);
    }
}

void InlineFlowBox::flipLinesInBlockDirection(int lineTop, int lineBottom)
{
    // Flip the box on the line such that the top is now relative to the lineBottom instead of the lineTop.
    setLogicalTop(lineBottom - (logicalTop() - lineTop) - logicalHeight());
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders aren't affected here.
        
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->flipLinesInBlockDirection(lineTop, lineBottom);
        else
            curr->setLogicalTop(lineBottom - (curr->logicalTop() - lineTop) - curr->logicalHeight());
    }
}

void InlineFlowBox::computeBlockDirectionOverflow(int lineTop, int lineBottom, bool strictMode, GlyphOverflowAndFallbackFontsMap& textBoxDataMap)
{
    bool isFlippedLine = renderer()->style(m_firstLine)->isFlippedLinesWritingMode();

    int boxHeight = logicalHeight();

    // Any spillage outside of the line top and bottom is not considered overflow.  We just ignore this, since it only happens
    // from the "your ascent/descent don't affect the line" quirk.
    int topOverflow = max(logicalTop(), lineTop);
    int bottomOverflow = min(logicalTop() + boxHeight, lineBottom);
    
    int topLayoutOverflow = topOverflow;
    int bottomLayoutOverflow = bottomOverflow;
    
    int topVisualOverflow = topOverflow;
    int bottomVisualOverflow = bottomOverflow;
  
    // box-shadow on root line boxes is applying to the block and not to the lines.
    if (parent()) {
        int boxShadowTop;
        int boxShadowBottom;
        renderer()->style(m_firstLine)->getBoxShadowBlockDirectionExtent(boxShadowTop, boxShadowBottom);
        
        topVisualOverflow = min(logicalTop() + boxShadowTop, topVisualOverflow);
        bottomVisualOverflow = max(logicalTop() + boxHeight + boxShadowBottom, bottomVisualOverflow);
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

            GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(static_cast<InlineTextBox*>(curr));
            GlyphOverflow* glyphOverflow = it == textBoxDataMap.end() ? 0 : &it->second.second;

            int topGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->bottom : glyphOverflow->top) : 0;
            int bottomGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->top : glyphOverflow->bottom) : 0;

            int topGlyphOverflow = -strokeOverflow - topGlyphEdge;
            int bottomGlyphOverflow = strokeOverflow + bottomGlyphEdge;

            int textShadowTop;
            int textShadowBottom;
            curr->renderer()->style(m_firstLine)->getTextShadowBlockDirectionExtent(textShadowTop, textShadowBottom);
        
            int childOverflowTop = min(textShadowTop + topGlyphOverflow, topGlyphOverflow);
            int childOverflowBottom = max(textShadowBottom + bottomGlyphOverflow, bottomGlyphOverflow);
    
            topVisualOverflow = min(curr->logicalTop() + childOverflowTop, topVisualOverflow);
            bottomVisualOverflow = max(curr->logicalTop() + text->logicalHeight() + childOverflowBottom, bottomVisualOverflow);
        } else  if (curr->renderer()->isRenderInline()) {
            InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
            flow->computeBlockDirectionOverflow(lineTop, lineBottom, strictMode, textBoxDataMap);
            topLayoutOverflow = min(topLayoutOverflow, flow->logicalTopLayoutOverflow());
            bottomLayoutOverflow = max(bottomLayoutOverflow, flow->logicalBottomLayoutOverflow());
            topVisualOverflow = min(topVisualOverflow, flow->logicalTopVisualOverflow());
            bottomVisualOverflow = max(bottomVisualOverflow, flow->logicalBottomVisualOverflow());
        } else if (!curr->boxModelObject()->hasSelfPaintingLayer()){
            // Only include overflow from replaced inlines if they do not paint themselves.
            int boxLogicalTop = curr->logicalTop();
            int childTopLayoutOverflow;
            int childBottomLayoutOverflow;
            int childTopVisualOverflow;
            int childBottomVisualOverflow;
            
            RenderBox* box = toRenderBox(curr->renderer());
            box->blockDirectionOverflow(isHorizontal(), childTopLayoutOverflow, childBottomLayoutOverflow,
                                        childTopVisualOverflow, childBottomVisualOverflow);
            
            if (box->hasOverflowClip()) {
                childTopLayoutOverflow = 0;
                childBottomLayoutOverflow = curr->logicalHeight();
            }

            topLayoutOverflow = min(boxLogicalTop + childTopLayoutOverflow, topLayoutOverflow);
            bottomLayoutOverflow = max(boxLogicalTop + childBottomLayoutOverflow, bottomLayoutOverflow);
            topVisualOverflow = min(boxLogicalTop + childTopVisualOverflow, topVisualOverflow);
            bottomVisualOverflow = max(boxLogicalTop + childBottomVisualOverflow, bottomVisualOverflow);
        }
    }
    
    setBlockDirectionOverflowPositions(topLayoutOverflow, bottomLayoutOverflow, topVisualOverflow, bottomVisualOverflow);
}

bool InlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    IntRect overflowRect(visibleOverflowRect());
    flipForWritingMode(overflowRect);
    overflowRect.move(tx, ty);
    if (!overflowRect.intersects(result.rectForPoint(x, y)))
        return false;

    // Check children first.
    for (InlineBox* curr = lastChild(); curr; curr = curr->prevOnLine()) {
        if ((curr->renderer()->isText() || !curr->boxModelObject()->hasSelfPaintingLayer()) && curr->nodeAtPoint(request, result, x, y, tx, ty)) {
            renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    }

    // Now check ourselves.
    IntPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.move(tx, ty);
    IntRect rect(boxOrigin, IntSize(width(), height()));
    if (visibleToHitTesting() && rect.intersects(result.rectForPoint(x, y))) {
        renderer()->updateHitTestResult(result, flipForWritingMode(IntPoint(x - tx, y - ty))); // Don't add in m_x or m_y here, we want coords in the containing block's space.
        if (!result.addNodeToRectBasedTestResult(renderer()->node(), x, y, rect))
            return true;
    }
    
    return false;
}

void InlineFlowBox::paint(PaintInfo& paintInfo, int tx, int ty)
{
    IntRect overflowRect(visibleOverflowRect());
    overflowRect.inflate(renderer()->maximalOutlineSize(paintInfo.phase));
    flipForWritingMode(overflowRect);
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
                bool containingBlockPaintsContinuationOutline = inlineFlow->continuation() || inlineFlow->isInlineElementContinuation();
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
                } else if (!inlineFlow->isInlineElementContinuation())
                    paintInfo.outlineObjects->add(inlineFlow);
            }
        } else if (paintInfo.phase == PaintPhaseMask) {
            paintMask(paintInfo, tx, ty);
            return;
        } else {
            // Paint our background, border and box-shadow.
            paintBoxDecorations(paintInfo, tx, ty);
        }
    }

    if (paintInfo.phase == PaintPhaseMask)
        return;

    PaintPhase paintPhase = paintInfo.phase == PaintPhaseChildOutlines ? PaintPhaseOutline : paintInfo.phase;
    PaintInfo childInfo(paintInfo);
    childInfo.phase = paintPhase;
    childInfo.updatePaintingRootForChildren(renderer());
    
    // Paint our children.
    if (paintPhase != PaintPhaseSelfOutline) {
        for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (curr->renderer()->isText() || !curr->boxModelObject()->hasSelfPaintingLayer())
                curr->paint(childInfo, tx, ty);
        }
    }
}

void InlineFlowBox::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, int _tx, int _ty, int w, int h, CompositeOperator op)
{
    if (!fillLayer)
        return;
    paintFillLayers(paintInfo, c, fillLayer->next(), _tx, _ty, w, h, op);
    paintFillLayer(paintInfo, c, fillLayer, _tx, _ty, w, h, op);
}

void InlineFlowBox::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, int tx, int ty, int w, int h, CompositeOperator op)
{
    StyleImage* img = fillLayer->image();
    bool hasFillImage = img && img->canRender(renderer()->style()->effectiveZoom());
    if ((!hasFillImage && !renderer()->style()->hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent())
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, tx, ty, w, h, this, op);
    else {
        // We have a fill image that spans multiple lines.
        // We need to adjust tx and ty by the width of all previous lines.
        // Think of background painting on inlines as though you had one long line, a single continuous
        // strip.  Even though that strip has been broken up across multiple lines, you still paint it
        // as though you had one single line.  This means each line has to pick up the background where
        // the previous line left off.
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        int logicalOffsetOnLine = 0;
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        int totalLogicalWidth = logicalOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
        int stripX = tx - (isHorizontal() ? logicalOffsetOnLine : 0);
        int stripY = ty - (isHorizontal() ? 0 : logicalOffsetOnLine);
        int stripWidth = isHorizontal() ? totalLogicalWidth : width();
        int stripHeight = isHorizontal() ? height() : totalLogicalWidth;
        paintInfo.context->save();
        paintInfo.context->clip(IntRect(tx, ty, width(), height()));
        boxModelObject()->paintFillLayerExtended(paintInfo, c, fillLayer, stripX, stripY, stripWidth, stripHeight, this, op);
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
        boxModelObject()->paintBoxShadow(context, tx, ty, w, h, s, shadowStyle, includeLogicalLeftEdge(), includeLogicalRightEdge());
    }
}

void InlineFlowBox::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
        return;

    int x = m_x;
    int y = m_y;
    int w = width();
    int h = height();

    // Constrain our background/border painting to the line top and bottom if necessary.
    bool noQuirksMode = renderer()->document()->inNoQuirksMode();
    if (!hasTextChildren() && !noQuirksMode) {
        RootInlineBox* rootBox = root();
        int& top = isHorizontal() ? y : x;
        int& logicalHeight = isHorizontal() ? h : w;
        int bottom = min(rootBox->lineBottom(), top + logicalHeight);
        top = max(rootBox->lineTop(), top);
        logicalHeight = bottom - top;
    }
    
    // Move x/y to our coordinates.
    IntRect localRect(x, y, w, h);
    flipForWritingMode(localRect);
    tx += localRect.x();
    ty += localRect.y();
    
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
                boxModelObject()->paintBorder(context, tx, ty, w, h, renderer()->style(), includeLogicalLeftEdge(), includeLogicalRightEdge());
            else {
                // We have a border image that spans multiple lines.
                // We need to adjust tx and ty by the width of all previous lines.
                // Think of border image painting on inlines as though you had one long line, a single continuous
                // strip.  Even though that strip has been broken up across multiple lines, you still paint it
                // as though you had one single line.  This means each line has to pick up the image where
                // the previous line left off.
                // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
                // but it isn't even clear how this should work at all.
                int logicalOffsetOnLine = 0;
                for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
                    logicalOffsetOnLine += curr->logicalWidth();
                int totalLogicalWidth = logicalOffsetOnLine;
                for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
                    totalLogicalWidth += curr->logicalWidth();
                int stripX = tx - (isHorizontal() ? logicalOffsetOnLine : 0);
                int stripY = ty - (isHorizontal() ? 0 : logicalOffsetOnLine);
                int stripWidth = isHorizontal() ? totalLogicalWidth : w;
                int stripHeight = isHorizontal() ? h : totalLogicalWidth;
                context->save();
                context->clip(IntRect(tx, ty, w, h));
                boxModelObject()->paintBorder(context, stripX, stripY, stripWidth, stripHeight, renderer()->style());
                context->restore();
            }
        }
    }
}

void InlineFlowBox::paintMask(PaintInfo& paintInfo, int tx, int ty)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    int x = m_x;
    int y = m_y;
    int w = width();
    int h = height();

    // Constrain our background/border painting to the line top and bottom if necessary.
    bool noQuirksMode = renderer()->document()->inNoQuirksMode();
    if (!hasTextChildren() && !noQuirksMode) {
        RootInlineBox* rootBox = root();
        int& top = isHorizontal() ? y : x;
        int& logicalHeight = isHorizontal() ? h : w;
        int bottom = min(rootBox->lineBottom(), top + logicalHeight);
        top = max(rootBox->lineTop(), top);
        logicalHeight = bottom - top;
    }
    
    // Move x/y to our coordinates.
    IntRect localRect(x, y, w, h);
    flipForWritingMode(localRect);
    tx += localRect.x();
    ty += localRect.y();

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
        int logicalOffsetOnLine = 0;
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        int totalLogicalWidth = logicalOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
        int stripX = tx - (isHorizontal() ? logicalOffsetOnLine : 0);
        int stripY = ty - (isHorizontal() ? 0 : logicalOffsetOnLine);
        int stripWidth = isHorizontal() ? totalLogicalWidth : w;
        int stripHeight = isHorizontal() ? h : totalLogicalWidth;
        paintInfo.context->save();
        paintInfo.context->clip(IntRect(tx, ty, w, h));
        boxModelObject()->paintNinePieceImage(paintInfo.context, stripX, stripY, stripWidth, stripHeight, renderer()->style(), maskNinePieceImage, compositeOp);
        paintInfo.context->restore();
    }
    
    if (pushTransparencyLayer)
        paintInfo.context->endTransparencyLayer();
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
            visibleLeftEdge += box->logicalWidth();
            box = box->nextOnLine();
        }
        else {
            visibleRightEdge -= box->logicalWidth();
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

int InlineFlowBox::computeBlockDirectionRubyAdjustment(int allowedPosition) const
{
    int result = 0;
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (curr->isInlineFlowBox())
            result = max(result, static_cast<InlineFlowBox*>(curr)->computeBlockDirectionRubyAdjustment(allowedPosition));
        
        if (curr->renderer()->isReplaced() && curr->renderer()->isRubyRun()) {
            RenderRubyRun* rubyRun = static_cast<RenderRubyRun*>(curr->renderer());
            RenderRubyText* rubyText = rubyRun->rubyText();
            if (!rubyText)
                continue;
            
            if (!rubyRun->style()->isFlippedLinesWritingMode()) {
                int topOfFirstRubyTextLine = rubyText->logicalTop() + (rubyText->firstRootBox() ? rubyText->firstRootBox()->lineTop() : 0);
                if (topOfFirstRubyTextLine >= 0)
                    continue;
                topOfFirstRubyTextLine += curr->logicalTop();
                result = max(result, allowedPosition - topOfFirstRubyTextLine);
            } else {
                int bottomOfLastRubyTextLine = rubyText->logicalTop() + (rubyText->lastRootBox() ? rubyText->lastRootBox()->lineBottom() : rubyText->logicalHeight());
                if (bottomOfLastRubyTextLine <= curr->logicalHeight())
                    continue;
                bottomOfLastRubyTextLine += curr->logicalTop();
                result = max(result, bottomOfLastRubyTextLine - allowedPosition);
            }
        }
    }
    return result;
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
