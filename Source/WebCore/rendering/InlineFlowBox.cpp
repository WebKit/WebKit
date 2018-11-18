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

#include "CSSPropertyNames.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "HitTestResult.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RootInlineBox.h"
#include "Settings.h"
#include "Text.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFlowBox);

struct SameSizeAsInlineFlowBox : public InlineBox {
    uint32_t bitfields : 23;
    void* pointers[5];
};

COMPILE_ASSERT(sizeof(InlineFlowBox) == sizeof(SameSizeAsInlineFlowBox), InlineFlowBox_should_stay_small);

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED

InlineFlowBox::~InlineFlowBox()
{
    setHasBadChildList();
}

void InlineFlowBox::setHasBadChildList()
{
    assertNotDeleted();
    if (m_hasBadChildList)
        return;
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->setHasBadParent();
    m_hasBadChildList = true;
}

#endif

LayoutUnit InlineFlowBox::getFlowSpacingLogicalWidth()
{
    LayoutUnit totalWidth = marginBorderPaddingLogicalLeft() + marginBorderPaddingLogicalRight();
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (is<InlineFlowBox>(*child))
            totalWidth += downcast<InlineFlowBox>(*child).getFlowSpacingLogicalWidth();
    }
    return totalWidth;
}

static void setHasTextDescendantsOnAncestors(InlineFlowBox* box)
{
    while (box && !box->hasTextDescendants()) {
        box->setHasTextDescendants();
        box = box->parent();
    }
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
    child->setIsFirstLine(isFirstLine());
    child->setIsHorizontal(isHorizontal());
    if (child->behavesLikeText()) {
        if (child->renderer().parent() == &renderer())
            m_hasTextChildren = true;
        setHasTextDescendantsOnAncestors(this);
    } else if (is<InlineFlowBox>(*child)) {
        if (downcast<InlineFlowBox>(*child).hasTextDescendants())
            setHasTextDescendantsOnAncestors(this);
    }
    if (descendantsHaveSameLineHeightAndBaseline() && !child->renderer().isOutOfFlowPositioned()) {
        const RenderStyle& parentStyle = lineStyle();
        const RenderStyle& childStyle = child->lineStyle();
        bool shouldClearDescendantsHaveSameLineHeightAndBaseline = false;
        if (child->renderer().isReplaced())
            shouldClearDescendantsHaveSameLineHeightAndBaseline = true;
        else if (child->behavesLikeText()) {
            if (child->renderer().isLineBreak() || child->renderer().parent() != &renderer()) {
                if (!parentStyle.fontCascade().fontMetrics().hasIdenticalAscentDescentAndLineGap(childStyle.fontCascade().fontMetrics())
                    || parentStyle.lineHeight() != childStyle.lineHeight()
                    || (parentStyle.verticalAlign() != VerticalAlign::Baseline && !isRootInlineBox()) || childStyle.verticalAlign() != VerticalAlign::Baseline)
                    shouldClearDescendantsHaveSameLineHeightAndBaseline = true;
            }
            if (childStyle.hasTextCombine() || childStyle.textEmphasisMark() != TextEmphasisMark::None)
                shouldClearDescendantsHaveSameLineHeightAndBaseline = true;
        } else {
            if (child->renderer().isLineBreak()) {
                // FIXME: This is dumb. We only turn off because current layout test results expect the <br> to be 0-height on the baseline.
                // Other than making a zillion tests have to regenerate results, there's no reason to ditch the optimization here.
                shouldClearDescendantsHaveSameLineHeightAndBaseline = child->renderer().isBR();
            } else {
                auto& childFlowBox = downcast<InlineFlowBox>(*child);
                // Check the child's bit, and then also check for differences in font, line-height, vertical-align
                if (!childFlowBox.descendantsHaveSameLineHeightAndBaseline()
                    || !parentStyle.fontCascade().fontMetrics().hasIdenticalAscentDescentAndLineGap(childStyle.fontCascade().fontMetrics())
                    || parentStyle.lineHeight() != childStyle.lineHeight()
                    || (parentStyle.verticalAlign() != VerticalAlign::Baseline && !isRootInlineBox()) || childStyle.verticalAlign() != VerticalAlign::Baseline
                    || childStyle.hasBorder() || childStyle.hasPadding() || childStyle.hasTextCombine())
                    shouldClearDescendantsHaveSameLineHeightAndBaseline = true;
            }
        }

        if (shouldClearDescendantsHaveSameLineHeightAndBaseline)
            clearDescendantsHaveSameLineHeightAndBaseline();
    }

    if (!child->renderer().isOutOfFlowPositioned()) {
        const RenderStyle& childStyle = child->lineStyle();
        if (child->behavesLikeText()) {
            const RenderStyle* childStyle = &child->lineStyle();
            bool hasMarkers = false;
            if (is<InlineTextBox>(child)) {
                const auto* textBox = downcast<InlineTextBox>(child);
                hasMarkers = textBox->hasMarkers();
            }
            if (childStyle->letterSpacing() < 0 || childStyle->textShadow() || childStyle->textEmphasisMark() != TextEmphasisMark::None || childStyle->hasPositiveStrokeWidth() || hasMarkers || !childStyle->textUnderlineOffset().isAuto() || !childStyle->textDecorationThickness().isAuto() || childStyle->textUnderlinePosition() != TextUnderlinePosition::Auto)
                child->clearKnownToHaveNoOverflow();
        } else if (child->renderer().isReplaced()) {
            const RenderBox& box = downcast<RenderBox>(child->renderer());
            if (box.hasRenderOverflow() || box.hasSelfPaintingLayer())
                child->clearKnownToHaveNoOverflow();
        } else if (!child->renderer().isLineBreak() && (childStyle.boxShadow() || child->boxModelObject()->hasSelfPaintingLayer()
            || (is<RenderListMarker>(child->renderer()) && !downcast<RenderListMarker>(child->renderer()).isInside())
            || childStyle.hasBorderImageOutsets()))
            child->clearKnownToHaveNoOverflow();
        else if (childStyle.hasOutlineInVisualOverflow())
            child->clearKnownToHaveNoOverflow();
        
        if (knownToHaveNoOverflow() && is<InlineFlowBox>(*child) && !downcast<InlineFlowBox>(*child).knownToHaveNoOverflow())
            clearKnownToHaveNoOverflow();
    }

    checkConsistency();
}

void InlineFlowBox::removeChild(InlineBox* child)
{
    checkConsistency();

    if (!isDirty())
        dirtyLineBoxes();

    root().childRemoved(child);

    if (child == m_firstChild)
        m_firstChild = child->nextOnLine();
    if (child == m_lastChild)
        m_lastChild = child->prevOnLine();
    if (child->nextOnLine())
        child->nextOnLine()->setPrevOnLine(child->prevOnLine());
    if (child->prevOnLine())
        child->prevOnLine()->setNextOnLine(child->nextOnLine());
    
    child->setParent(nullptr);

    checkConsistency();
}

void InlineFlowBox::deleteLine()
{
    InlineBox* child = firstChild();
    InlineBox* next = nullptr;
    while (child) {
        ASSERT(this == child->parent());
        next = child->nextOnLine();
#ifndef NDEBUG
        child->setParent(nullptr);
#endif
        child->deleteLine();
        child = next;
    }
#ifndef NDEBUG
    m_firstChild = nullptr;
    m_lastChild = nullptr;
#endif

    removeLineBoxFromRenderObject();
    delete this;
}

void InlineFlowBox::removeLineBoxFromRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().removeLineBox(this);
}

void InlineFlowBox::extractLine()
{
    if (!extracted())
        extractLineBoxFromRenderObject();
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->extractLine();
}

void InlineFlowBox::extractLineBoxFromRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().extractLineBox(this);
}

void InlineFlowBox::attachLine()
{
    if (extracted())
        attachLineBoxToRenderObject();
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->attachLine();
}

void InlineFlowBox::attachLineBoxToRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().attachLineBox(this);
}

void InlineFlowBox::adjustPosition(float dx, float dy)
{
    InlineBox::adjustPosition(dx, dy);
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
    if (m_overflow)
        m_overflow->move(dx, dy); // FIXME: Rounding error here since overflow was pixel snapped, but nobody other than list markers passes non-integral values here.
}

static inline bool isLastChildForRenderer(const RenderElement& ancestor, const RenderObject* child)
{
    if (!child)
        return false;
    
    if (child == &ancestor)
        return true;

    const RenderObject* curr = child;
    const RenderElement* parent = curr->parent();
    while (parent && (!parent->isRenderBlock() || parent->isInline())) {
        if (parent->lastChild() != curr)
            return false;
        if (parent == &ancestor)
            return true;
            
        curr = parent;
        parent = curr->parent();
    }

    return true;
}

static bool isAncestorAndWithinBlock(const RenderInline& ancestor, const RenderObject* child)
{
    const RenderObject* object = child;
    while (object && (!object->isRenderBlock() || object->isInline())) {
        if (object == &ancestor)
            return true;
        object = object->parent();
    }
    return false;
}

