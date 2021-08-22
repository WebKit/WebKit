/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include "LegacyRootInlineBox.h"

#include "BidiResolver.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyEllipsisBox.h"
#include "LegacyInlineTextBox.h"
#include "LogicalSelectionOffsetCaches.h"
#include "PaintInfo.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayoutState.h"
#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderView.h"
#include "VerticalPositionCache.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRootInlineBox);

struct SameSizeAsLegacyRootInlineBox : public LegacyInlineFlowBox, public CanMakeWeakPtr<LegacyRootInlineBox> {
    unsigned variables[7];
    WeakPtr<RenderObject> lineBreakObj;
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(LegacyRootInlineBox) == sizeof(SameSizeAsLegacyRootInlineBox), LegacyRootInlineBox_should_stay_small);
#if !ASSERT_ENABLED
COMPILE_ASSERT(sizeof(WeakPtr<RenderObject>) == sizeof(void*), WeakPtr_should_be_same_size_as_raw_pointer);
#endif

typedef WTF::HashMap<const LegacyRootInlineBox*, std::unique_ptr<LegacyEllipsisBox>> EllipsisBoxMap;
static EllipsisBoxMap* gEllipsisBoxMap;

static ContainingFragmentMap& containingFragmentMap(RenderBlockFlow& block)
{
    ASSERT(block.enclosingFragmentedFlow());
    return block.enclosingFragmentedFlow()->containingFragmentMap();
}

LegacyRootInlineBox::LegacyRootInlineBox(RenderBlockFlow& block)
    : LegacyInlineFlowBox(block)
{
    setIsHorizontal(block.isHorizontalWritingMode());
}

LegacyRootInlineBox::~LegacyRootInlineBox()
{
    detachEllipsisBox();

    if (blockFlow().enclosingFragmentedFlow())
        containingFragmentMap(blockFlow()).remove(this);
}

void LegacyRootInlineBox::detachEllipsisBox()
{
    if (hasEllipsisBox()) {
        auto box = gEllipsisBoxMap->take(this);
        box->setParent(nullptr);
        setHasEllipsisBox(false);
    }
}

void LegacyRootInlineBox::clearTruncation()
{
    if (hasEllipsisBox()) {
        detachEllipsisBox();
        LegacyInlineFlowBox::clearTruncation();
    }
}

bool LegacyRootInlineBox::isHyphenated() const
{
    for (auto* box = firstLeafDescendant(); box; box = box->nextLeafOnLine()) {
        if (is<LegacyInlineTextBox>(*box) && downcast<LegacyInlineTextBox>(*box).hasHyphen())
            return true;
    }
    return false;
}

LayoutUnit LegacyRootInlineBox::baselinePosition(FontBaseline baselineType) const
{
    return renderer().baselinePosition(baselineType, isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

LayoutUnit LegacyRootInlineBox::lineHeight() const
{
    return renderer().lineHeight(isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

bool LegacyRootInlineBox::lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth)
{
    // First sanity-check the unoverflowed width of the whole line to see if there is sufficient room.
    int delta = ltr ? lineBoxEdge - blockEdge : blockEdge - lineBoxEdge;
    if (logicalWidth() - delta < ellipsisWidth)
        return false;

    // Next iterate over all the line boxes on the line. If we find a replaced element that intersects
    // then we refuse to accommodate the ellipsis. Otherwise we're ok.
    return LegacyInlineFlowBox::canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth);
}

float LegacyRootInlineBox::placeEllipsis(const AtomString& ellipsisStr,  bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, LegacyInlineBox* markupBox)
{
    if (!gEllipsisBoxMap)
        gEllipsisBoxMap = new EllipsisBoxMap();

    ASSERT(!hasEllipsisBox());
    auto* ellipsisBox = gEllipsisBoxMap->set(this, makeUnique<LegacyEllipsisBox>(blockFlow(), ellipsisStr, this, ellipsisWidth - (markupBox ? markupBox->logicalWidth() : 0), logicalHeight(), y(), !prevRootBox(), isHorizontal(), markupBox)).iterator->value.get();
    setHasEllipsisBox(true);
    // FIXME: Do we need an RTL version of this?
    if (ltr && (x() + logicalWidth() + ellipsisWidth) <= blockRightEdge) {
        ellipsisBox->setX(x() + logicalWidth());
        return logicalWidth() + ellipsisWidth;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph. Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    float truncatedWidth = 0;
    float position = placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, truncatedWidth, foundBox);
    ellipsisBox->setX(position);
    return truncatedWidth;
}

float LegacyRootInlineBox::placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    float result = LegacyInlineFlowBox::placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, truncatedWidth, foundBox);
    if (result == -1) {
        result = ltr ? blockRightEdge - ellipsisWidth : blockLeftEdge;
        truncatedWidth = blockRightEdge - blockLeftEdge;
    }
    return result;
}

void LegacyRootInlineBox::paintEllipsisBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom) const
{
    if (hasEllipsisBox() && paintInfo.shouldPaintWithinRoot(renderer()) && renderer().style().visibility() == Visibility::Visible && paintInfo.phase == PaintPhase::Foreground)
        ellipsisBox()->paint(paintInfo, paintOffset, lineTop, lineBottom);
}

void LegacyRootInlineBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LegacyInlineFlowBox::paint(paintInfo, paintOffset, lineTop, lineBottom);
    paintEllipsisBox(paintInfo, paintOffset, lineTop, lineBottom);
}

bool LegacyRootInlineBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction hitTestAction)
{
    if (hasEllipsisBox() && visibleToHitTesting(request)) {
        if (ellipsisBox()->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom, hitTestAction)) {
            renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
            return true;
        }
    }
    return LegacyInlineFlowBox::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom, hitTestAction);
}

