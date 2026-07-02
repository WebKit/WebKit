/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridLayoutUtils.h"

#include "GridFormattingContext.h"
#include "GridSizeTypes.h"
#include "LayoutIntegrationUtils.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {
namespace GridLayoutUtils {

LayoutUnit totalGuttersSize(size_t tracksCount, LayoutUnit gapsSize)
{
    ASSERT(tracksCount);
    return gapsSize * (tracksCount - 1);
}

static bool NODELETE spansAutoMinTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].min.isAuto())
            return true;
    }
    return false;
}

static bool NODELETE spansFlexMaxTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].max.isFlex())
            return true;
    }
    return false;
}

// https://www.w3.org/TR/css-grid-2/#specified-size-suggestion
// If the item's preferred size in the relevant axis is definite, then the specified size suggestion is that size. It is otherwise undefined.
static std::optional<BorderBoxSize> inlineSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, LayoutUnit containingBlockSize)
{
    auto& preferredSize = gridItem.inlineAxisSizes().preferredSize;
    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, containingBlockSize, gridItem.usedZoom()) }, borderAndPadding };
    if (preferredSize.isAuto())
        return { };
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static std::optional<LayoutUnit> NODELETE inlineTransferredSizeSuggestion(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static BorderBoxSize inlineContentSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const IntegrationUtils& integrationUtils)
{
    ASSERT(!gridItem.preferredAspectRatio(), "Grid items with preferred aspect ratio not supported yet.");
    return BorderBoxSize { ContentBoxSize { integrationUtils.minContentWidth(gridItem.layoutBox()) }, borderAndPadding };
}

// https://www.w3.org/TR/css-grid-2/#specified-size-suggestion
// If the item's preferred size in the relevant axis is definite, then the specified size suggestion is that size. It is otherwise undefined.
static std::optional<BorderBoxSize> blockSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, LayoutUnit containingBlockSize)
{
    auto& preferredSize = gridItem.blockAxisSizes().preferredSize;
    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, containingBlockSize, gridItem.usedZoom()) }, borderAndPadding };
    if (preferredSize.isAuto())
        return { };
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static std::optional<BorderBoxSize> NODELETE blockTransferredSizeSuggestion(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
// The content size suggestion is the min-content size in the relevant axis, clamped, if it has a preferred aspect ratio,
// by any definite opposite-axis minimum and maximum sizes converted through the aspect ratio.
// https://drafts.csswg.org/css-sizing-3/#sizing-values
// For a box’s block size, unless otherwise specified, this [min-content] is equivalent to its automatic size.
static BorderBoxSize blockContentSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint, const GridFormattingContext& formattingContext)
{
    // FIXME: Clamp by opposite-axis min/max sizes converted through the aspect ratio.
    ASSERT(!gridItem.preferredAspectRatio(), "Grid items with preferred aspect ratio not supported yet.");
    return BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().minContentHeight(gridItem.layoutBox(), inlineAxisConstraint));
}

// https://drafts.csswg.org/css-overflow-3/#overflow-properties
// The scroll, auto, and hidden values are known as the scrollable values of overflow.
static bool NODELETE hasScrollableInlineComputedOverflowValue(const PlacedGridItem& gridItem)
{
    auto computedOverflow = gridItem.layoutBox().style().overflowX();
    return computedOverflow == Overflow::Hidden || computedOverflow == Overflow::Scroll || computedOverflow == Overflow::Auto;
}

// https://drafts.csswg.org/css-overflow-3/#overflow-properties
// The scroll, auto, and hidden values are known as the scrollable values of overflow.
static bool NODELETE hasScrollableBlockComputedOverflowValue(const PlacedGridItem& gridItem)
{
    auto computedOverflow = gridItem.layoutBox().style().overflowY();
    return computedOverflow == Overflow::Hidden || computedOverflow == Overflow::Scroll || computedOverflow == Overflow::Auto;
}