void InlineFlowBox::determineSpacingForFlowBoxes(bool lastLine, bool isLogicallyLastRunWrapped, RenderObject* logicallyLastRunRenderer)
{
    // All boxes start off open.  They will not apply any margins/border/padding on
    // any side.
    bool includeLeftEdge = false;
    bool includeRightEdge = false;

    // The root inline box never has borders/margins/padding.
    if (parent()) {
        const auto& inlineFlow = downcast<RenderInline>(renderer());

        bool ltr = renderer().style().isLeftToRightDirection();

        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line (unless we are a continuation).
        const auto& lineBoxList = inlineFlow.lineBoxes();
        if (!lineBoxList.firstLineBox()->isConstructed() && !inlineFlow.isContinuation()) {
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (renderer().style().boxDecorationBreak() == BoxDecorationBreak::Clone)
                includeLeftEdge = includeRightEdge = true;
            else
#endif
            if (ltr && lineBoxList.firstLineBox() == this)
                includeLeftEdge = true;
            else if (!ltr && lineBoxList.lastLineBox() == this)
                includeRightEdge = true;
        }

        if (!lineBoxList.lastLineBox()->isConstructed()) {
            bool isLastObjectOnLine = !isAncestorAndWithinBlock(inlineFlow, logicallyLastRunRenderer) || (isLastChildForRenderer(renderer(), logicallyLastRunRenderer) && !isLogicallyLastRunWrapped);

            // We include the border under these conditions:
            // (1) The next line was not created, or it is constructed. We check the previous line for rtl.
            // (2) The logicallyLastRun is not a descendant of this renderer.
            // (3) The logicallyLastRun is a descendant of this renderer, but it is the last child of this renderer and it does not wrap to the next line.
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            // (4) The decoration break is set to clone therefore there will be borders on every sides.
            if (renderer().style().boxDecorationBreak() == BoxDecorationBreak::Clone)
                includeLeftEdge = includeRightEdge = true;
            else
#endif
            if (ltr) {
                if (!nextLineBox()
                    && ((lastLine || isLastObjectOnLine) && !inlineFlow.continuation()))
                    includeRightEdge = true;
            } else {
                if ((!prevLineBox() || prevLineBox()->isConstructed())
                    && ((lastLine || isLastObjectOnLine) && !inlineFlow.continuation()))
                    includeLeftEdge = true;
            }
        }
    }

    setEdges(includeLeftEdge, includeRightEdge);

    // Recur into our children.
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (is<InlineFlowBox>(*child))
            downcast<InlineFlowBox>(*child).determineSpacingForFlowBoxes(lastLine, isLogicallyLastRunWrapped, logicallyLastRunRenderer);
    }
}

float InlineFlowBox::placeBoxesInInlineDirection(float logicalLeft, bool& needsWordSpacing)
{
    // Set our x position.
    beginPlacingBoxRangesInInlineDirection(logicalLeft);

    float startLogicalLeft = logicalLeft;
    logicalLeft += borderLogicalLeft() + paddingLogicalLeft();

    float minLogicalLeft = startLogicalLeft;
    float maxLogicalRight = logicalLeft;

    placeBoxRangeInInlineDirection(firstChild(), nullptr, logicalLeft, minLogicalLeft, maxLogicalRight, needsWordSpacing);

    logicalLeft += borderLogicalRight() + paddingLogicalRight();
    endPlacingBoxRangesInInlineDirection(startLogicalLeft, logicalLeft, minLogicalLeft, maxLogicalRight);
    return logicalLeft;
}

float InlineFlowBox::placeBoxRangeInInlineDirection(InlineBox* firstChild, InlineBox* lastChild, float& logicalLeft, float& minLogicalLeft, float& maxLogicalRight, bool& needsWordSpacing)
{
    float totalExpansion = 0;
    for (InlineBox* child = firstChild; child && child != lastChild; child = child->nextOnLine()) {
        if (is<RenderText>(child->renderer())) {
            auto& textBox = downcast<InlineTextBox>(*child);
            RenderText& renderText = textBox.renderer();
            if (renderText.text().length()) {
                if (needsWordSpacing && isSpaceOrNewline(renderText.characterAt(textBox.start())))
                    logicalLeft += textBox.lineStyle().fontCascade().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(renderText.characterAt(textBox.end()));
            }
            textBox.setLogicalLeft(logicalLeft);
            if (knownToHaveNoOverflow())
                minLogicalLeft = std::min(logicalLeft, minLogicalLeft);
            logicalLeft += textBox.logicalWidth();
            totalExpansion += textBox.expansion();
            if (knownToHaveNoOverflow())
                maxLogicalRight = std::max(logicalLeft, maxLogicalRight);
        } else {
            if (child->renderer().isOutOfFlowPositioned()) {
                if (child->renderer().parent()->style().isLeftToRightDirection())
                    child->setLogicalLeft(logicalLeft);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    child->setLogicalLeft(root().blockFlow().logicalWidth() - logicalLeft);
                continue; // The positioned object has no effect on the width.
            }
            if (is<RenderInline>(child->renderer())) {
                auto& flow = downcast<InlineFlowBox>(*child);
                logicalLeft += flow.marginLogicalLeft();
                if (knownToHaveNoOverflow())
                    minLogicalLeft = std::min(logicalLeft, minLogicalLeft);
                logicalLeft = flow.placeBoxesInInlineDirection(logicalLeft, needsWordSpacing);
                totalExpansion += flow.expansion();
                if (knownToHaveNoOverflow())
                    maxLogicalRight = std::max(logicalLeft, maxLogicalRight);
                logicalLeft += flow.marginLogicalRight();
            } else if (!is<RenderListMarker>(child->renderer()) || downcast<RenderListMarker>(child->renderer()).isInside()) {
                // The box can have a different writing-mode than the overall line, so this is a bit complicated.
                // Just get all the physical margin and overflow values by hand based off |isVertical|.
                LayoutUnit logicalLeftMargin = isHorizontal() ? child->boxModelObject()->marginLeft() : child->boxModelObject()->marginTop();
                LayoutUnit logicalRightMargin = isHorizontal() ? child->boxModelObject()->marginRight() : child->boxModelObject()->marginBottom();
                
                logicalLeft += logicalLeftMargin;
                child->setLogicalLeft(logicalLeft);
                if (knownToHaveNoOverflow())
                    minLogicalLeft = std::min(logicalLeft, minLogicalLeft);
                logicalLeft += child->logicalWidth();
                if (knownToHaveNoOverflow())
                    maxLogicalRight = std::max(logicalLeft, maxLogicalRight);
                logicalLeft += logicalRightMargin;
                // If we encounter any space after this inline block then ensure it is treated as the space between two words.
                needsWordSpacing = true;
            }
        }
    }
    setExpansionWithoutGrowing(totalExpansion);
    return logicalLeft;
}

bool InlineFlowBox::requiresIdeographicBaseline(const GlyphOverflowAndFallbackFontsMap& textBoxDataMap) const
{
    if (isHorizontal())
        return false;

    const RenderStyle& lineStyle = this->lineStyle();
    if (lineStyle.fontDescription().nonCJKGlyphOrientation() == NonCJKGlyphOrientation::Upright
        || lineStyle.fontCascade().primaryFont().hasVerticalGlyphs())
        return true;

    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (is<InlineFlowBox>(*child)) {
            if (downcast<InlineFlowBox>(*child).requiresIdeographicBaseline(textBoxDataMap))
                return true;
        } else {
            if (child->lineStyle().fontCascade().primaryFont().hasVerticalGlyphs())
                return true;
            
            const Vector<const Font*>* usedFonts = nullptr;
            if (is<InlineTextBox>(*child)) {
                GlyphOverflowAndFallbackFontsMap::const_iterator it = textBoxDataMap.find(downcast<InlineTextBox>(child));
                usedFonts = it == textBoxDataMap.end() ? nullptr : &it->value.first;
            }

            if (usedFonts) {
                for (const Font* font : *usedFonts) {
                    if (font->hasVerticalGlyphs())
                        return true;
                }
            }
        }
    }
    
    return false;
}

static bool verticalAlignApplies(const RenderObject& renderer)
{
    // http://www.w3.org/TR/CSS2/visudet.html#propdef-vertical-align - vertical-align
    // only applies to inline level and table-cell elements
    return !renderer.isText() || renderer.parent()->isInline() || renderer.parent()->isTableCell();
}

void InlineFlowBox::adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent, int maxPositionTop, int maxPositionBottom)
{
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        // The computed lineheight needs to be extended for the
        // positioned elements
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if ((child->verticalAlign() == VerticalAlign::Top || child->verticalAlign() == VerticalAlign::Bottom) && verticalAlignApplies(child->renderer())) {
            int lineHeight = child->lineHeight();
            if (child->verticalAlign() == VerticalAlign::Top) {
                if (maxAscent + maxDescent < lineHeight)
                    maxDescent = lineHeight - maxAscent;
            }
            else {
                if (maxAscent + maxDescent < lineHeight)
                    maxAscent = lineHeight - maxDescent;
            }

            if (maxAscent + maxDescent >= std::max(maxPositionTop, maxPositionBottom))
                break;
        }

        if (is<InlineFlowBox>(*child))
            downcast<InlineFlowBox>(*child).adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);
    }
}