void LegacyRootInlineBox::adjustPosition(float dx, float dy)
{
    LegacyInlineFlowBox::adjustPosition(dx, dy);
    LayoutUnit blockDirectionDelta { isHorizontal() ? dy : dx }; // The block direction delta is a LayoutUnit.
    m_lineTop += blockDirectionDelta;
    m_lineBottom += blockDirectionDelta;
    m_lineBoxTop += blockDirectionDelta;
    m_lineBoxBottom += blockDirectionDelta;
    if (hasEllipsisBox())
        ellipsisBox()->adjustPosition(dx, dy);
}

void LegacyRootInlineBox::childRemoved(LegacyInlineBox* box)
{
    if (&box->renderer() == m_lineBreakObj)
        setLineBreakInfo(nullptr, 0, BidiStatus());

    for (auto* prev = prevRootBox(); prev && prev->lineBreakObj() == &box->renderer(); prev = prev->prevRootBox()) {
        prev->setLineBreakInfo(nullptr, 0, BidiStatus());
        prev->markDirty();
    }
}

RenderFragmentContainer* LegacyRootInlineBox::containingFragment() const
{
    ContainingFragmentMap& fragmentMap = containingFragmentMap(blockFlow());
    bool hasContainingFragment = fragmentMap.contains(this);
    RenderFragmentContainer* fragment = hasContainingFragment ? fragmentMap.get(this) : nullptr;

#ifndef NDEBUG
    if (hasContainingFragment) {
        RenderFragmentedFlow* fragmentedFlow = blockFlow().enclosingFragmentedFlow();
        const RenderFragmentContainerList& fragmentList = fragmentedFlow->renderFragmentContainerList();
        ASSERT_WITH_SECURITY_IMPLICATION(fragmentList.contains(fragment));
    }
#endif

    return fragment;
}

void LegacyRootInlineBox::clearContainingFragment()
{
    ASSERT(!isDirty());

    if (!containingFragmentMap(blockFlow()).contains(this))
        return;

    containingFragmentMap(blockFlow()).remove(this);
}

void LegacyRootInlineBox::setContainingFragment(RenderFragmentContainer& fragment)
{
    ASSERT(!isDirty());

    containingFragmentMap(blockFlow()).set(this, &fragment);
}