LayoutUnit inlinePreferredSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit columnsSize)
{
    auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();
    ASSERT(inlineAxisSizes.maximumSize.isFixed() || inlineAxisSizes.maximumSize.isNone());

    auto& preferredSize = inlineAxisSizes.preferredSize;
    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        auto alignmentPosition = placedGridItem.inlineAxisAlignment().position();

        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box’s computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box’s used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible.
        auto& marginStart = inlineAxisSizes.marginStart;
        auto& marginEnd = inlineAxisSizes.marginEnd;
        if ((alignmentPosition == ItemPosition::Normal) && !placedGridItem.preferredAspectRatio() && !placedGridItem.isReplacedElement()
            && !marginStart.isAuto() && !marginEnd.isAuto()) {
            auto& usedZoom = placedGridItem.usedZoom();
            auto usedMarginStart = LayoutUnit { marginStart.tryFixed()->resolveZoom(usedZoom) };
            auto usedMarginEnd = LayoutUnit { marginEnd.tryFixed()->resolveZoom(usedZoom) };
            auto stretchedContentBoxWidth = ContentBoxSize { columnsSize - usedMarginStart - usedMarginEnd - borderAndPadding };
            return BorderBoxSize { stretchedContentBoxWidth, borderAndPadding }.value;
        }

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, columnsSize, placedGridItem.usedZoom()) }, borderAndPadding }.value;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
static BorderBoxSize automaticMinimumInlineSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit containingBlockSize, const IntegrationUtils& integrationUtils)
{
    auto& inlineAxisSizes = gridItem.inlineAxisSizes();
    ASSERT(inlineAxisSizes.minimumSize.isAuto());

    // the used value of its automatic minimum size in a given axis is the content-based
    // minimum size if all of the following are true
    //
    // its computed overflow is not a scrollable overflow value
    // it spans at least one track in that axis whose min track sizing function is auto
    // if it spans more than one track in that axis, none of those tracks are flexible
    //
    // Otherwise, the automatic minimum size is zero, as usual.
    if (hasScrollableInlineComputedOverflowValue(gridItem))
        return BorderBoxSize::zeroSized();

    auto gridItemColumnStartLine = gridItem.columnStartLine();
    auto gridItemColumnEndLine = gridItem.columnEndLine();
    if (!spansAutoMinTrackSizingFunction({ gridItemColumnStartLine, gridItemColumnEndLine }, trackSizingFunctions))
        return BorderBoxSize::zeroSized();

    auto gridItemColumnSpanCount = gridItemColumnEndLine - gridItemColumnStartLine;
    if (gridItemColumnSpanCount > 1 && spansFlexMaxTrackSizingFunction({ gridItemColumnStartLine, gridItemColumnEndLine }, trackSizingFunctions))
        return BorderBoxSize::zeroSized();

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = inlineSpecifiedSizeSuggestion(gridItem, borderAndPadding, containingBlockSize))
            return *specifiedSizeSuggestion;

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = inlineTransferredSizeSuggestion(gridItem))
                return BorderBoxSize { ContentBoxSize { *transferredSizeSuggestion }, borderAndPadding };
        }
        // else its content size suggestion
        return inlineContentSizeSuggestion(gridItem, borderAndPadding, integrationUtils);
    };

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = inlineAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed()) {
        auto maximumBorderBoxSize = BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding };
        return std::min(contentBasedMinimumSize(), maximumBorderBoxSize);
    }
    return contentBasedMinimumSize();
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
static BorderBoxSize automaticMinimumBlockSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit containingBlockSize, const GridFormattingContext& formattingContext, LayoutUnit inlineAxisConstraint)
{
    auto& blockAxisSizes = gridItem.blockAxisSizes();
    ASSERT(blockAxisSizes.minimumSize.isAuto());

    // the used value of its automatic minimum size in a given axis is the content-based
    // minimum size if all of the following are true
    //
    // its computed overflow is not a scrollable overflow value
    // it spans at least one track in that axis whose min track sizing function is auto
    // if it spans more than one track in that axis, none of those tracks are flexible
    //
    // Otherwise, the automatic minimum size is zero, as usual.
    if (hasScrollableBlockComputedOverflowValue(gridItem))
        return BorderBoxSize::zeroSized();

    auto gridItemRowStartLine = gridItem.rowStartLine();
    auto gridItemRowEndLine = gridItem.rowEndLine();
    if (!spansAutoMinTrackSizingFunction({ gridItemRowStartLine, gridItemRowEndLine }, trackSizingFunctions))
        return BorderBoxSize::zeroSized();

    auto gridItemRowSpanCount = gridItemRowEndLine - gridItemRowStartLine;
    if (gridItemRowSpanCount > 1 && spansFlexMaxTrackSizingFunction({ gridItemRowStartLine, gridItemRowEndLine }, trackSizingFunctions))
        return BorderBoxSize::zeroSized();

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = blockSpecifiedSizeSuggestion(gridItem, borderAndPadding, containingBlockSize))
            return *specifiedSizeSuggestion;

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = blockTransferredSizeSuggestion(gridItem))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return blockContentSizeSuggestion(gridItem, inlineAxisConstraint, formattingContext);
    };

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = blockAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed()) {
        auto maximumBorderBoxSize = BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding };
        return std::min(contentBasedMinimumSize(), maximumBorderBoxSize);
    }
    return contentBasedMinimumSize();
}