void InlineFlowBox::computeLogicalBoxHeights(RootInlineBox& rootBox, LayoutUnit& maxPositionTop, LayoutUnit& maxPositionBottom,
    int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent,
    bool strictMode, GlyphOverflowAndFallbackFontsMap& textBoxDataMap,
    FontBaseline baselineType, VerticalPositionCache& verticalPositionCache)
{
    // The primary purpose of this function is to compute the maximal ascent and descent values for
    // a line. These values are computed based off the block's line-box-contain property, which indicates
    // what parts of descendant boxes have to fit within the line.
    //
    // The maxAscent value represents the distance of the highest point of any box (typically including line-height) from
    // the root box's baseline. The maxDescent value represents the distance of the lowest point of any box
    // (also typically including line-height) from the root box baseline. These values can be negative.
    //
    // A secondary purpose of this function is to store the offset of every box's baseline from the root box's
    // baseline. This information is cached in the logicalTop() of every box. We're effectively just using
    // the logicalTop() as scratch space.
    //
    // Because a box can be positioned such that it ends up fully above or fully below the
    // root line box, we only consider it to affect the maxAscent and maxDescent values if some
    // part of the box (EXCLUDING leading) is above (for ascent) or below (for descent) the root box's baseline.
    bool affectsAscent = false;
    bool affectsDescent = false;
    bool checkChildren = !descendantsHaveSameLineHeightAndBaseline();
    
    if (isRootInlineBox()) {
        // Examine our root box.
        int ascent = 0;
        int descent = 0;
        rootBox.ascentAndDescentForBox(rootBox, textBoxDataMap, ascent, descent, affectsAscent, affectsDescent);
        if (strictMode || hasTextChildren() || (!checkChildren && hasTextDescendants())) {
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

    if (!checkChildren)
        return;

    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        InlineFlowBox* inlineFlowBox = is<InlineFlowBox>(*child) ? downcast<InlineFlowBox>(child) : nullptr;
        
        bool affectsAscent = false;
        bool affectsDescent = false;
        
        // The verticalPositionForBox function returns the distance between the child box's baseline
        // and the root box's baseline.  The value is negative if the child box's baseline is above the
        // root box's baseline, and it is positive if the child box's baseline is below the root box's baseline.
        child->setLogicalTop(rootBox.verticalPositionForBox(child, verticalPositionCache));
        
        int ascent = 0;
        int descent = 0;
        rootBox.ascentAndDescentForBox(*child, textBoxDataMap, ascent, descent, affectsAscent, affectsDescent);

        LayoutUnit boxHeight = ascent + descent;
        if (child->verticalAlign() == VerticalAlign::Top && verticalAlignApplies(child->renderer())) {
            if (maxPositionTop < boxHeight)
                maxPositionTop = boxHeight;
        } else if (child->verticalAlign() == VerticalAlign::Bottom && verticalAlignApplies(child->renderer())) {
            if (maxPositionBottom < boxHeight)
                maxPositionBottom = boxHeight;
        } else if (!inlineFlowBox || strictMode || inlineFlowBox->hasTextChildren() || (inlineFlowBox->descendantsHaveSameLineHeightAndBaseline() && inlineFlowBox->hasTextDescendants())
                   || inlineFlowBox->renderer().hasInlineDirectionBordersOrPadding()) {
            // Note that these values can be negative.  Even though we only affect the maxAscent and maxDescent values
            // if our box (excluding line-height) was above (for ascent) or below (for descent) the root baseline, once you factor in line-height
            // the final box can end up being fully above or fully below the root box's baseline!  This is ok, but what it
            // means is that ascent and descent (including leading), can end up being negative.  The setMaxAscent and
            // setMaxDescent booleans are used to ensure that we're willing to initially set maxAscent/Descent to negative
            // values.
            ascent -= child->logicalTop();
            descent += child->logicalTop();
            if (affectsAscent && (maxAscent < ascent || !setMaxAscent)) {
                maxAscent = ascent;
                setMaxAscent = true;
            }

            if (affectsDescent && (maxDescent < descent || !setMaxDescent)) {
                maxDescent = descent;
                setMaxDescent = true;
            }
        }

        if (inlineFlowBox)
            inlineFlowBox->computeLogicalBoxHeights(rootBox, maxPositionTop, maxPositionBottom, maxAscent, maxDescent,
                                                    setMaxAscent, setMaxDescent, strictMode, textBoxDataMap,
                                                    baselineType, verticalPositionCache);
    }
}

void InlineFlowBox::placeBoxesInBlockDirection(LayoutUnit top, LayoutUnit maxHeight, int maxAscent, bool strictMode, LayoutUnit& lineTop, LayoutUnit& lineBottom, bool& setLineTop,
    LayoutUnit& lineTopIncludingMargins, LayoutUnit& lineBottomIncludingMargins, bool& hasAnnotationsBefore, bool& hasAnnotationsAfter, FontBaseline baselineType)
{
    bool isRootBox = isRootInlineBox();
    if (isRootBox) {
        const FontMetrics& fontMetrics = lineStyle().fontMetrics();
        // RootInlineBoxes are always placed on at pixel boundaries in their logical y direction. Not doing
        // so results in incorrect rendering of text decorations, most notably underlines.
        setLogicalTop(roundToInt(top + maxAscent - fontMetrics.ascent(baselineType)));
    }

    LayoutUnit adjustmentForChildrenWithSameLineHeightAndBaseline;
    if (descendantsHaveSameLineHeightAndBaseline()) {
        adjustmentForChildrenWithSameLineHeightAndBaseline = logicalTop();
        if (parent())
            adjustmentForChildrenWithSameLineHeightAndBaseline += renderer().borderAndPaddingBefore();
    }

    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (descendantsHaveSameLineHeightAndBaseline()) {
            child->adjustBlockDirectionPosition(adjustmentForChildrenWithSameLineHeightAndBaseline);
            continue;
        }

        InlineFlowBox* inlineFlowBox = is<InlineFlowBox>(*child) ? downcast<InlineFlowBox>(child) : nullptr;
        bool childAffectsTopBottomPos = true;

        if (child->verticalAlign() == VerticalAlign::Top && verticalAlignApplies(child->renderer()))
            child->setLogicalTop(top);
        else if (child->verticalAlign() == VerticalAlign::Bottom && verticalAlignApplies(child->renderer()))
            child->setLogicalTop(top + maxHeight - child->lineHeight());
        else {
            if (!strictMode && inlineFlowBox && !inlineFlowBox->hasTextChildren() && !inlineFlowBox->renderer().hasInlineDirectionBordersOrPadding()
                && !(inlineFlowBox->descendantsHaveSameLineHeightAndBaseline() && inlineFlowBox->hasTextDescendants()))
                childAffectsTopBottomPos = false;
            LayoutUnit posAdjust = maxAscent - child->baselinePosition(baselineType);
            child->setLogicalTop(child->logicalTop() + top + posAdjust);
        }

        LayoutUnit newLogicalTop = child->logicalTop();
        LayoutUnit newLogicalTopIncludingMargins = newLogicalTop;
        LayoutUnit boxHeight = child->logicalHeight();
        LayoutUnit boxHeightIncludingMargins = boxHeight;

        const RenderStyle& childLineStyle = child->lineStyle();
        if (child->behavesLikeText() || is<InlineFlowBox>(*child)) {
            const FontMetrics& fontMetrics = childLineStyle.fontMetrics();
            newLogicalTop += child->baselinePosition(baselineType) - fontMetrics.ascent(baselineType);
            if (is<InlineFlowBox>(*child)) {
                RenderBoxModelObject& boxObject = downcast<InlineFlowBox>(*child).renderer();
                newLogicalTop -= childLineStyle.isHorizontalWritingMode()
                    ? boxObject.borderTop() + boxObject.paddingTop()
                    : boxObject.borderRight() + boxObject.paddingRight();
            }
            newLogicalTopIncludingMargins = newLogicalTop;
        } else if (!child->renderer().isBR()) {
            const auto& box = downcast<RenderBox>(child->renderer());
            newLogicalTopIncludingMargins = newLogicalTop;
            // We may flip lines in case of verticalLR mode, so we can assume verticalRL for now.
            LayoutUnit overSideMargin = child->isHorizontal() ? box.marginTop() : box.marginRight();
            LayoutUnit underSideMargin = child->isHorizontal() ? box.marginBottom() : box.marginLeft();
            newLogicalTop += overSideMargin;
            boxHeightIncludingMargins += overSideMargin + underSideMargin;
        }

        child->setLogicalTop(newLogicalTop);

        if (childAffectsTopBottomPos) {
            if (is<RenderRubyRun>(child->renderer())) {
                // Treat the leading on the first and last lines of ruby runs as not being part of the overall lineTop/lineBottom.
                // Really this is a workaround hack for the fact that ruby should have been done as line layout and not done using
                // inline-block.
                if (renderer().style().isFlippedLinesWritingMode() == (child->renderer().style().rubyPosition() == RubyPosition::After))
                    hasAnnotationsBefore = true;
                else
                    hasAnnotationsAfter = true;

                auto& rubyRun = downcast<RenderRubyRun>(child->renderer());
                if (RenderRubyBase* rubyBase = rubyRun.rubyBase()) {
                    LayoutUnit bottomRubyBaseLeading = (child->logicalHeight() - rubyBase->logicalBottom()) + rubyBase->logicalHeight() - (rubyBase->lastRootBox() ? rubyBase->lastRootBox()->lineBottom() : LayoutUnit());
                    LayoutUnit topRubyBaseLeading = rubyBase->logicalTop() + (rubyBase->firstRootBox() ? rubyBase->firstRootBox()->lineTop() : LayoutUnit());
                    newLogicalTop += !renderer().style().isFlippedLinesWritingMode() ? topRubyBaseLeading : bottomRubyBaseLeading;
                    boxHeight -= (topRubyBaseLeading + bottomRubyBaseLeading);
                }
            }
            if (is<InlineTextBox>(*child)) {
                if (std::optional<bool> markExistsAndIsAbove = downcast<InlineTextBox>(*child).emphasisMarkExistsAndIsAbove(childLineStyle)) {
                    if (*markExistsAndIsAbove != childLineStyle.isFlippedLinesWritingMode())
                        hasAnnotationsBefore = true;
                    else
                        hasAnnotationsAfter = true;
                }
            }

            if (!setLineTop) {
                setLineTop = true;
                lineTop = newLogicalTop;
                lineTopIncludingMargins = std::min(lineTop, newLogicalTopIncludingMargins);
            } else {
                lineTop = std::min(lineTop, newLogicalTop);
                lineTopIncludingMargins = std::min(lineTop, std::min(lineTopIncludingMargins, newLogicalTopIncludingMargins));
            }
            lineBottom = std::max(lineBottom, newLogicalTop + boxHeight);
            lineBottomIncludingMargins = std::max(lineBottom, std::max(lineBottomIncludingMargins, newLogicalTopIncludingMargins + boxHeightIncludingMargins));
        }

        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        if (inlineFlowBox)
            inlineFlowBox->placeBoxesInBlockDirection(top, maxHeight, maxAscent, strictMode, lineTop, lineBottom, setLineTop,
                                                      lineTopIncludingMargins, lineBottomIncludingMargins, hasAnnotationsBefore, hasAnnotationsAfter, baselineType);
    }

    if (isRootBox) {
        if (strictMode || hasTextChildren() || (descendantsHaveSameLineHeightAndBaseline() && hasTextDescendants())) {
            if (!setLineTop) {
                setLineTop = true;
                lineTop = logicalTop();
                lineTopIncludingMargins = lineTop;
            } else {
                lineTop = std::min<LayoutUnit>(lineTop, logicalTop());
                lineTopIncludingMargins = std::min(lineTop, lineTopIncludingMargins);
            }
            lineBottom = std::max<LayoutUnit>(lineBottom, logicalBottom());
            lineBottomIncludingMargins = std::max(lineBottom, lineBottomIncludingMargins);
        }
        
        if (renderer().style().isFlippedLinesWritingMode())
            flipLinesInBlockDirection(lineTopIncludingMargins, lineBottomIncludingMargins);
    }
}