LayoutUnit LegacyRootInlineBox::alignBoxesInBlockDirection(LayoutUnit heightOfBlock, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache)
{
    // SVG will handle vertical alignment on its own.
    if (isSVGRootInlineBox())
        return 0;

    LayoutUnit maxPositionTop;
    LayoutUnit maxPositionBottom;
    LayoutUnit maxAscent;
    LayoutUnit maxDescent;
    bool setMaxAscent = false;
    bool setMaxDescent = false;

    // Figure out if we're in no-quirks mode.
    bool noQuirksMode = renderer().document().inNoQuirksMode();

    m_baselineType = requiresIdeographicBaseline(textBoxDataMap) ? IdeographicBaseline : AlphabeticBaseline;

    computeLogicalBoxHeights(*this, maxPositionTop, maxPositionBottom, maxAscent, maxDescent, setMaxAscent, setMaxDescent, noQuirksMode,
        textBoxDataMap, baselineType(), verticalPositionCache);

    if (maxAscent + maxDescent < std::max(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    LayoutUnit maxHeight = maxAscent + maxDescent;
    LayoutUnit lineTop = heightOfBlock;
    LayoutUnit lineBottom = heightOfBlock;
    LayoutUnit lineTopIncludingMargins = heightOfBlock;
    LayoutUnit lineBottomIncludingMargins = heightOfBlock;
    bool setLineTop = false;
    bool hasAnnotationsBefore = false;
    bool hasAnnotationsAfter = false;
    placeBoxesInBlockDirection(heightOfBlock, maxHeight, maxAscent, noQuirksMode, lineTop, lineBottom, setLineTop,
        lineTopIncludingMargins, lineBottomIncludingMargins, hasAnnotationsBefore, hasAnnotationsAfter, baselineType());
    m_hasAnnotationsBefore = hasAnnotationsBefore;
    m_hasAnnotationsAfter = hasAnnotationsAfter;
    
    maxHeight = std::max<LayoutUnit>(0, maxHeight); // FIXME: Is this really necessary?

    LayoutUnit lineBoxTop = heightOfBlock;
    LayoutUnit lineBoxBottom = heightOfBlock + maxHeight;
    setLineTopBottomPositions(lineTop, lineBottom, lineBoxTop, lineBoxBottom);
    setPaginatedLineWidth(blockFlow().availableLogicalWidthForContent(heightOfBlock));

    LayoutUnit annotationsAdjustment = beforeAnnotationsAdjustment();
    if (annotationsAdjustment) {
        // FIXME: Need to handle pagination here. We might have to move to the next page/column as a result of the
        // ruby expansion.
        adjustBlockDirectionPosition(annotationsAdjustment);
        heightOfBlock += annotationsAdjustment;
    }

    LayoutUnit gridSnapAdjustment = lineSnapAdjustment();
    if (gridSnapAdjustment) {
        adjustBlockDirectionPosition(gridSnapAdjustment);
        heightOfBlock += gridSnapAdjustment;
    }

    return heightOfBlock + maxHeight;
}

LayoutUnit LegacyRootInlineBox::beforeAnnotationsAdjustment() const
{
    LayoutUnit result;

    if (!renderer().style().isFlippedLinesWritingMode()) {
        // Annotations under the previous line may push us down.
        if (prevRootBox() && prevRootBox()->hasAnnotationsAfter())
            result = prevRootBox()->computeUnderAnnotationAdjustment(lineTop());

        if (!hasAnnotationsBefore())
            return result;

        // Annotations over this line may push us further down.
        LayoutUnit highestAllowedPosition = prevRootBox() ? std::min(prevRootBox()->lineBottom(), lineTop()) + result : blockFlow().borderBefore();
        result = computeOverAnnotationAdjustment(highestAllowedPosition);
    } else {
        // Annotations under this line may push us up.
        if (hasAnnotationsBefore())
            result = computeUnderAnnotationAdjustment(prevRootBox() ? prevRootBox()->lineBottom() : blockFlow().borderBefore());

        if (!prevRootBox() || !prevRootBox()->hasAnnotationsAfter())
            return result;

        // We have to compute the expansion for annotations over the previous line to see how much we should move.
        LayoutUnit lowestAllowedPosition = std::max(prevRootBox()->lineBottom(), lineTop()) - result;
        result = prevRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }

    return result;
}

LayoutUnit LegacyRootInlineBox::lineSnapAdjustment(LayoutUnit delta) const
{
    // If our block doesn't have snapping turned on, do nothing.
    // FIXME: Implement bounds snapping.
    if (blockFlow().style().lineSnap() == LineSnap::None)
        return 0;

    // Get the current line grid and offset.
    auto* layoutState = blockFlow().view().frameView().layoutContext().layoutState();
    RenderBlockFlow* lineGrid = layoutState->lineGrid();
    LayoutSize lineGridOffset = layoutState->lineGridOffset();
    if (!lineGrid || lineGrid->style().writingMode() != blockFlow().style().writingMode())
        return 0;

    // Get the hypothetical line box used to establish the grid.
    LegacyRootInlineBox* lineGridBox = lineGrid->lineGridBox();
    if (!lineGridBox)
        return 0;
    
    LayoutUnit lineGridBlockOffset = lineGrid->isHorizontalWritingMode() ? lineGridOffset.height() : lineGridOffset.width();
    LayoutUnit blockOffset = blockFlow().isHorizontalWritingMode() ? layoutState->layoutOffset().height() : layoutState->layoutOffset().width();

    // Now determine our position on the grid. Our baseline needs to be adjusted to the nearest baseline multiple
    // as established by the line box.
    // FIXME: Need to handle crazy line-box-contain values that cause the root line box to not be considered. I assume
    // the grid should honor line-box-contain.
    LayoutUnit gridLineHeight = lineGridBox->lineBoxBottom() - lineGridBox->lineBoxTop();
    if (!gridLineHeight)
        return 0;

    LayoutUnit lineGridFontAscent = lineGrid->style().fontMetrics().ascent(baselineType());
    LayoutUnit lineGridFontHeight { lineGridBox->logicalHeight() };
    LayoutUnit firstTextTop { lineGridBlockOffset + lineGridBox->logicalTop() };
    LayoutUnit firstLineTopWithLeading = lineGridBlockOffset + lineGridBox->lineBoxTop();
    LayoutUnit firstBaselinePosition = firstTextTop + lineGridFontAscent;

    LayoutUnit currentTextTop { blockOffset + logicalTop() + delta };
    LayoutUnit currentFontAscent = blockFlow().style().fontMetrics().ascent(baselineType());
    LayoutUnit currentBaselinePosition = currentTextTop + currentFontAscent;

    LayoutUnit lineGridPaginationOrigin = isHorizontal() ? layoutState->lineGridPaginationOrigin().height() : layoutState->lineGridPaginationOrigin().width();

    // If we're paginated, see if we're on a page after the first one. If so, the grid resets on subsequent pages.
    // FIXME: If the grid is an ancestor of the pagination establisher, then this is incorrect.
    LayoutUnit pageLogicalTop;
    if (layoutState->isPaginated() && layoutState->pageLogicalHeight()) {
        pageLogicalTop = blockFlow().pageLogicalTopForOffset(lineBoxTop() + delta);
        if (pageLogicalTop > firstLineTopWithLeading)
            firstTextTop = pageLogicalTop + lineGridBox->logicalTop() - lineGrid->borderAndPaddingBefore() + lineGridPaginationOrigin;
    }

    if (blockFlow().style().lineSnap() == LineSnap::Contain) {
        // Compute the desired offset from the text-top of a grid line.
        // Look at our height (logicalHeight()).
        // Look at the total available height. It's going to be (textBottom - textTop) + (n-1)*(multiple with leading)
        // where n is number of grid lines required to enclose us.
        if (logicalHeight() <= lineGridFontHeight)
            firstTextTop += (lineGridFontHeight - logicalHeight()) / 2;
        else {
            LayoutUnit numberOfLinesWithLeading { ceilf(static_cast<float>(logicalHeight() - lineGridFontHeight) / gridLineHeight) };
            LayoutUnit totalHeight = lineGridFontHeight + numberOfLinesWithLeading * gridLineHeight;
            firstTextTop += (totalHeight - logicalHeight()) / 2;
        }
        firstBaselinePosition = firstTextTop + currentFontAscent;
    } else
        firstBaselinePosition = firstTextTop + lineGridFontAscent;

    // If we're above the first line, just push to the first line.
    if (currentBaselinePosition < firstBaselinePosition)
        return delta + firstBaselinePosition - currentBaselinePosition;

    // Otherwise we're in the middle of the grid somewhere. Just push to the next line.
    LayoutUnit baselineOffset = currentBaselinePosition - firstBaselinePosition;
    LayoutUnit remainder = roundToInt(baselineOffset) % roundToInt(gridLineHeight);
    LayoutUnit result = delta;
    if (remainder)
        result += gridLineHeight - remainder;

    // If we aren't paginated we can return the result.
    if (!layoutState->isPaginated() || !layoutState->pageLogicalHeight() || result == delta)
        return result;
    
    // We may end up shifted to a new page. We need to do a re-snap when that happens.
    LayoutUnit newPageLogicalTop = blockFlow().pageLogicalTopForOffset(lineBoxBottom() + result);
    if (newPageLogicalTop == pageLogicalTop)
        return result;
    
    auto pageLogicalHeight = blockFlow().pageLogicalHeightForOffset(lineBoxBottom() + result);
    // Put ourselves at the top of the next page to force a snap onto the new grid established by that page unless
    // the line is taller than the page. In such cases let's just leave the line overflow. 
    if (pageLogicalHeight < lineBoxBottom() + result)
        return result;
    return lineSnapAdjustment(newPageLogicalTop - (blockOffset + lineBoxTop()));
}

GapRects LegacyRootInlineBox::lineSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit selTop, LayoutUnit selHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    RenderObject::HighlightState lineState = selectionState();

    bool leftGap, rightGap;
    blockFlow().getSelectionGapInfo(lineState, leftGap, rightGap);

    GapRects result;

    LegacyInlineBox* firstBox = firstSelectedBox();
    LegacyInlineBox* lastBox = lastSelectedBox();
    if (leftGap) {
        result.uniteLeft(blockFlow().logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, &firstBox->parent()->renderer(), LayoutUnit(firstBox->logicalLeft()),
            selTop, selHeight, cache, paintInfo));
    }
    if (rightGap) {
        result.uniteRight(blockFlow().logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, &lastBox->parent()->renderer(), LayoutUnit(lastBox->logicalRight()),
            selTop, selHeight, cache, paintInfo));
    }

    // When dealing with bidi text, a non-contiguous selection region is possible.
    // e.g. The logical text aaaAAAbbb (capitals denote RTL text and non-capitals LTR) is layed out
    // visually as 3 text runs |aaa|bbb|AAA| if we select 4 characters from the start of the text the
    // selection will look like (underline denotes selection):
    // |aaa|bbb|AAA|
    //  ___       _
    // We can see that the |bbb| run is not part of the selection while the runs around it are.
    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        LayoutUnit lastLogicalLeft { firstBox->logicalRight() };
        bool isPreviousBoxSelected = firstBox->selectionState() != RenderObject::HighlightState::None;
        for (auto* box = firstBox->nextLeafOnLine(); box; box = box->nextLeafOnLine()) {
            if (box->selectionState() != RenderObject::HighlightState::None) {
                LayoutRect logicalRect { lastLogicalLeft, selTop, LayoutUnit(box->logicalLeft() - lastLogicalLeft), selHeight };
                logicalRect.move(renderer().isHorizontalWritingMode() ? offsetFromRootBlock : LayoutSize(offsetFromRootBlock.height(), offsetFromRootBlock.width()));
                LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
                if (isPreviousBoxSelected && gapRect.width() > 0 && gapRect.height() > 0) {
                    if (paintInfo && box->parent()->renderer().style().visibility() == Visibility::Visible)
                        paintInfo->context().fillRect(gapRect, box->parent()->renderer().selectionBackgroundColor());
                    // VisibleSelection may be non-contiguous, see comment above.
                    result.uniteCenter(gapRect);
                }
                lastLogicalLeft = box->logicalRight();
            }
            if (box == lastBox)
                break;
            isPreviousBoxSelected = box->selectionState() != RenderObject::HighlightState::None;
        }
    }

    return result;
}