bool hasStretchedBlockSize(const PlacedGridItem& placedGridItem)
{
    if (!placedGridItem.blockAxisSizes().preferredSize.isAuto())
        return false;
    if (placedGridItem.preferredAspectRatio() || placedGridItem.isReplacedElement())
        return false;
    auto& marginStart = placedGridItem.blockAxisSizes().marginStart;
    auto& marginEnd = placedGridItem.blockAxisSizes().marginEnd;
    return placedGridItem.blockAxisAlignment().position() == ItemPosition::Normal && !marginStart.isAuto() && !marginEnd.isAuto();
}

LayoutUnit stretchedBlockSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize)
{
    ASSERT(hasStretchedBlockSize(placedGridItem));
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    auto& usedZoom = placedGridItem.usedZoom();
    auto usedMarginStart = LayoutUnit { blockAxisSizes.marginStart.tryFixed()->resolveZoom(usedZoom) };
    auto usedMarginEnd = LayoutUnit { blockAxisSizes.marginEnd.tryFixed()->resolveZoom(usedZoom) };
    auto stretchedContentBoxHeight = ContentBoxSize { rowsSize - usedMarginStart - usedMarginEnd - borderAndPadding };
    return BorderBoxSize { stretchedContentBoxHeight, borderAndPadding }.value;
}

LayoutUnit blockPreferredSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize)
{
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    ASSERT(blockAxisSizes.maximumSize.isFixed() || blockAxisSizes.maximumSize.isNone());

    auto& preferredSize = blockAxisSizes.preferredSize;
    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        //
        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box's computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box's used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible.
        if (hasStretchedBlockSize(placedGridItem))
            return stretchedBlockSize(placedGridItem, borderAndPadding, rowsSize);
    }

    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, rowsSize, placedGridItem.usedZoom()) }, borderAndPadding }.value;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

LayoutUnit inlineMinimumSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils& integrationUtils)
{
    auto& minimumSize = gridItem.inlineAxisSizes().minimumSize;
    return WTF::switchOn(minimumSize,
        [&](const Style::MinimumSize::Fixed& fixed) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) }, borderAndPadding }.value;
        },
        [&](const Style::MinimumSize::Percentage& percentage) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(percentage, columnsSize) }, borderAndPadding }.value;
        },
        [&](const Style::MinimumSize::Calc& calculated) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(calculated, columnsSize, gridItem.usedZoom()) }, borderAndPadding }.value;
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            return automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, columnsSize, integrationUtils).value;
        },
        [](const auto&) -> LayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        });
}

LayoutUnit blockMinimumSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext& formattingContext, LayoutUnit inlineAxisConstraint)
{
    auto& minimumSize = gridItem.blockAxisSizes().minimumSize;
    return WTF::switchOn(minimumSize,
        [&](const Style::MinimumSize::Fixed& fixed) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) }, borderAndPadding }.value;
        },
        [&](const Style::MinimumSize::Percentage& percentage) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(percentage, rowsSize) }, borderAndPadding }.value;
        },
        [&](const Style::MinimumSize::Calc& calculated) {
            return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(calculated, rowsSize, gridItem.usedZoom()) }, borderAndPadding }.value;
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            return automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, rowsSize, formattingContext, inlineAxisConstraint).value;
        },
        [](const auto&) -> LayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        });
}

LayoutUnit inlineMaximumSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding)
{
    auto& maximumSize = gridItem.inlineAxisSizes().maximumSize;
    if (maximumSize.isNone())
        return BorderBoxSize::maxSized().value;
    return BorderBoxSize { ContentBoxSize { LayoutUnit { maximumSize.tryFixed()->resolveZoom(gridItem.usedZoom()) } }, borderAndPadding }.value;
}

LayoutUnit blockMaximumSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding)
{
    auto& maximumSize = gridItem.blockAxisSizes().maximumSize;
    if (maximumSize.isNone())
        return BorderBoxSize::maxSized().value;
    return BorderBoxSize { ContentBoxSize { LayoutUnit { maximumSize.tryFixed()->resolveZoom(gridItem.usedZoom()) } }, borderAndPadding }.value;
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-grid-1/#layout-algorithm
// Lay out the grid items into their respective containing blocks. Each grid area's width and height are considered definite for this purpose.
LayoutUnit inlineUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils& integrationUtils)
{
    auto preferredSize = inlinePreferredSize(gridItem, borderAndPadding, columnsSize);
    auto minimumSize = inlineMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, columnsSize, integrationUtils);
    auto maximumSize = inlineMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-grid-1/#layout-algorithm