void InlineFlowBox::maxLogicalBottomForTextDecorationLine(float& maxLogicalBottom, const RenderElement* decorationRenderer, OptionSet<TextDecoration> textDecoration) const
{
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (!(child->lineStyle().textDecorationsInEffect() & textDecoration))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decorationRenderer|'s hierarchy.
        
        if (decorationRenderer && decorationRenderer->isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(*decorationRenderer), &child->renderer()))
            continue;
        
        if (is<InlineFlowBox>(*child))
            downcast<InlineFlowBox>(*child).maxLogicalBottomForTextDecorationLine(maxLogicalBottom, decorationRenderer, textDecoration);
        else {
            if (child->isInlineTextBox() || child->lineStyle().textDecorationSkip().isEmpty())
                maxLogicalBottom = std::max<float>(maxLogicalBottom, child->logicalBottom());
        }
    }
}

void InlineFlowBox::minLogicalTopForTextDecorationLine(float& minLogicalTop, const RenderElement* decorationRenderer, OptionSet<TextDecoration> textDecoration) const
{
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (!(child->lineStyle().textDecorationsInEffect() & textDecoration))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decorationRenderer|'s hierarchy.
        
        if (decorationRenderer && decorationRenderer->isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(*decorationRenderer), &child->renderer()))
            continue;
        
        if (is<InlineFlowBox>(*child))
            downcast<InlineFlowBox>(*child).minLogicalTopForTextDecorationLine(minLogicalTop, decorationRenderer, textDecoration);
        else {
            if (child->isInlineTextBox() || child->lineStyle().textDecorationSkip().isEmpty())
                minLogicalTop = std::min<float>(minLogicalTop, child->logicalTop());
        }
    }
}

void InlineFlowBox::flipLinesInBlockDirection(LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // Flip the box on the line such that the top is now relative to the lineBottom instead of the lineTop.
    setLogicalTop(lineBottom - (logicalTop() - lineTop) - logicalHeight());
    
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders aren't affected here.
        
        if (is<InlineFlowBox>(*child))
            downcast<InlineFlowBox>(*child).flipLinesInBlockDirection(lineTop, lineBottom);
        else
            child->setLogicalTop(lineBottom - (child->logicalTop() - lineTop) - child->logicalHeight());
    }
}

inline void InlineFlowBox::addBoxShadowVisualOverflow(LayoutRect& logicalVisualOverflow)
{
    // box-shadow on root line boxes is applying to the block and not to the lines.
    if (!parent())
        return;

    const RenderStyle& lineStyle = this->lineStyle();
    if (!lineStyle.boxShadow())
        return;

    LayoutUnit boxShadowLogicalTop;
    LayoutUnit boxShadowLogicalBottom;
    lineStyle.getBoxShadowBlockDirectionExtent(boxShadowLogicalTop, boxShadowLogicalBottom);
    
    // Similar to how glyph overflow works, if our lines are flipped, then it's actually the opposite shadow that applies, since
    // the line is "upside down" in terms of block coordinates.
    LayoutUnit shadowLogicalTop = lineStyle.isFlippedLinesWritingMode() ? -boxShadowLogicalBottom : boxShadowLogicalTop;
    LayoutUnit shadowLogicalBottom = lineStyle.isFlippedLinesWritingMode() ? -boxShadowLogicalTop : boxShadowLogicalBottom;
    
    LayoutUnit logicalTopVisualOverflow = std::min<LayoutUnit>(logicalTop() + shadowLogicalTop, logicalVisualOverflow.y());
    LayoutUnit logicalBottomVisualOverflow = std::max<LayoutUnit>(logicalBottom() + shadowLogicalBottom, logicalVisualOverflow.maxY());
    
    LayoutUnit boxShadowLogicalLeft;
    LayoutUnit boxShadowLogicalRight;
    lineStyle.getBoxShadowInlineDirectionExtent(boxShadowLogicalLeft, boxShadowLogicalRight);

    LayoutUnit logicalLeftVisualOverflow = std::min<LayoutUnit>(logicalLeft() + boxShadowLogicalLeft, logicalVisualOverflow.x());
    LayoutUnit logicalRightVisualOverflow = std::max<LayoutUnit>(logicalRight() + boxShadowLogicalRight, logicalVisualOverflow.maxX());
    
    logicalVisualOverflow = LayoutRect(logicalLeftVisualOverflow, logicalTopVisualOverflow,
                                       logicalRightVisualOverflow - logicalLeftVisualOverflow, logicalBottomVisualOverflow - logicalTopVisualOverflow);
}

inline void InlineFlowBox::addBorderOutsetVisualOverflow(LayoutRect& logicalVisualOverflow)
{
    // border-image-outset on root line boxes is applying to the block and not to the lines.
    if (!parent())
        return;
    
    const RenderStyle& lineStyle = this->lineStyle();
    if (!lineStyle.hasBorderImageOutsets())
        return;

    LayoutBoxExtent borderOutsets = lineStyle.borderImageOutsets();

    LayoutUnit borderOutsetLogicalTop = borderOutsets.before(lineStyle.writingMode());
    LayoutUnit borderOutsetLogicalBottom = borderOutsets.after(lineStyle.writingMode());
    LayoutUnit borderOutsetLogicalLeft = borderOutsets.start(lineStyle.writingMode());
    LayoutUnit borderOutsetLogicalRight = borderOutsets.end(lineStyle.writingMode());

    // Similar to how glyph overflow works, if our lines are flipped, then it's actually the opposite border that applies, since
    // the line is "upside down" in terms of block coordinates. vertical-rl and horizontal-bt are the flipped line modes.
    LayoutUnit outsetLogicalTop = lineStyle.isFlippedLinesWritingMode() ? borderOutsetLogicalBottom : borderOutsetLogicalTop;
    LayoutUnit outsetLogicalBottom = lineStyle.isFlippedLinesWritingMode() ? borderOutsetLogicalTop : borderOutsetLogicalBottom;

    LayoutUnit logicalTopVisualOverflow = std::min<LayoutUnit>(logicalTop() - outsetLogicalTop, logicalVisualOverflow.y());
    LayoutUnit logicalBottomVisualOverflow = std::max<LayoutUnit>(logicalBottom() + outsetLogicalBottom, logicalVisualOverflow.maxY());

    LayoutUnit outsetLogicalLeft = includeLogicalLeftEdge() ? borderOutsetLogicalLeft : LayoutUnit();
    LayoutUnit outsetLogicalRight = includeLogicalRightEdge() ? borderOutsetLogicalRight : LayoutUnit();

    LayoutUnit logicalLeftVisualOverflow = std::min<LayoutUnit>(logicalLeft() - outsetLogicalLeft, logicalVisualOverflow.x());
    LayoutUnit logicalRightVisualOverflow = std::max<LayoutUnit>(logicalRight() + outsetLogicalRight, logicalVisualOverflow.maxX());
    
    logicalVisualOverflow = LayoutRect(logicalLeftVisualOverflow, logicalTopVisualOverflow,
                                       logicalRightVisualOverflow - logicalLeftVisualOverflow, logicalBottomVisualOverflow - logicalTopVisualOverflow);
}