RenderObject::HighlightState LegacyRootInlineBox::selectionState()
{
    // Walk over all of the selected boxes.
    RenderObject::HighlightState state = RenderObject::HighlightState::None;
    for (auto* box = firstLeafDescendant(); box; box = box->nextLeafOnLine()) {
        RenderObject::HighlightState boxState = box->selectionState();
        if ((boxState == RenderObject::HighlightState::Start && state == RenderObject::HighlightState::End)
            || (boxState == RenderObject::HighlightState::End && state == RenderObject::HighlightState::Start))
            state = RenderObject::HighlightState::Both;
        else if (state == RenderObject::HighlightState::None || ((boxState == RenderObject::HighlightState::Start || boxState == RenderObject::HighlightState::End)
            && (state == RenderObject::HighlightState::None || state == RenderObject::HighlightState::Inside)))
            state = boxState;
        else if (boxState == RenderObject::HighlightState::None && state == RenderObject::HighlightState::Start) {
            // We are past the end of the selection.
            state = RenderObject::HighlightState::Both;
        }
        if (state == RenderObject::HighlightState::Both)
            break;
    }

    return state;
}

LegacyInlineBox* LegacyRootInlineBox::firstSelectedBox()
{
    for (auto* box = firstLeafDescendant(); box; box = box->nextLeafOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return nullptr;
}

LegacyInlineBox* LegacyRootInlineBox::lastSelectedBox()
{
    for (auto* box = lastLeafDescendant(); box; box = box->previousLeafOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return nullptr;
}

LayoutUnit LegacyRootInlineBox::selectionTop(ForHitTesting forHitTesting) const
{
    LayoutUnit selectionTop = m_lineTop;
    
    if (m_hasAnnotationsBefore)
        selectionTop -= !renderer().style().isFlippedLinesWritingMode() ? computeOverAnnotationAdjustment(m_lineTop) : computeUnderAnnotationAdjustment(m_lineTop);

    if (renderer().style().isFlippedLinesWritingMode())
        return selectionTop;

#if !PLATFORM(IOS_FAMILY)
    // See rdar://problem/19692206 ... don't want to do this adjustment for iOS where overlap is ok and handled.
    if (renderer().isRubyBase()) {
        // The ruby base selection should avoid intruding into the ruby text. This is only the case if there is an actual ruby text above us.
        RenderRubyBase* base = &downcast<RenderRubyBase>(renderer());
        RenderRubyRun* run = base->rubyRun();
        if (run) {
            RenderRubyText* text = run->rubyText();
            if (text && text->logicalTop() < base->logicalTop()) {
                // The ruby text is above the ruby base. Just return now in order to avoid painting on top of the ruby text.
                return selectionTop;
            }
        }
    } else if (renderer().isRubyText()) {
        // The ruby text selection should go all the way to the selection top of the containing line.
        RenderRubyText* text = &downcast<RenderRubyText>(renderer());
        RenderRubyRun* run = text->rubyRun();
        if (run && run->inlineBoxWrapper()) {
            RenderRubyBase* base = run->rubyBase();
            if (base && text->logicalTop() < base->logicalTop()) {
                // The ruby text is above the ruby base.
                const LegacyRootInlineBox& containingLine = run->inlineBoxWrapper()->root();
                LayoutUnit enclosingSelectionTop = containingLine.selectionTop();
                LayoutUnit deltaBetweenObjects = text->logicalTop() + run->logicalTop();
                LayoutUnit selectionTopInRubyTextCoords = enclosingSelectionTop - deltaBetweenObjects;
                return std::min(selectionTop, selectionTopInRubyTextCoords);
            }
        }
    }
#endif

    LayoutUnit prevBottom;
    if (auto* previousBox = prevRootBox())
        prevBottom = previousBox->selectionBottom();
    else {
        auto borderAndPaddingBefore = blockFlow().borderAndPaddingBefore();
        prevBottom = forHitTesting == ForHitTesting::Yes ? borderAndPaddingBefore : std::max(borderAndPaddingBefore, selectionTop);
    }

    if (prevBottom < selectionTop && blockFlow().containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats. If so, let's check the offsets, and only be willing to use the previous
        // line's bottom if the offsets are greater on both sides.
        LayoutUnit prevLeft = blockFlow().logicalLeftOffsetForLine(prevBottom, DoNotIndentText);
        LayoutUnit prevRight = blockFlow().logicalRightOffsetForLine(prevBottom, DoNotIndentText);
        LayoutUnit newLeft = blockFlow().logicalLeftOffsetForLine(selectionTop, DoNotIndentText);
        LayoutUnit newRight = blockFlow().logicalRightOffsetForLine(selectionTop, DoNotIndentText);
        if (prevLeft > newLeft || prevRight < newRight)
            return selectionTop;
    }

    return prevBottom;
}

static RenderBlock* blockBeforeWithinSelectionRoot(const RenderBlockFlow& blockFlow, LayoutSize& offset)
{
    if (blockFlow.isSelectionRoot())
        return nullptr;

    const RenderElement* object = &blockFlow;
    RenderObject* sibling;
    do {
        sibling = object->previousSibling();
        while (sibling && (!is<RenderBlock>(*sibling) || downcast<RenderBlock>(*sibling).isSelectionRoot()))
            sibling = sibling->previousSibling();

        offset -= LayoutSize(downcast<RenderBlock>(*object).logicalLeft(), downcast<RenderBlock>(*object).logicalTop());
        object = object->parent();
    } while (!sibling && is<RenderBlock>(object) && !downcast<RenderBlock>(*object).isSelectionRoot());

    if (!sibling)
        return nullptr;

    RenderBlock* beforeBlock = downcast<RenderBlock>(sibling);

    offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());

    RenderObject* child = beforeBlock->lastChild();
    while (is<RenderBlock>(child)) {
        beforeBlock = downcast<RenderBlock>(child);
        offset += LayoutSize(beforeBlock->logicalLeft(), beforeBlock->logicalTop());
        child = beforeBlock->lastChild();
    }
    return beforeBlock;
}

LayoutUnit LegacyRootInlineBox::selectionTopAdjustedForPrecedingBlock() const
{
    const LegacyRootInlineBox& rootBox = root();
    LayoutUnit top = selectionTop();

    auto blockSelectionState = rootBox.blockFlow().selectionState();
    if (blockSelectionState != RenderObject::HighlightState::Inside && blockSelectionState != RenderObject::HighlightState::End)
        return top;

    LayoutSize offsetToBlockBefore;
    auto* blockBefore = blockBeforeWithinSelectionRoot(rootBox.blockFlow(), offsetToBlockBefore);
    if (!is<RenderBlockFlow>(blockBefore))
        return top;

    // Do not adjust blocks sharing the same line.
    if (!offsetToBlockBefore.height())
        return top;

    if (auto* lastLine = downcast<RenderBlockFlow>(*blockBefore).lastRootBox()) {
        RenderObject::HighlightState lastLineSelectionState = lastLine->selectionState();
        if (lastLineSelectionState != RenderObject::HighlightState::Inside && lastLineSelectionState != RenderObject::HighlightState::Start)
            return top;

        LayoutUnit lastLineSelectionBottom = lastLine->selectionBottom() + offsetToBlockBefore.height();
        top = std::max(top, lastLineSelectionBottom);
    }
    return top;
}

LayoutUnit LegacyRootInlineBox::selectionBottom() const
{
    LayoutUnit selectionBottom = m_lineBottom;

    if (m_hasAnnotationsAfter)
        selectionBottom += !renderer().style().isFlippedLinesWritingMode() ? computeUnderAnnotationAdjustment(m_lineBottom) : computeOverAnnotationAdjustment(m_lineBottom);
    
    if (!renderer().style().isFlippedLinesWritingMode() || !nextRootBox())
        return selectionBottom;
    
#if !PLATFORM(IOS_FAMILY)
    // See rdar://problem/19692206 ... don't want to do this adjustment for iOS where overlap is ok and handled.
    if (renderer().isRubyBase()) {
        // The ruby base selection should avoid intruding into the ruby text. This is only the case if there is an actual ruby text below us.
        RenderRubyBase* base = &downcast<RenderRubyBase>(renderer());
        RenderRubyRun* run = base->rubyRun();
        if (run) {
            RenderRubyText* text = run->rubyText();
            if (text && text->logicalTop() > base->logicalTop()) {
                // The ruby text is below the ruby base. Just return now in order to avoid painting on top of the ruby text.
                return selectionBottom;
            }
        }
    } else if (renderer().isRubyText()) {
        // The ruby text selection should go all the way to the selection bottom of the containing line.
        RenderRubyText* text = &downcast<RenderRubyText>(renderer());
        RenderRubyRun* run = text->rubyRun();
        if (run && run->inlineBoxWrapper()) {
            RenderRubyBase* base = run->rubyBase();
            if (base && text->logicalTop() > base->logicalTop()) {
                // The ruby text is above the ruby base.
                const LegacyRootInlineBox& containingLine = run->inlineBoxWrapper()->root();
                LayoutUnit enclosingSelectionBottom = containingLine.selectionBottom();
                LayoutUnit deltaBetweenObjects = text->logicalTop() + run->logicalTop();
                LayoutUnit selectionBottomInRubyTextCoords = enclosingSelectionBottom - deltaBetweenObjects;
                return std::min(selectionBottom, selectionBottomInRubyTextCoords);
            }
        }
    }
#endif

    LayoutUnit nextTop = nextRootBox()->selectionTop();
    if (nextTop > selectionBottom && blockFlow().containsFloats()) {
        // The next line has actually been moved further over, probably from a large line-height, but possibly because the
        // line was forced to clear floats. If so, let's check the offsets, and only be willing to use the next
        // line's top if the offsets are greater on both sides.
        LayoutUnit nextLeft = blockFlow().logicalLeftOffsetForLine(nextTop, DoNotIndentText);
        LayoutUnit nextRight = blockFlow().logicalRightOffsetForLine(nextTop, DoNotIndentText);
        LayoutUnit newLeft = blockFlow().logicalLeftOffsetForLine(selectionBottom, DoNotIndentText);
        LayoutUnit newRight = blockFlow().logicalRightOffsetForLine(selectionBottom, DoNotIndentText);
        if (nextLeft > newLeft || nextRight < newRight)
            return selectionBottom;
    }

    return nextTop;
}

RenderBlockFlow& LegacyRootInlineBox::blockFlow() const
{
    return downcast<RenderBlockFlow>(renderer());
}

BidiStatus LegacyRootInlineBox::lineBreakBidiStatus() const
{ 
    return { static_cast<UCharDirection>(m_lineBreakBidiStatusEor), static_cast<UCharDirection>(m_lineBreakBidiStatusLastStrong), static_cast<UCharDirection>(m_lineBreakBidiStatusLast), m_lineBreakContext.copyRef() };
}

void LegacyRootInlineBox::setLineBreakInfo(RenderObject* object, unsigned breakPosition, const BidiStatus& status)
{
    m_lineBreakObj = makeWeakPtr(object);
    m_lineBreakPos = breakPosition;
    m_lineBreakBidiStatusEor = status.eor;
    m_lineBreakBidiStatusLastStrong = status.lastStrong;
    m_lineBreakBidiStatusLast = status.last;
    m_lineBreakContext = status.context;
}

LegacyEllipsisBox* LegacyRootInlineBox::ellipsisBox() const
{
    if (!hasEllipsisBox())
        return nullptr;
    return gEllipsisBoxMap->get(this);
}

void LegacyRootInlineBox::removeLineBoxFromRenderObject()
{
    // Null if we are destroying LegacyLineLayout.
    if (auto* legacyLineLayout = blockFlow().legacyLineLayout())
        legacyLineLayout->lineBoxes().removeLineBox(this);
}

void LegacyRootInlineBox::extractLineBoxFromRenderObject()
{
    blockFlow().legacyLineLayout()->lineBoxes().extractLineBox(this);
}

void LegacyRootInlineBox::attachLineBoxToRenderObject()
{
    blockFlow().legacyLineLayout()->lineBoxes().attachLineBox(this);
}

LayoutRect LegacyRootInlineBox::paddedLayoutOverflowRect(LayoutUnit endPadding) const
{
    LayoutRect lineLayoutOverflow = layoutOverflowRect(lineTop(), lineBottom());
    if (!endPadding)
        return lineLayoutOverflow;
    
    if (isHorizontal()) {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxXEdgeTo(std::max(lineLayoutOverflow.maxX(), LayoutUnit(logicalRight() + endPadding)));
        else
            lineLayoutOverflow.shiftXEdgeTo(std::min(lineLayoutOverflow.x(), LayoutUnit(logicalLeft() - endPadding)));
    } else {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxYEdgeTo(std::max(lineLayoutOverflow.maxY(), LayoutUnit(logicalRight() + endPadding)));
        else
            lineLayoutOverflow.shiftYEdgeTo(std::min(lineLayoutOverflow.y(), LayoutUnit(logicalLeft() - endPadding)));
    }
    
    return lineLayoutOverflow;
}

static void setAscentAndDescent(LayoutUnit& ascent, LayoutUnit& descent, LayoutUnit newAscent, LayoutUnit newDescent, bool& ascentDescentSet)
{
    if (!ascentDescentSet) {
        ascentDescentSet = true;
        ascent = newAscent;
        descent = newDescent;
    } else {
        ascent = std::max(ascent, newAscent);
        descent = std::max(descent, newDescent);
    }
}

void LegacyRootInlineBox::ascentAndDescentForBox(LegacyInlineBox& box, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, LayoutUnit& ascent, LayoutUnit& descent,
    bool& affectsAscent, bool& affectsDescent) const
{
    bool ascentDescentSet = false;

    // Replaced boxes will return 0 for the line-height if line-box-contain says they are
    // not to be included.
    if (box.renderer().isReplaced()) {
        if (lineStyle().lineBoxContain().contains(LineBoxContain::Replaced)) {
            ascent = box.baselinePosition(baselineType());
            descent = roundToInt(box.lineHeight()) - ascent;
            
            // Replaced elements always affect both the ascent and descent.
            affectsAscent = true;
            affectsDescent = true;
        }
        return;
    }

    Vector<const Font*>* usedFonts = nullptr;
    GlyphOverflow* glyphOverflow = nullptr;
    if (is<LegacyInlineTextBox>(box)) {
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(&downcast<LegacyInlineTextBox>(box));
        usedFonts = it == textBoxDataMap.end() ? nullptr : &it->value.first;
        glyphOverflow = it == textBoxDataMap.end() ? nullptr : &it->value.second;
    }
        
    bool includeLeading = includeLeadingForBox(box);
    bool includeFont = includeFontForBox(box);
    
    bool setUsedFont = false;
    bool setUsedFontWithLeading = false;

    const RenderStyle& boxLineStyle = box.lineStyle();
    if (usedFonts && !usedFonts->isEmpty() && (includeFont || (boxLineStyle.lineHeight().isNegative() && includeLeading))) {
        usedFonts->append(&boxLineStyle.fontCascade().primaryFont());
        for (auto& font : *usedFonts) {
            auto& fontMetrics = font->fontMetrics();
            LayoutUnit usedFontAscent { fontMetrics.ascent(baselineType()) };
            LayoutUnit usedFontDescent { fontMetrics.descent(baselineType()) };
            LayoutUnit halfLeading { (fontMetrics.lineSpacing() - fontMetrics.height()) / 2 };
            LayoutUnit usedFontAscentAndLeading { usedFontAscent + halfLeading };
            LayoutUnit usedFontDescentAndLeading { fontMetrics.lineSpacing() - usedFontAscentAndLeading };
            if (includeFont) {
                setAscentAndDescent(ascent, descent, usedFontAscent, usedFontDescent, ascentDescentSet);
                setUsedFont = true;
            }
            if (includeLeading) {
                setAscentAndDescent(ascent, descent, usedFontAscentAndLeading, usedFontDescentAndLeading, ascentDescentSet);
                setUsedFontWithLeading = true;
            }
            if (!affectsAscent)
                affectsAscent = usedFontAscent - box.logicalTop() > 0;
            if (!affectsDescent)
                affectsDescent = usedFontDescent + box.logicalTop() > 0;
        }
    }

    // If leading is included for the box, then we compute that box.
    if (includeLeading && !setUsedFontWithLeading) {
        LayoutUnit ascentWithLeading { box.baselinePosition(baselineType()) };
        LayoutUnit descentWithLeading { box.lineHeight() - ascentWithLeading };
        setAscentAndDescent(ascent, descent, ascentWithLeading, descentWithLeading, ascentDescentSet);
        
        // Examine the font box for inline flows and text boxes to see if any part of it is above the baseline.
        // If the top of our font box relative to the root box baseline is above the root box baseline, then
        // we are contributing to the maxAscent value. Descent is similar. If any part of our font box is below
        // the root box's baseline, then we contribute to the maxDescent value.
        affectsAscent = ascentWithLeading - box.logicalTop() > 0;
        affectsDescent = descentWithLeading + box.logicalTop() > 0;
    }
    
    if (includeFontForBox(box) && !setUsedFont) {
        LayoutUnit fontAscent { boxLineStyle.fontMetrics().ascent(baselineType()) };
        LayoutUnit fontDescent { boxLineStyle.fontMetrics().descent(baselineType()) };
        setAscentAndDescent(ascent, descent, fontAscent, fontDescent, ascentDescentSet);
        affectsAscent = fontAscent - box.logicalTop() > 0;
        affectsDescent = fontDescent + box.logicalTop() > 0;
    }

    if (includeGlyphsForBox(box) && glyphOverflow && glyphOverflow->computeBounds) {
        setAscentAndDescent(ascent, descent, glyphOverflow->top, glyphOverflow->bottom, ascentDescentSet);
        affectsAscent = glyphOverflow->top - box.logicalTop() > 0;
        affectsDescent = glyphOverflow->bottom + box.logicalTop() > 0;
        glyphOverflow->top = std::min(glyphOverflow->top, std::max(0_lu, glyphOverflow->top - boxLineStyle.fontMetrics().ascent(baselineType())));
        glyphOverflow->bottom = std::min(glyphOverflow->bottom, std::max(0_lu, glyphOverflow->bottom - boxLineStyle.fontMetrics().descent(baselineType())));
    }
    
    if (includeInitialLetterForBox(box)) {
        bool canUseGlyphs = glyphOverflow && glyphOverflow->computeBounds;
        auto letterAscent { baselineType() == AlphabeticBaseline ? LayoutUnit(boxLineStyle.fontMetrics().capHeight()) : (canUseGlyphs ? glyphOverflow->top : LayoutUnit(boxLineStyle.fontMetrics().ascent(baselineType()))) };
        auto letterDescent { canUseGlyphs ? glyphOverflow->bottom : (box.isRootInlineBox() ? 0_lu : LayoutUnit(boxLineStyle.fontMetrics().descent(baselineType()))) };
        setAscentAndDescent(ascent, descent, letterAscent, letterDescent, ascentDescentSet);
        affectsAscent = letterAscent - box.logicalTop() > 0;
        affectsDescent = letterDescent + box.logicalTop() > 0;
        if (canUseGlyphs) {
            glyphOverflow->top = std::min(glyphOverflow->top, std::max(0_lu, glyphOverflow->top - boxLineStyle.fontMetrics().ascent(baselineType())));
            glyphOverflow->bottom = std::min(glyphOverflow->bottom, std::max(0_lu, glyphOverflow->bottom - boxLineStyle.fontMetrics().descent(baselineType())));
        }
    }

    if (includeMarginForBox(box)) {
        LayoutUnit ascentWithMargin = boxLineStyle.fontMetrics().ascent(baselineType());
        LayoutUnit descentWithMargin = boxLineStyle.fontMetrics().descent(baselineType());
        if (box.parent() && !box.renderer().isTextOrLineBreak()) {
            ascentWithMargin += box.boxModelObject()->borderAndPaddingBefore() + box.boxModelObject()->marginBefore();
            descentWithMargin += box.boxModelObject()->borderAndPaddingAfter() + box.boxModelObject()->marginAfter();
        }
        setAscentAndDescent(ascent, descent, ascentWithMargin, descentWithMargin, ascentDescentSet);
        
        // Treat like a replaced element, since we're using the margin box.
        affectsAscent = true;
        affectsDescent = true;
    }
}

LayoutUnit LegacyRootInlineBox::verticalPositionForBox(LegacyInlineBox* box, VerticalPositionCache& verticalPositionCache)
{
    if (box->renderer().isTextOrLineBreak())
        return LayoutUnit(box->parent()->logicalTop());
    
    RenderBoxModelObject* renderer = box->boxModelObject();
    ASSERT(renderer->isInline());
    if (!renderer->isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    bool firstLine = isFirstLine();
    if (firstLine && !blockFlow().view().usesFirstLineRules())
        firstLine = false;

    // Check the cache.
    bool isRenderInline = renderer->isRenderInline();
    if (isRenderInline && !firstLine) {
        LayoutUnit cachedPosition;
        if (verticalPositionCache.get(renderer, baselineType(), cachedPosition))
            return cachedPosition;
    }

    LayoutUnit verticalPosition;
    VerticalAlign verticalAlign = renderer->style().verticalAlign();
    if (verticalAlign == VerticalAlign::Top || verticalAlign == VerticalAlign::Bottom)
        return 0;

    RenderElement* parent = renderer->parent();
    if (parent->isRenderInline() && parent->style().verticalAlign() != VerticalAlign::Top && parent->style().verticalAlign() != VerticalAlign::Bottom)
        verticalPosition = box->parent()->logicalTop();
    
    if (verticalAlign != VerticalAlign::Baseline) {
        const RenderStyle& parentLineStyle = firstLine ? parent->firstLineStyle() : parent->style();
        const FontCascade& font = parentLineStyle.fontCascade();
        const FontMetrics& fontMetrics = font.fontMetrics();
        auto fontSize = LayoutUnit { font.pixelSize() };

        LineDirectionMode lineDirection = parent->isHorizontalWritingMode() ? HorizontalLine : VerticalLine;

        if (verticalAlign == VerticalAlign::Sub)
            verticalPosition += fontSize / 5 + 1;
        else if (verticalAlign == VerticalAlign::Super)
            verticalPosition -= fontSize / 3 + 1;
        else if (verticalAlign == VerticalAlign::TextTop)
            verticalPosition += renderer->baselinePosition(baselineType(), firstLine, lineDirection) - fontMetrics.ascent(baselineType());
        else if (verticalAlign == VerticalAlign::Middle)
            verticalPosition = (verticalPosition - LayoutUnit(fontMetrics.xHeight() / 2) - renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType(), firstLine, lineDirection));
        else if (verticalAlign == VerticalAlign::TextBottom) {
            verticalPosition += fontMetrics.descent(baselineType());
            // lineHeight - baselinePosition is always 0 for replaced elements (except inline blocks), so don't bother wasting time in that case.
            if (!renderer->isReplaced() || renderer->isInlineBlockOrInlineTable())
                verticalPosition -= (renderer->lineHeight(firstLine, lineDirection) - renderer->baselinePosition(baselineType(), firstLine, lineDirection));
        } else if (verticalAlign == VerticalAlign::BaselineMiddle)
            verticalPosition += -renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType(), firstLine, lineDirection);
        else if (verticalAlign == VerticalAlign::Length) {
            LayoutUnit lineHeight;
            // Per http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align: 'Percentages: refer to the 'line-height' of the element itself'.
            if (renderer->style().verticalAlignLength().isPercentOrCalculated())
                lineHeight = renderer->style().computedLineHeight();
            else
                lineHeight = renderer->lineHeight(firstLine, lineDirection);
            verticalPosition -= valueForLength(renderer->style().verticalAlignLength(), lineHeight);
        }
    }

    // Store the cached value.
    if (isRenderInline && !firstLine)
        verticalPositionCache.set(renderer, baselineType(), verticalPosition);

    return verticalPosition;
}