// Lay out the grid items into their respective containing blocks. Each grid area's width and height are considered definite for this purpose.
LayoutUnit blockUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext& formattingContext, LayoutUnit inlineAxisConstraint)
{
    auto preferredSize = blockPreferredSize(gridItem, borderAndPadding, rowsSize);
    auto minimumSize = blockMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, rowsSize, formattingContext, inlineAxisConstraint);
    auto maximumSize = blockMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes& trackSizes, LayoutUnit gap)
{
    auto trackSizesBefore = trackSizes.subspan(0, gridLineIndex);
    auto sumOfTrackSizes = std::reduce(trackSizesBefore.begin(), trackSizesBefore.end());

    // https://drafts.csswg.org/css-grid-1/#gutters
    // A grid line used as an item's start edge is preceded by gridLineIndex tracks, and a
    // gutter follows each of those tracks. So the line is offset by gridLineIndex gutters.
    auto numberOfGaps = gridLineIndex;

    return sumOfTrackSizes + (numberOfGaps * gap);
}

LayoutUnit gridAreaDimensionSize(size_t startLine, size_t endLine, const TrackSizes& trackSizes, LayoutUnit gap)
{
    ASSERT(endLine > startLine);

    // https://drafts.csswg.org/css-grid-1/#gutters
    // The size of a grid area is the sum of the sizes of the tracks it spans, plus the gutters
    // *between* those tracks. A span of N tracks contains only N - 1 interior gutters — the
    // gutter that follows the area's last track belongs to the space between grid areas, not
    // to the area itself.
    auto spannedTrackSizes = trackSizes.subspan(startLine, endLine - startLine);

    auto sumOfTrackSizes = std::reduce(spannedTrackSizes.begin(), spannedTrackSizes.end());
    auto numberOfInteriorGaps = spannedTrackSizes.size() - 1;
    return sumOfTrackSizes + (numberOfInteriorGaps * gap);
}

LayoutUnit inlineAxisMinContentContribution(const PlacedGridItem& gridItem, LayoutUnit blockAxisConstraint, const IntegrationUtils& integrationUtils)
{
    UNUSED_PARAM(blockAxisConstraint);
    return BorderBoxSize::fromIntegrationFunction(integrationUtils.minContentLogicalWidthContribution(gridItem.layoutBox())).value;
}

LayoutUnit inlineAxisMaxContentContribution(const PlacedGridItem& gridItem, LayoutUnit blockAxisConstraint, const IntegrationUtils& integrationUtils)
{
    UNUSED_PARAM(blockAxisConstraint);
    return BorderBoxSize::fromIntegrationFunction(integrationUtils.maxContentLogicalWidthContribution(gridItem.layoutBox())).value;
}

// FIXME: this should be marginBoxHeight().
LayoutUnit blockAxisMinContentContribution(const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint, const GridFormattingContext& formattingContext)
{
    formattingContext.integrationUtils().layoutWithFormattingContextForBox(gridItem.layoutBox(), inlineAxisConstraint);
    return BorderBoxSize::fromIntegrationFunction(formattingContext.geometryForGridItem(gridItem.layoutBox()).borderBoxHeight()).value;
}

// FIXME: this should be marginBoxHeight().
LayoutUnit blockAxisMaxContentContribution(const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint, const GridFormattingContext& formattingContext)
{
    formattingContext.integrationUtils().layoutWithFormattingContextForBox(gridItem.layoutBox(), inlineAxisConstraint);
    return BorderBoxSize::fromIntegrationFunction(formattingContext.geometryForGridItem(gridItem.layoutBox()).borderBoxHeight()).value;
}

// https://www.w3.org/TR/css-sizing-3/#behave-as-auto
// To have a common term for both when width/height computes to auto and
// when it is defined to behave as if auto were specified.
// (as in the case of block percentage heights resolving against an indefinite size, see CSS2§10.5),
// the property is said to behave as auto in both of these cases.
bool preferredSizeBehavesAsAuto(const Style::PreferredSize& preferredSize)
{
    // FIXME: Handle cases where preferred size is not auto but behaves as auto,
    // such as percentage height resolving against indefinite size.
    return preferredSize.isAuto();
}

bool preferredSizeDependsOnContainingBlockSize(const Style::PreferredSize& preferredSize)
{
    return preferredSize.isStretch() || preferredSize.isPercentOrCalculated();
}

}
}
}