inline void InlineFlowBox::addTextBoxVisualOverflow(InlineTextBox& textBox, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, LayoutRect& logicalVisualOverflow)
{
    if (textBox.knownToHaveNoOverflow())
        return;

    const RenderStyle& lineStyle = this->lineStyle();
    
    GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(&textBox);
    GlyphOverflow* glyphOverflow = it == textBoxDataMap.end() ? nullptr : &it->value.second;
    bool isFlippedLine = lineStyle.isFlippedLinesWritingMode();

    int topGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->bottom : glyphOverflow->top) : 0;
    int bottomGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->top : glyphOverflow->bottom) : 0;
    int leftGlyphEdge = glyphOverflow ? glyphOverflow->left : 0;
    int rightGlyphEdge = glyphOverflow ? glyphOverflow->right : 0;

    auto viewportSize = textBox.renderer().frame().view() ? textBox.renderer().frame().view()->size() : IntSize();
    int strokeOverflow = std::ceil(lineStyle.computedStrokeWidth(viewportSize) / 2.0f);
    int topGlyphOverflow = -strokeOverflow - topGlyphEdge;
    int bottomGlyphOverflow = strokeOverflow + bottomGlyphEdge;
    int leftGlyphOverflow = -strokeOverflow - leftGlyphEdge;
    int rightGlyphOverflow = strokeOverflow + rightGlyphEdge;

    if (std::optional<bool> markExistsAndIsAbove = textBox.emphasisMarkExistsAndIsAbove(lineStyle)) {
        int emphasisMarkHeight = lineStyle.fontCascade().emphasisMarkHeight(lineStyle.textEmphasisMarkString());
        if (*markExistsAndIsAbove == !lineStyle.isFlippedLinesWritingMode())
            topGlyphOverflow = std::min(topGlyphOverflow, -emphasisMarkHeight);
        else
            bottomGlyphOverflow = std::max(bottomGlyphOverflow, emphasisMarkHeight);
    }

    // If letter-spacing is negative, we should factor that into right layout overflow. (Even in RTL, letter-spacing is
    // applied to the right, so this is not an issue with left overflow.
    rightGlyphOverflow -= std::min(0, (int)lineStyle.fontCascade().letterSpacing());

    LayoutUnit textShadowLogicalTop;
    LayoutUnit textShadowLogicalBottom;
    lineStyle.getTextShadowBlockDirectionExtent(textShadowLogicalTop, textShadowLogicalBottom);
    
    LayoutUnit childOverflowLogicalTop = std::min<LayoutUnit>(textShadowLogicalTop + topGlyphOverflow, topGlyphOverflow);
    LayoutUnit childOverflowLogicalBottom = std::max<LayoutUnit>(textShadowLogicalBottom + bottomGlyphOverflow, bottomGlyphOverflow);
   
    LayoutUnit textShadowLogicalLeft;
    LayoutUnit textShadowLogicalRight;
    lineStyle.getTextShadowInlineDirectionExtent(textShadowLogicalLeft, textShadowLogicalRight);
   
    LayoutUnit childOverflowLogicalLeft = std::min<LayoutUnit>(textShadowLogicalLeft + leftGlyphOverflow, leftGlyphOverflow);
    LayoutUnit childOverflowLogicalRight = std::max<LayoutUnit>(textShadowLogicalRight + rightGlyphOverflow, rightGlyphOverflow);

    LayoutUnit logicalTopVisualOverflow = std::min<LayoutUnit>(textBox.logicalTop() + childOverflowLogicalTop, logicalVisualOverflow.y());
    LayoutUnit logicalBottomVisualOverflow = std::max<LayoutUnit>(textBox.logicalBottom() + childOverflowLogicalBottom, logicalVisualOverflow.maxY());
    LayoutUnit logicalLeftVisualOverflow = std::min<LayoutUnit>(textBox.logicalLeft() + childOverflowLogicalLeft, logicalVisualOverflow.x());
    LayoutUnit logicalRightVisualOverflow = std::max<LayoutUnit>(textBox.logicalRight() + childOverflowLogicalRight, logicalVisualOverflow.maxX());
    
    logicalVisualOverflow = LayoutRect(logicalLeftVisualOverflow, logicalTopVisualOverflow,
                                       logicalRightVisualOverflow - logicalLeftVisualOverflow, logicalBottomVisualOverflow - logicalTopVisualOverflow);

    auto documentMarkerBounds = textBox.calculateUnionOfAllDocumentMarkerBounds();
    documentMarkerBounds.move(textBox.logicalLeft(), textBox.logicalTop());
    logicalVisualOverflow = unionRect(logicalVisualOverflow, LayoutRect(documentMarkerBounds));
                                    
    textBox.setLogicalOverflowRect(logicalVisualOverflow);
}

inline void InlineFlowBox::addOutlineVisualOverflow(LayoutRect& logicalVisualOverflow)
{
    const auto& lineStyle = this->lineStyle();
    if (!lineStyle.hasOutlineInVisualOverflow())
        return;
    LayoutUnit outlineSize = lineStyle.outlineSize();
    LayoutUnit logicalTopVisualOverflow = std::min(LayoutUnit(logicalTop() - outlineSize), logicalVisualOverflow.y());
    LayoutUnit logicalBottomVisualOverflow = std::max(LayoutUnit(logicalBottom() + outlineSize), logicalVisualOverflow.maxY());
    LayoutUnit logicalLeftVisualOverflow = std::min(LayoutUnit(logicalLeft() - outlineSize), logicalVisualOverflow.x());
    LayoutUnit logicalRightVisualOverflow = std::max(LayoutUnit(logicalRight() + outlineSize), logicalVisualOverflow.maxX());
    logicalVisualOverflow = LayoutRect(logicalLeftVisualOverflow, logicalTopVisualOverflow,
        logicalRightVisualOverflow - logicalLeftVisualOverflow, logicalBottomVisualOverflow - logicalTopVisualOverflow);
}

inline void InlineFlowBox::addReplacedChildOverflow(const InlineBox* inlineBox, LayoutRect& logicalLayoutOverflow, LayoutRect& logicalVisualOverflow)
{
    const RenderBox& box = downcast<RenderBox>(inlineBox->renderer());
    
    // Visual overflow only propagates if the box doesn't have a self-painting layer.  This rectangle does not include
    // transforms or relative positioning (since those objects always have self-painting layers), but it does need to be adjusted
    // for writing-mode differences.
    if (!box.hasSelfPaintingLayer()) {
        LayoutRect childLogicalVisualOverflow = box.logicalVisualOverflowRectForPropagation(&renderer().style());
        childLogicalVisualOverflow.move(inlineBox->logicalLeft(), inlineBox->logicalTop());
        logicalVisualOverflow.unite(childLogicalVisualOverflow);
    }

    // Layout overflow internal to the child box only propagates if the child box doesn't have overflow clip set.
    // Otherwise the child border box propagates as layout overflow.  This rectangle must include transforms and relative positioning
    // and be adjusted for writing-mode differences.
    LayoutRect childLogicalLayoutOverflow = box.logicalLayoutOverflowRectForPropagation(&renderer().style());
    childLogicalLayoutOverflow.move(inlineBox->logicalLeft(), inlineBox->logicalTop());
    logicalLayoutOverflow.unite(childLogicalLayoutOverflow);
}

void InlineFlowBox::computeOverflow(LayoutUnit lineTop, LayoutUnit lineBottom, GlyphOverflowAndFallbackFontsMap& textBoxDataMap)
{
    // If we know we have no overflow, we can just bail.
    if (knownToHaveNoOverflow())
        return;

    if (m_overflow)
        m_overflow = nullptr;

    // Visual overflow just includes overflow for stuff we need to repaint ourselves.  Self-painting layers are ignored.
    // Layout overflow is used to determine scrolling extent, so it still includes child layers and also factors in
    // transforms, relative positioning, etc.
    LayoutRect logicalLayoutOverflow(enclosingLayoutRect(logicalFrameRectIncludingLineHeight(lineTop, lineBottom)));
    LayoutRect logicalVisualOverflow(logicalLayoutOverflow);
  
    addBoxShadowVisualOverflow(logicalVisualOverflow);
    addOutlineVisualOverflow(logicalVisualOverflow);
    addBorderOutsetVisualOverflow(logicalVisualOverflow);

    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (is<RenderLineBreak>(child->renderer()))
            continue;
        if (is<RenderText>(child->renderer())) {
            auto& textBox = downcast<InlineTextBox>(*child);
            LayoutRect textBoxOverflow(enclosingLayoutRect(textBox.logicalFrameRect()));
            addTextBoxVisualOverflow(textBox, textBoxDataMap, textBoxOverflow);
            logicalVisualOverflow.unite(textBoxOverflow);
        } else if (is<RenderInline>(child->renderer())) {
            auto& flow = downcast<InlineFlowBox>(*child);
            flow.computeOverflow(lineTop, lineBottom, textBoxDataMap);
            if (!flow.renderer().hasSelfPaintingLayer())
                logicalVisualOverflow.unite(flow.logicalVisualOverflowRect(lineTop, lineBottom));
            LayoutRect childLayoutOverflow = flow.logicalLayoutOverflowRect(lineTop, lineBottom);
            childLayoutOverflow.move(flow.renderer().relativePositionLogicalOffset());
            logicalLayoutOverflow.unite(childLayoutOverflow);
        } else
            addReplacedChildOverflow(child, logicalLayoutOverflow, logicalVisualOverflow);
    }
    
    setOverflowFromLogicalRects(logicalLayoutOverflow, logicalVisualOverflow, lineTop, lineBottom);
}

void InlineFlowBox::setLayoutOverflow(const LayoutRect& rect, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutRect frameBox = enclosingLayoutRect(frameRectIncludingLineHeight(lineTop, lineBottom));
    if (frameBox.contains(rect) || rect.isEmpty())
        return;

    if (!m_overflow)
        m_overflow = adoptRef(new RenderOverflow(frameBox, frameBox));
    
    m_overflow->setLayoutOverflow(rect);
}

void InlineFlowBox::setVisualOverflow(const LayoutRect& rect, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutRect frameBox = enclosingLayoutRect(frameRectIncludingLineHeight(lineTop, lineBottom));
    if (frameBox.contains(rect) || rect.isEmpty())
        return;
        
    if (!m_overflow)
        m_overflow = adoptRef(new RenderOverflow(frameBox, frameBox));
    
    m_overflow->setVisualOverflow(rect);
}

void InlineFlowBox::setOverflowFromLogicalRects(const LayoutRect& logicalLayoutOverflow, const LayoutRect& logicalVisualOverflow, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutRect layoutOverflow(isHorizontal() ? logicalLayoutOverflow : logicalLayoutOverflow.transposedRect());
    setLayoutOverflow(layoutOverflow, lineTop, lineBottom);
    
    LayoutRect visualOverflow(isHorizontal() ? logicalVisualOverflow : logicalVisualOverflow.transposedRect());
    setVisualOverflow(visualOverflow, lineTop, lineBottom);
}