bool LegacyRootInlineBox::includeLeadingForBox(LegacyInlineBox& box) const
{
    if (box.renderer().isReplaced() || (box.renderer().isTextOrLineBreak() && !box.behavesLikeText()))
        return false;

    auto lineBoxContain = renderer().style().lineBoxContain();
    return lineBoxContain.contains(LineBoxContain::Inline) || (&box == this && lineBoxContain.contains(LineBoxContain::Block));
}

bool LegacyRootInlineBox::includeFontForBox(LegacyInlineBox& box) const
{
    if (box.renderer().isReplaced() || (box.renderer().isTextOrLineBreak() && !box.behavesLikeText()))
        return false;
    
    if (!box.behavesLikeText() && is<LegacyInlineFlowBox>(box) && !downcast<LegacyInlineFlowBox>(box).hasTextChildren())
        return false;

    return renderer().style().lineBoxContain().contains(LineBoxContain::Font);
}

bool LegacyRootInlineBox::includeGlyphsForBox(LegacyInlineBox& box) const
{
    if (box.renderer().isReplaced() || (box.renderer().isTextOrLineBreak() && !box.behavesLikeText()))
        return false;
    
    if (!box.behavesLikeText() && is<LegacyInlineFlowBox>(box) && !downcast<LegacyInlineFlowBox>(box).hasTextChildren())
        return false;

    return renderer().style().lineBoxContain().contains(LineBoxContain::Glyphs);
}