bool InlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    LayoutRect overflowRect(visualOverflowRect(lineTop, lineBottom));
    flipForWritingMode(overflowRect);
    overflowRect.moveBy(accumulatedOffset);
    if (!locationInContainer.intersects(overflowRect))
        return false;

    // Check children first.
    // We need to account for culled inline parents of the hit-tested nodes, so that they may also get included in area-based hit-tests.
    RenderElement* culledParent = nullptr;
    for (InlineBox* child = lastChild(); child; child = child->prevOnLine()) {
        if (is<RenderText>(child->renderer()) || !child->boxModelObject()->hasSelfPaintingLayer()) {
            RenderElement* newParent = nullptr;
            // Culled parents are only relevant for area-based hit-tests, so ignore it in point-based ones.
            if (locationInContainer.isRectBasedTest()) {
                newParent = child->renderer().parent();
                if (newParent == &renderer())
                    newParent = nullptr;
            }
            // Check the culled parent after all its children have been checked, to do this we wait until
            // we are about to test an element with a different parent.
            if (newParent != culledParent) {
                if (!newParent || !newParent->isDescendantOf(culledParent)) {
                    while (culledParent && culledParent != &renderer() && culledParent != newParent) {
                        if (is<RenderInline>(*culledParent) && downcast<RenderInline>(*culledParent).hitTestCulledInline(request, result, locationInContainer, accumulatedOffset))
                            return true;
                        culledParent = culledParent->parent();
                    }
                }
                culledParent = newParent;
            }
            if (child->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom, hitTestAction)) {
                renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                return true;
            }
        }
    }
    // Check any culled ancestor of the final children tested.
    while (culledParent && culledParent != &renderer()) {
        if (is<RenderInline>(*culledParent) && downcast<RenderInline>(*culledParent).hitTestCulledInline(request, result, locationInContainer, accumulatedOffset))
            return true;
        culledParent = culledParent->parent();
    }

    // Now check ourselves. Pixel snap hit testing.
    if (!visibleToHitTesting())
        return false;

    // Do not hittest content beyond the ellipsis box.
    if (isRootInlineBox() && hasEllipsisBox()) {
        const EllipsisBox* ellipsisBox = root().ellipsisBox();
        FloatRect boundsRect(frameRect());

        if (isHorizontal())
            renderer().style().isLeftToRightDirection() ? boundsRect.shiftXEdgeTo(ellipsisBox->right()) : boundsRect.setWidth(ellipsisBox->left() - left());
        else
            boundsRect.shiftYEdgeTo(ellipsisBox->right());

        flipForWritingMode(boundsRect);
        boundsRect.moveBy(accumulatedOffset);
        // We are beyond the ellipsis box.
        if (locationInContainer.intersects(boundsRect))
            return false;
    }

    // Constrain our hit testing to the line top and bottom if necessary.
    bool noQuirksMode = renderer().document().inNoQuirksMode();
    if (!noQuirksMode && !hasTextChildren() && !(descendantsHaveSameLineHeightAndBaseline() && hasTextDescendants())) {
        RootInlineBox& rootBox = root();
        LayoutUnit top = isHorizontal() ? y() : x();
        LayoutUnit logicalHeight = isHorizontal() ? height() : width();
        LayoutUnit bottom = std::min(rootBox.lineBottom(), top + logicalHeight);
        top = std::max(rootBox.lineTop(), top);
        logicalHeight = bottom - top;
    }

    // Move x/y to our coordinates.
    FloatRect rect(frameRect());
    flipForWritingMode(rect);
    rect.moveBy(accumulatedOffset);

    if (locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset))); // Don't add in m_x or m_y here, we want coords in the containing block's space.
        if (result.addNodeToListBasedTestResult(renderer().element(), request, locationInContainer, rect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

void InlineFlowBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection && paintInfo.phase != PaintPhase::Outline && paintInfo.phase != PaintPhase::SelfOutline && paintInfo.phase != PaintPhase::ChildOutlines && paintInfo.phase != PaintPhase::TextClip && paintInfo.phase != PaintPhase::Mask)
        return;

    LayoutRect overflowRect(visualOverflowRect(lineTop, lineBottom));
    flipForWritingMode(overflowRect);
    overflowRect.moveBy(paintOffset);
    
    if (!paintInfo.rect.intersects(snappedIntRect(overflowRect)))
        return;

    if (paintInfo.phase != PaintPhase::ChildOutlines) {
        if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
            // Add ourselves to the paint info struct's list of inlines that need to paint their
            // outlines.
            if (renderer().style().visibility() == Visibility::Visible && renderer().hasOutline() && !isRootInlineBox()) {
                RenderInline& inlineFlow = downcast<RenderInline>(renderer());

                RenderBlock* containingBlock = nullptr;
                bool containingBlockPaintsContinuationOutline = inlineFlow.continuation() || inlineFlow.isContinuation();
                if (containingBlockPaintsContinuationOutline) {           
                    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=54690. We currently don't reconnect inline continuations
                    // after a child removal. As a result, those merged inlines do not get seperated and hence not get enclosed by
                    // anonymous blocks. In this case, it is better to bail out and paint it ourself.
                    RenderBlock* enclosingAnonymousBlock = renderer().containingBlock();
                    if (!enclosingAnonymousBlock->isAnonymousBlock())
                        containingBlockPaintsContinuationOutline = false;
                    else {
                        containingBlock = enclosingAnonymousBlock->containingBlock();
                        for (auto* box = &renderer(); box != containingBlock; box = &box->parent()->enclosingBoxModelObject()) {
                            if (box->hasSelfPaintingLayer()) {
                                containingBlockPaintsContinuationOutline = false;
                                break;
                            }
                        }
                    }
                }

                if (containingBlockPaintsContinuationOutline) {
                    // Add ourselves to the containing block of the entire continuation so that it can
                    // paint us atomically.
                    containingBlock->addContinuationWithOutline(downcast<RenderInline>(renderer().element()->renderer()));
                } else if (!inlineFlow.isContinuation())
                    paintInfo.outlineObjects->add(&inlineFlow);
            }
        } else if (paintInfo.phase == PaintPhase::Mask)
            paintMask(paintInfo, paintOffset);
        else {
            // Paint our background, border and box-shadow.
            paintBoxDecorations(paintInfo, paintOffset);
        }
    }

    if (paintInfo.phase == PaintPhase::Mask)
        return;

    PaintPhase paintPhase = paintInfo.phase == PaintPhase::ChildOutlines ? PaintPhase::Outline : paintInfo.phase;
    PaintInfo childInfo(paintInfo);
    childInfo.phase = paintPhase;
    childInfo.updateSubtreePaintRootForChildren(&renderer());
    
    // Paint our children.
    if (paintPhase != PaintPhase::SelfOutline) {
        for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (curr->renderer().isText() || !curr->boxModelObject()->hasSelfPaintingLayer())
                curr->paint(childInfo, paintOffset, lineTop, lineBottom);
        }
    }
}

void InlineFlowBox::paintFillLayers(const PaintInfo& paintInfo, const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    Vector<const FillLayer*, 8> layers;
    for (auto* layer = &fillLayer; layer; layer = layer->next())
        layers.append(layer);
    layers.reverse();
    for (auto* layer : layers)
        paintFillLayer(paintInfo, color, *layer, rect, op);
}

bool InlineFlowBox::boxShadowCanBeAppliedToBackground(const FillLayer& lastBackgroundLayer) const
{
    // The checks here match how paintFillLayer() decides whether to clip (if it does, the shadow
    // would be clipped out, so it has to be drawn separately).
    StyleImage* image = lastBackgroundLayer.image();
    bool hasFillImage = image && image->canRender(&renderer(), renderer().style().effectiveZoom());
    return (!hasFillImage && !renderer().style().hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent();
}

void InlineFlowBox::paintFillLayer(const PaintInfo& paintInfo, const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    auto* image = fillLayer.image();
    bool hasFillImage = image && image->canRender(&renderer(), renderer().style().effectiveZoom());
    if ((!hasFillImage && !renderer().style().hasBorderRadius()) || (!prevLineBox() && !nextLineBox()) || !parent())
        renderer().paintFillLayerExtended(paintInfo, color, fillLayer, rect, BackgroundBleedNone, this, rect.size(), op);
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    else if (renderer().style().boxDecorationBreak() == BoxDecorationBreak::Clone) {
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        paintInfo.context().clip(LayoutRect(rect.x(), rect.y(), width(), height()));
        renderer().paintFillLayerExtended(paintInfo, color, fillLayer, rect, BackgroundBleedNone, this, rect.size(), op);
    }
#endif
    else {
        // We have a fill image that spans multiple lines.
        // We need to adjust tx and ty by the width of all previous lines.
        // Think of background painting on inlines as though you had one long line, a single continuous
        // strip.  Even though that strip has been broken up across multiple lines, you still paint it
        // as though you had one single line.  This means each line has to pick up the background where
        // the previous line left off.
        LayoutUnit logicalOffsetOnLine;
        LayoutUnit totalLogicalWidth;
        if (renderer().style().direction() == TextDirection::LTR) {
            for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
                logicalOffsetOnLine += curr->logicalWidth();
            totalLogicalWidth = logicalOffsetOnLine;
            for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
                totalLogicalWidth += curr->logicalWidth();
        } else {
            for (InlineFlowBox* curr = nextLineBox(); curr; curr = curr->nextLineBox())
                logicalOffsetOnLine += curr->logicalWidth();
            totalLogicalWidth = logicalOffsetOnLine;
            for (InlineFlowBox* curr = this; curr; curr = curr->prevLineBox())
                totalLogicalWidth += curr->logicalWidth();
        }
        LayoutUnit stripX = rect.x() - (isHorizontal() ? logicalOffsetOnLine : LayoutUnit());
        LayoutUnit stripY = rect.y() - (isHorizontal() ? LayoutUnit() : logicalOffsetOnLine);
        LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : LayoutUnit(width());
        LayoutUnit stripHeight = isHorizontal() ? LayoutUnit(height()) : totalLogicalWidth;

        GraphicsContextStateSaver stateSaver(paintInfo.context());
        paintInfo.context().clip(LayoutRect(rect.x(), rect.y(), width(), height()));
        renderer().paintFillLayerExtended(paintInfo, color, fillLayer, LayoutRect(stripX, stripY, stripWidth, stripHeight), BackgroundBleedNone, this, rect.size(), op);
    }
}

void InlineFlowBox::paintBoxShadow(const PaintInfo& info, const RenderStyle& style, ShadowStyle shadowStyle, const LayoutRect& paintRect)
{
    if ((!prevLineBox() && !nextLineBox()) || !parent())
        renderer().paintBoxShadow(info, paintRect, style, shadowStyle);
    else {
        // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
        // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
        renderer().paintBoxShadow(info, paintRect, style, shadowStyle, includeLogicalLeftEdge(), includeLogicalRightEdge());
    }
}

void InlineFlowBox::constrainToLineTopAndBottomIfNeeded(LayoutRect& rect) const
{
    bool noQuirksMode = renderer().document().inNoQuirksMode();
    if (!noQuirksMode && !hasTextChildren() && !(descendantsHaveSameLineHeightAndBaseline() && hasTextDescendants())) {
        const RootInlineBox& rootBox = root();
        LayoutUnit logicalTop = isHorizontal() ? rect.y() : rect.x();
        LayoutUnit logicalHeight = isHorizontal() ? rect.height() : rect.width();
        LayoutUnit bottom = std::min(rootBox.lineBottom(), logicalTop + logicalHeight);
        logicalTop = std::max(rootBox.lineTop(), logicalTop);
        logicalHeight = bottom - logicalTop;
        if (isHorizontal()) {
            rect.setY(logicalTop);
            rect.setHeight(logicalHeight);
        } else {
            rect.setX(logicalTop);
            rect.setWidth(logicalHeight);
        }
    }
}

static LayoutRect clipRectForNinePieceImageStrip(InlineFlowBox* box, const NinePieceImage& image, const LayoutRect& paintRect)
{
    LayoutRect clipRect(paintRect);
    auto& style = box->renderer().style();
    LayoutBoxExtent outsets = style.imageOutsets(image);
    if (box->isHorizontal()) {
        clipRect.setY(paintRect.y() - outsets.top());
        clipRect.setHeight(paintRect.height() + outsets.top() + outsets.bottom());
        if (box->includeLogicalLeftEdge()) {
            clipRect.setX(paintRect.x() - outsets.left());
            clipRect.setWidth(paintRect.width() + outsets.left());
        }
        if (box->includeLogicalRightEdge())
            clipRect.setWidth(clipRect.width() + outsets.right());
    } else {
        clipRect.setX(paintRect.x() - outsets.left());
        clipRect.setWidth(paintRect.width() + outsets.left() + outsets.right());
        if (box->includeLogicalLeftEdge()) {
            clipRect.setY(paintRect.y() - outsets.top());
            clipRect.setHeight(paintRect.height() + outsets.top());
        }
        if (box->includeLogicalRightEdge())
            clipRect.setHeight(clipRect.height() + outsets.bottom());
    }
    return clipRect;
}

void InlineFlowBox::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Foreground)
        return;

    LayoutRect frameRect(this->frameRect());
    constrainToLineTopAndBottomIfNeeded(frameRect);
    
    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    flipForWritingMode(localRect);

    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    if (parent() && !renderer().hasVisibleBoxDecorations())
        return;
    const RenderStyle& lineStyle = this->lineStyle();
    if (!parent() && (!isFirstLine() || &lineStyle == &renderer().style()))
        return;

    LayoutPoint adjustedPaintoffset = paintOffset + localRect.location();
    GraphicsContext& context = paintInfo.context();
    LayoutRect paintRect = LayoutRect(adjustedPaintoffset, frameRect.size());
    // Shadow comes first and is behind the background and border.
    if (!renderer().boxShadowShouldBeAppliedToBackground(adjustedPaintoffset, BackgroundBleedNone, this))
        paintBoxShadow(paintInfo, lineStyle, Normal, paintRect);

    Color color = lineStyle.visitedDependentColor(CSSPropertyBackgroundColor);

    CompositeOperator compositeOp = CompositeSourceOver;
    if (renderer().document().settings().punchOutWhiteBackgroundsInDarkMode() && Color::isWhiteColor(color) && renderer().useDarkAppearance())
        compositeOp = CompositeDestinationOut;

    color = lineStyle.colorByApplyingColorFilter(color);

    paintFillLayers(paintInfo, color, lineStyle.backgroundLayers(), paintRect, compositeOp);
    paintBoxShadow(paintInfo, lineStyle, Inset, paintRect);

    // :first-line cannot be used to put borders on a line. Always paint borders with our
    // non-first-line style.
    if (!parent() || !renderer().style().hasVisibleBorderDecoration())
        return;
    const NinePieceImage& borderImage = renderer().style().borderImage();
    StyleImage* borderImageSource = borderImage.image();
    bool hasBorderImage = borderImageSource && borderImageSource->canRender(&renderer(), lineStyle.effectiveZoom());
    if (hasBorderImage && !borderImageSource->isLoaded())
        return; // Don't paint anything while we wait for the image to load.

    // The simple case is where we either have no border image or we are the only box for this object. In those
    // cases only a single call to draw is required.
    if (!hasBorderImage || (!prevLineBox() && !nextLineBox()))
        renderer().paintBorder(paintInfo, paintRect, lineStyle, BackgroundBleedNone, includeLogicalLeftEdge(), includeLogicalRightEdge());
    else {
        // We have a border image that spans multiple lines.
        // We need to adjust tx and ty by the width of all previous lines.
        // Think of border image painting on inlines as though you had one long line, a single continuous
        // strip. Even though that strip has been broken up across multiple lines, you still paint it
        // as though you had one single line. This means each line has to pick up the image where
        // the previous line left off.
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        LayoutUnit logicalOffsetOnLine;
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
        LayoutUnit stripX = adjustedPaintoffset.x() - (isHorizontal() ? logicalOffsetOnLine : LayoutUnit());
        LayoutUnit stripY = adjustedPaintoffset.y() - (isHorizontal() ? LayoutUnit() : logicalOffsetOnLine);
        LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : frameRect.width();
        LayoutUnit stripHeight = isHorizontal() ? frameRect.height() : totalLogicalWidth;

        LayoutRect clipRect = clipRectForNinePieceImageStrip(this, borderImage, paintRect);
        GraphicsContextStateSaver stateSaver(context);
        context.clip(clipRect);
        renderer().paintBorder(paintInfo, LayoutRect(stripX, stripY, stripWidth, stripHeight), lineStyle);
    }
}

void InlineFlowBox::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Mask)
        return;

    LayoutRect frameRect(this->frameRect());
    constrainToLineTopAndBottomIfNeeded(frameRect);
    
    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    flipForWritingMode(localRect);
    LayoutPoint adjustedPaintOffset = paintOffset + localRect.location();

    const NinePieceImage& maskNinePieceImage = renderer().style().maskBoxImage();
    StyleImage* maskBoxImage = renderer().style().maskBoxImage().image();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = renderer().hasLayer() && renderer().layer()->hasCompositedMask();
    bool flattenCompositingLayers = renderer().view().frameView().paintBehavior().contains(PaintBehavior::FlattenCompositingLayers);
    CompositeOperator compositeOp = CompositeSourceOver;
    if (!compositedMask || flattenCompositingLayers) {
        if ((maskBoxImage && renderer().style().maskLayers().hasImage()) || renderer().style().maskLayers().next())
            pushTransparencyLayer = true;
        
        compositeOp = CompositeDestinationIn;
        if (pushTransparencyLayer) {
            paintInfo.context().setCompositeOperation(CompositeDestinationIn);
            paintInfo.context().beginTransparencyLayer(1.0f);
            compositeOp = CompositeSourceOver;
        }
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, frameRect.size());
    paintFillLayers(paintInfo, Color(), renderer().style().maskLayers(), paintRect, compositeOp);
    
    bool hasBoxImage = maskBoxImage && maskBoxImage->canRender(&renderer(), renderer().style().effectiveZoom());
    if (!hasBoxImage || !maskBoxImage->isLoaded()) {
        if (pushTransparencyLayer)
            paintInfo.context().endTransparencyLayer();
        return; // Don't paint anything while we wait for the image to load.
    }

    // The simple case is where we are the only box for this object.  In those
    // cases only a single call to draw is required.
    if (!prevLineBox() && !nextLineBox()) {
        renderer().paintNinePieceImage(paintInfo.context(), LayoutRect(adjustedPaintOffset, frameRect.size()), renderer().style(), maskNinePieceImage, compositeOp);
    } else {
        // We have a mask image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        LayoutUnit logicalOffsetOnLine;
        for (InlineFlowBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
        for (InlineFlowBox* curr = this; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
        LayoutUnit stripX = adjustedPaintOffset.x() - (isHorizontal() ? logicalOffsetOnLine : LayoutUnit());
        LayoutUnit stripY = adjustedPaintOffset.y() - (isHorizontal() ? LayoutUnit() : logicalOffsetOnLine);
        LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : frameRect.width();
        LayoutUnit stripHeight = isHorizontal() ? frameRect.height() : totalLogicalWidth;

        LayoutRect clipRect = clipRectForNinePieceImageStrip(this, maskNinePieceImage, paintRect);
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        paintInfo.context().clip(clipRect);
        renderer().paintNinePieceImage(paintInfo.context(), LayoutRect(stripX, stripY, stripWidth, stripHeight), renderer().style(), maskNinePieceImage, compositeOp);
    }
    
    if (pushTransparencyLayer)
        paintInfo.context().endTransparencyLayer();
}

InlineBox* InlineFlowBox::firstLeafChild() const
{
    InlineBox* leaf = nullptr;
    for (InlineBox* child = firstChild(); child && !leaf; child = child->nextOnLine())
        leaf = child->isLeaf() ? child : downcast<InlineFlowBox>(*child).firstLeafChild();
    return leaf;
}

InlineBox* InlineFlowBox::lastLeafChild() const
{
    InlineBox* leaf = nullptr;
    for (InlineBox* child = lastChild(); child && !leaf; child = child->prevOnLine())
        leaf = child->isLeaf() ? child : downcast<InlineFlowBox>(*child).lastLeafChild();
    return leaf;
}

RenderObject::SelectionState InlineFlowBox::selectionState()
{
    return RenderObject::SelectionNone;
}

bool InlineFlowBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth) const
{
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine()) {
        if (!box->canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth))
            return false;
    }
    return true;
}