bool LegacyRootInlineBox::includeInitialLetterForBox(LegacyInlineBox& box) const
{
    if (box.renderer().isReplaced() || (box.renderer().isTextOrLineBreak() && !box.behavesLikeText()))
        return false;
    
    if (!box.behavesLikeText() && is<LegacyInlineFlowBox>(box) && !downcast<LegacyInlineFlowBox>(box).hasTextChildren())
        return false;

    return renderer().style().lineBoxContain().contains(LineBoxContain::InitialLetter);
}

bool LegacyRootInlineBox::includeMarginForBox(LegacyInlineBox& box) const
{
    if (box.renderer().isReplaced() || (box.renderer().isTextOrLineBreak() && !box.behavesLikeText()))
        return false;

    return renderer().style().lineBoxContain().contains(LineBoxContain::InlineBox);
}

bool LegacyRootInlineBox::fitsToGlyphs() const
{
    return renderer().style().lineBoxContain().containsAny({ LineBoxContain::Glyphs, LineBoxContain::InitialLetter });
}

bool LegacyRootInlineBox::includesRootLineBoxFontOrLeading() const
{
    return renderer().style().lineBoxContain().containsAny({ LineBoxContain::Block, LineBoxContain::Inline, LineBoxContain::Font });
}

#if ENABLE(TREE_DEBUGGING)

void LegacyRootInlineBox::outputLineBox(WTF::TextStream& stream, bool mark, int depth) const
{
    stream << "-------- --";
    int printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";
    stream << "Line: (top: " << lineTop() << " bottom: " << lineBottom() << ") with leading (top: " << lineBoxTop() << " bottom: " << lineBoxBottom() << ")";
    stream.nextLine();
    LegacyInlineBox::outputLineBox(stream, mark, depth);
}

const char* LegacyRootInlineBox::boxName() const
{
    return "RootInlineBox";
}
#endif

} // namespace WebCore