float InlineFlowBox::placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    float result = -1;
    // We iterate over all children, the foundBox variable tells us when we've found the
    // box containing the ellipsis.  All boxes after that one in the flow are hidden.
    // If our flow is ltr then iterate over the boxes from left to right, otherwise iterate
    // from right to left. Varying the order allows us to correctly hide the boxes following the ellipsis.
    InlineBox* box = ltr ? firstChild() : lastChild();

    // NOTE: these will cross after foundBox = true.
    int visibleLeftEdge = blockLeftEdge;
    int visibleRightEdge = blockRightEdge;

    while (box) {
        int currResult = box->placeEllipsisBox(ltr, visibleLeftEdge, visibleRightEdge, ellipsisWidth, truncatedWidth, foundBox);
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

LayoutUnit InlineFlowBox::computeOverAnnotationAdjustment(LayoutUnit allowedPosition) const
{
    LayoutUnit result;
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (is<InlineFlowBox>(*child))
            result = std::max(result, downcast<InlineFlowBox>(*child).computeOverAnnotationAdjustment(allowedPosition));
        
        if (child->renderer().isReplaced() && is<RenderRubyRun>(child->renderer()) && child->renderer().style().rubyPosition() == RubyPosition::Before) {
            auto& rubyRun = downcast<RenderRubyRun>(child->renderer());
            RenderRubyText* rubyText = rubyRun.rubyText();
            if (!rubyText)
                continue;
            
            if (!rubyRun.style().isFlippedLinesWritingMode()) {
                LayoutUnit topOfFirstRubyTextLine = rubyText->logicalTop() + (rubyText->firstRootBox() ? rubyText->firstRootBox()->lineTop() : LayoutUnit());
                if (topOfFirstRubyTextLine >= 0)
                    continue;
                topOfFirstRubyTextLine += child->logicalTop();
                result = std::max(result, allowedPosition - topOfFirstRubyTextLine);
            } else {
                LayoutUnit bottomOfLastRubyTextLine = rubyText->logicalTop() + (rubyText->lastRootBox() ? rubyText->lastRootBox()->lineBottom() : rubyText->logicalHeight());
                if (bottomOfLastRubyTextLine <= child->logicalHeight())
                    continue;
                bottomOfLastRubyTextLine += child->logicalTop();
                result = std::max(result, bottomOfLastRubyTextLine - allowedPosition);
            }
        }

        if (is<InlineTextBox>(*child)) {
            const RenderStyle& childLineStyle = child->lineStyle();
            std::optional<bool> markExistsAndIsAbove = downcast<InlineTextBox>(*child).emphasisMarkExistsAndIsAbove(childLineStyle);
            if (markExistsAndIsAbove && *markExistsAndIsAbove) {
                if (!childLineStyle.isFlippedLinesWritingMode()) {
                    int topOfEmphasisMark = child->logicalTop() - childLineStyle.fontCascade().emphasisMarkHeight(childLineStyle.textEmphasisMarkString());
                    result = std::max(result, allowedPosition - topOfEmphasisMark);
                } else {
                    int bottomOfEmphasisMark = child->logicalBottom() + childLineStyle.fontCascade().emphasisMarkHeight(childLineStyle.textEmphasisMarkString());
                    result = std::max(result, bottomOfEmphasisMark - allowedPosition);
                }
            }
        }
    }
    return result;
}

LayoutUnit InlineFlowBox::computeUnderAnnotationAdjustment(LayoutUnit allowedPosition) const
{
    LayoutUnit result;
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (is<InlineFlowBox>(*child))
            result = std::max(result, downcast<InlineFlowBox>(*child).computeUnderAnnotationAdjustment(allowedPosition));

        if (child->renderer().isReplaced() && is<RenderRubyRun>(child->renderer()) && child->renderer().style().rubyPosition() == RubyPosition::After) {
            auto& rubyRun = downcast<RenderRubyRun>(child->renderer());
            RenderRubyText* rubyText = rubyRun.rubyText();
            if (!rubyText)
                continue;

            if (rubyRun.style().isFlippedLinesWritingMode()) {
                LayoutUnit topOfFirstRubyTextLine = rubyText->logicalTop() + (rubyText->firstRootBox() ? rubyText->firstRootBox()->lineTop() : LayoutUnit());
                if (topOfFirstRubyTextLine >= 0)
                    continue;
                topOfFirstRubyTextLine += child->logicalTop();
                result = std::max(result, allowedPosition - topOfFirstRubyTextLine);
            } else {
                LayoutUnit bottomOfLastRubyTextLine = rubyText->logicalTop() + (rubyText->lastRootBox() ? rubyText->lastRootBox()->lineBottom() : rubyText->logicalHeight());
                if (bottomOfLastRubyTextLine <= child->logicalHeight())
                    continue;
                bottomOfLastRubyTextLine += child->logicalTop();
                result = std::max(result, bottomOfLastRubyTextLine - allowedPosition);
            }
        }

        if (is<InlineTextBox>(*child)) {
            const RenderStyle& childLineStyle = child->lineStyle();
            std::optional<bool> markExistsAndIsAbove = downcast<InlineTextBox>(*child).emphasisMarkExistsAndIsAbove(childLineStyle);
            if (markExistsAndIsAbove && !*markExistsAndIsAbove) {
                if (!childLineStyle.isFlippedLinesWritingMode()) {
                    LayoutUnit bottomOfEmphasisMark = child->logicalBottom() + childLineStyle.fontCascade().emphasisMarkHeight(childLineStyle.textEmphasisMarkString());
                    result = std::max(result, bottomOfEmphasisMark - allowedPosition);
                } else {
                    LayoutUnit topOfEmphasisMark = child->logicalTop() - childLineStyle.fontCascade().emphasisMarkHeight(childLineStyle.textEmphasisMarkString());
                    result = std::max(result, allowedPosition - topOfEmphasisMark);
                }
            }
        }
    }
    return result;
}

void InlineFlowBox::collectLeafBoxesInLogicalOrder(Vector<InlineBox*>& leafBoxesInLogicalOrder, CustomInlineBoxRangeReverse customReverseImplementation, void* userData) const
{
    InlineBox* leaf = firstLeafChild();

    // FIXME: The reordering code is a copy of parts from BidiResolver::createBidiRunsForLine, operating directly on InlineBoxes, instead of BidiRuns.
    // Investigate on how this code could possibly be shared.
    unsigned char minLevel = 128;
    unsigned char maxLevel = 0;

    // First find highest and lowest levels, and initialize leafBoxesInLogicalOrder with the leaf boxes in visual order.
    for (; leaf; leaf = leaf->nextLeafChild()) {
        minLevel = std::min(minLevel, leaf->bidiLevel());
        maxLevel = std::max(maxLevel, leaf->bidiLevel());
        leafBoxesInLogicalOrder.append(leaf);
    }

    if (renderer().style().rtlOrdering() == Order::Visual)
        return;

    // Reverse of reordering of the line (L2 according to Bidi spec):
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // Reversing the reordering of the line is only done up to the lowest odd level.
    if (!(minLevel % 2))
        ++minLevel;

    Vector<InlineBox*>::iterator end = leafBoxesInLogicalOrder.end();
    while (minLevel <= maxLevel) {
        Vector<InlineBox*>::iterator it = leafBoxesInLogicalOrder.begin();
        while (it != end) {
            while (it != end) {
                if ((*it)->bidiLevel() >= minLevel)
                    break;
                ++it;
            }
            Vector<InlineBox*>::iterator first = it;
            while (it != end) {
                if ((*it)->bidiLevel() < minLevel)
                    break;
                ++it;
            }
            Vector<InlineBox*>::iterator last = it;
            if (customReverseImplementation) {
                ASSERT(userData);
                (*customReverseImplementation)(userData, first, last);
            } else
                std::reverse(first, last);
        }                
        ++minLevel;
    }
}

void InlineFlowBox::computeReplacedAndTextLineTopAndBottom(LayoutUnit& lineTop, LayoutUnit& lineBottom) const
{
    for (const auto* box = firstChild(); box; box = box->nextOnLine()) {
        if (is<InlineFlowBox>(*box))
            downcast<InlineFlowBox>(*box).computeReplacedAndTextLineTopAndBottom(lineTop, lineBottom);
        else {
            if (box->logicalTop() < lineTop)
                lineTop = box->logicalTop();
            if (box->logicalBottom() > lineBottom)
                lineBottom = box->logicalBottom();
        }
    }
}

#if ENABLE(TREE_DEBUGGING)

const char* InlineFlowBox::boxName() const
{
    return "InlineFlowBox";
}

void InlineFlowBox::outputLineTreeAndMark(WTF::TextStream& stream, const InlineBox* markedBox, int depth) const
{
    InlineBox::outputLineTreeAndMark(stream, markedBox, depth);
    for (const InlineBox* box = firstChild(); box; box = box->nextOnLine())
        box->outputLineTreeAndMark(stream, markedBox, depth + 1);
}

#endif

#ifndef NDEBUG

void InlineFlowBox::checkConsistency() const
{
    assertNotDeleted();
    ASSERT_WITH_SECURITY_IMPLICATION(!m_hasBadChildList);
#ifdef CHECK_CONSISTENCY
    const InlineBox* previousChild = nullptr;
    for (const InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        ASSERT(child->parent() == this);
        ASSERT(child->prevOnLine() == previousChild);
        previousChild = child;
    }
    ASSERT(previousChild == m_lastChild);
#endif
}

#endif

} // namespace WebCore
