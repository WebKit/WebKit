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
#include "LayoutElementBox.h"
#include "LayoutIntegrationUtils.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include "WritingMode.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {
namespace GridLayoutUtils {

LayoutUnit totalGuttersSize(size_t tracksCount, LayoutUnit gapsSize)
{
    ASSERT(tracksCount);
    return tracksCount ? gapsSize * (tracksCount - 1) : LayoutUnit { };
}

// Resolves a grid item's used margins in one axis.
// FIXME: Resolve percentage and calc() margins against the grid area's inline size.
UsedMargins usedMarginsForAxis(const PlacedGridItem& gridItem, const ComputedSizes& axisSizes)
{
    auto marginStart = [&] -> LayoutUnit {
        if (auto fixedMarginStart = axisSizes.marginStart.tryFixed())
            return LayoutUnit { fixedMarginStart->resolveZoom(gridItem.usedZoom()) };

        if (axisSizes.marginStart.isKnownZero())
            return { };

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    };

    auto marginEnd = [&] -> LayoutUnit {
        if (auto fixedMarginEnd = axisSizes.marginEnd.tryFixed())
            return LayoutUnit { fixedMarginEnd->resolveZoom(gridItem.usedZoom()) };

        if (axisSizes.marginEnd.isKnownZero())
            return { };

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    };

    return UsedMargins { marginStart(), marginEnd() };
}

// https://drafts.csswg.org/css-sizing-4/#aspect-ratio
std::optional<double> preferredAspectRatio(const ElementBox& gridItem)
{
    ASSERT(gridItem.isGridItem());

    auto& computedAspectRatio = gridItem.style().aspectRatio();

    auto isDegenerateRatio = [&] {
        auto ratio = computedAspectRatio.tryRatio();
        return !ratio || !ratio->numerator.value || !ratio->denominator.value;
    };

    // "If the <ratio> is degenerate, the property instead behaves as auto."
    //
    // auto: "Replaced elements with a natural aspect ratio use that aspect ratio;
    // otherwise the box has no preferred aspect ratio."
    if (computedAspectRatio.isAuto() || isDegenerateRatio()) {
        if (gridItem.isReplacedBox() && gridItem.hasIntrinsicRatio())
            return gridItem.intrinsicRatio();
        return { };
    }

    // <ratio>: "The box's preferred aspect ratio is the specified ratio of width / height."
    if (computedAspectRatio.isRatio()) {
        auto ratio = *computedAspectRatio.tryRatio();
        return ratio.numerator.value / ratio.denominator.value;
    }

    // auto && <ratio>: "The preferred aspect ratio is the specified ratio of width / height
    // unless it is a replaced element with a natural aspect ratio, in which case that aspect
    // ratio is used instead."
    if (computedAspectRatio.isAutoAndRatio()) {
        if (gridItem.isReplacedBox() && gridItem.hasIntrinsicRatio())
            return gridItem.intrinsicRatio();
        auto ratio = *computedAspectRatio.tryRatio();
        return ratio.numerator.value / ratio.denominator.value;
    }

    ASSERT_NOT_REACHED();
    return { };
}

// https://drafts.csswg.org/css-sizing-4/#aspect-ratio
static BorderBoxSize blockSizeFromAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit borderBoxInlineSize, LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (gridItem.layoutBox().style().boxSizingForAspectRatio() == BoxSizing::BorderBox)
        return BorderBoxSize { ContentBoxSize { std::max<LayoutUnit>({ }, LayoutUnit { borderBoxInlineSize / aspectRatio } - blockBorderAndPadding) }, blockBorderAndPadding };
    return BorderBoxSize { ContentBoxSize { LayoutUnit { std::max<LayoutUnit>({ }, borderBoxInlineSize - inlineBorderAndPadding) / aspectRatio } }, blockBorderAndPadding };
}

static BorderBoxSize inlineSizeFromAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit borderBoxBlockSize, LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (gridItem.layoutBox().style().boxSizingForAspectRatio() == BoxSizing::BorderBox)
        return BorderBoxSize { ContentBoxSize { std::max<LayoutUnit>({ }, LayoutUnit { borderBoxBlockSize * aspectRatio } - inlineBorderAndPadding) }, inlineBorderAndPadding };
    return BorderBoxSize { ContentBoxSize { LayoutUnit { std::max<LayoutUnit>({ }, borderBoxBlockSize - blockBorderAndPadding) * aspectRatio } }, inlineBorderAndPadding };
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// A grid item with an automatic preferred size fills its grid area (i.e. is sized as for
// align-self: stretch) in two cases:
//
// normal:
// If the grid item has no preferred aspect ratio, and no natural size in the relevant axis
// (if it is a replaced element), the grid item is sized as for align-self: stretch.
//
// stretch:
// Always uses the stretch-fit size. Unlike the normal case this applies even to replaced
// items or items with a preferred aspect ratio (which can distort the ratio).
//
// In both cases the stretch keyword only takes effect when the box's size in the axis is auto
// and neither of its margins in the axis are auto.
// https://drafts.csswg.org/css-align-3/#valdef-justify-self-stretch
static bool isStretchedForAutomaticSize(const PlacedGridItem& placedGridItem, const ComputedSizes& axisSizes, const StyleSelfAlignmentData& axisAlignment)
{
    ASSERT(axisSizes.preferredSize.isAuto());

    if (axisSizes.marginStart.isAuto() || axisSizes.marginEnd.isAuto())
        return false;

    auto alignmentPosition = axisAlignment.position();
    if (alignmentPosition == ItemPosition::Normal)
        return !preferredAspectRatio(placedGridItem.layoutBox()) && !placedGridItem.isReplacedElement();

    return alignmentPosition == ItemPosition::Stretch;
}

static bool hasAutomaticSizeForAspectRatio(const PlacedGridItem& gridItem, LogicalBoxAxis axis, AspectRatioSizingLayoutPhase phase)
{
    auto& axisSizes = axis == LogicalBoxAxis::Inline ? gridItem.inlineAxisSizes() : gridItem.blockAxisSizes();
    auto& axisAlignment = axis == LogicalBoxAxis::Inline ? gridItem.inlineAxisAlignment() : gridItem.blockAxisAlignment();
    switch (phase) {
    case AspectRatioSizingLayoutPhase::TrackSizing:
        ASSERT_NOT_IMPLEMENTED_YET();
        return false;
    case AspectRatioSizingLayoutPhase::ItemSizing:
        return axisSizes.preferredSize.isAuto() && !isStretchedForAutomaticSize(gridItem, axisSizes, axisAlignment);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool sizeDependsOnAspectRatio(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    if (gridItem.isReplacedElement() || !preferredAspectRatio(gridItem.layoutBox()))
        return false;

    // A preferred aspect ratio only ever has an effect if at least one of the box's sizes is automatic.
    // When neither is, any transferred minimum or maximum is capped or floored by the definite
    // preferred size in its destination axis, so it cannot change the used size either.
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Inline, phase) || hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Block, phase);
}

bool inlineContributionMayRequireFullSizingAlgorithmForIntrinsicWidth(const ElementBox& gridItem, WritingMode containerWritingMode)
{
    CheckedRef itemStyle = gridItem.style();

    // The inline contribution of an orthogonal grid item is the grid item's block contribution. We should run the full sizing algorithm.
    if (itemStyle->writingMode().isOrthogonal(containerWritingMode))
        return true;

    // Grid items with an aspect ratio may transfers block size into inline size, so we should run the full sizing algorithm.
    // FIXME: This is only the case when inline size is indefinite, so we can tighten this constraint.
    if (preferredAspectRatio(gridItem))
        return true;

    // A wrapped column flex container (flex-flow: column wrap) lays out its flex lines along the cross (inline) axis,
    // so the number of lines - and thus its inline contribution - grows as the available block size shrinks.
    if (itemStyle->display().isFlexibleBox() && itemStyle->isColumnFlexDirection() && itemStyle->flexWrap().isMultiline())
        return true;

    // A multi-column container fills its columns based on the available block size, so its column count - and thus
    // its inline contribution - depends on the block size.
    if (itemStyle->specifiesColumns())
        return true;

    // If the computed size of an item depends on the size of its containing block, we should run the full sizing algorithm.
    if (sizeDependsOnContainingBlockSize(itemStyle->logicalHeight())
        || sizeDependsOnContainingBlockSize(itemStyle->logicalMinHeight())
        || sizeDependsOnContainingBlockSize(itemStyle->logicalMaxHeight()))
        return true;

    return false;
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
static std::optional<BorderBoxSize> inlineSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> containingBlockSize)
{
    auto& preferredSize = gridItem.inlineAxisSizes().preferredSize;
    if (auto fixedSize = preferredSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedSize, gridItem.usedZoom()) }, borderAndPadding };
    // A percentage or calc() preferred size depends on the containing block size. When it is not
    // known (e.g. during track sizing, before the available space is resolved) the size is not
    // definite and there is no specified size suggestion.
    if (preferredSize.isPercent() || preferredSize.isCalculated()) {
        if (!containingBlockSize)
            return { };
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, *containingBlockSize, gridItem.usedZoom()) }, borderAndPadding };
    }
    if (preferredSize.isAuto())
        return { };

    // https://drafts.csswg.org/css-sizing-3/#definite
    // The intrinsic sizing keywords (min-content, max-content, fit-content, and the non-standard
    // intrinsic/min-intrinsic) size the box from its contents, so the preferred size is not
    // definite and there is no specified size suggestion.
    if (preferredSize.isIntrinsic() || preferredSize.isIntrinsicKeyword() || preferredSize.isMinIntrinsic())
        return { };

    // stretch/-webkit-fill-available resolve to the stretch-fit size, which is only definite when
    // the grid area size is known.
    if (preferredSize.isStretch()) {
        if (!containingBlockSize)
            return { };
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static std::optional<LayoutUnit> NODELETE inlineTransferredSizeSuggestion(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static BorderBoxSize inlineContentSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, LayoutUnit gridAreaInlineSize, const IntegrationUtils& integrationUtils)
{
    ASSERT(!preferredAspectRatio(gridItem.layoutBox()), "Grid items with preferred aspect ratio not supported yet.");
    return BorderBoxSize { ContentBoxSize { integrationUtils.minContentWidthForGridItem(gridItem.layoutBox(), gridAreaInlineSize) }, borderAndPadding };
}

// https://www.w3.org/TR/css-grid-2/#specified-size-suggestion
// If the item's preferred size in the relevant axis is definite, then the specified size suggestion is that size. It is otherwise undefined.
static std::optional<BorderBoxSize> blockSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> containingBlockSize)
{
    auto& preferredSize = gridItem.blockAxisSizes().preferredSize;
    if (auto fixedSize = preferredSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedSize, gridItem.usedZoom()) }, borderAndPadding };
    // A percentage or calc() preferred size depends on the containing block size. When it is not
    // known (e.g. during track sizing, before the available space is resolved) the size is not
    // definite and there is no specified size suggestion.
    if (preferredSize.isPercent() || preferredSize.isCalculated()) {
        if (!containingBlockSize)
            return { };
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, *containingBlockSize, gridItem.usedZoom()) }, borderAndPadding };
    }
    if (preferredSize.isAuto())
        return { };

    // https://drafts.csswg.org/css-sizing-3/#definite
    // The intrinsic sizing keywords (min-content, max-content, fit-content, and the non-standard
    // intrinsic/min-intrinsic) size the box from its contents, so the preferred size is not
    // definite and there is no specified size suggestion.
    if (preferredSize.isIntrinsic() || preferredSize.isIntrinsicKeyword() || preferredSize.isMinIntrinsic())
        return { };

    // stretch/-webkit-fill-available resolve to the stretch-fit size, which is only definite when
    // the grid area size is known.
    if (preferredSize.isStretch()) {
        if (!containingBlockSize)
            return { };
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    ASSERT_NOT_REACHED();
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
    ASSERT(!preferredAspectRatio(gridItem.layoutBox()), "Grid items with preferred aspect ratio not supported yet.");
    return BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().minContentHeightForGridItem(gridItem.layoutBox(), inlineAxisConstraint));
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

// https://www.w3.org/TR/css-sizing-3/#stretch-fit-size
// The size a box would take if its outer size filled the available space in the given axis:
// the available space (the grid area size) less the box's margins, border, and padding.
static BorderBoxSize stretchFitSize(LayoutUnit borderAndPadding, LayoutUnit availableSize, const UsedMargins& usedMargins)
{
    // A content box cannot be negative, so an item whose margins, border, and padding already fill
    // the available space stretches to a zero content box and overflows its grid area.
    return BorderBoxSize { ContentBoxSize { std::max(0_lu, availableSize - usedMargins.marginStart - usedMargins.marginEnd - borderAndPadding) }, borderAndPadding };
}

// https://www.w3.org/TR/css-sizing-3/#fit-content-size
// fit-content size = clamp(min-content, stretch-fit, max-content).
static LayoutUnit fitContentSize(BorderBoxSize minContentSize, BorderBoxSize maxContentSize, BorderBoxSize stretchFit)
{
    return std::max(minContentSize, std::min(maxContentSize, stretchFit)).value;
}

LayoutUnit inlinePreferredSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils& integrationUtils, const UsedMargins& usedMargins)
{
    auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();
    ASSERT(inlineAxisSizes.maximumSize.isFixed() || inlineAxisSizes.maximumSize.isNone());

    auto& preferredSize = inlineAxisSizes.preferredSize;
    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box’s computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box’s used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible.
        if (isStretchedForAutomaticSize(placedGridItem, inlineAxisSizes, placedGridItem.inlineAxisAlignment()))
            return stretchFitSize(borderAndPadding, columnsSize, usedMargins).value;

        // Otherwise, a grid item with a preferred aspect ratio and normal self-alignment is sized
        // consistent with the size calculation rules for block-level elements, so its automatic
        // inline size fills the grid area. Its block size is then transferred through the ratio.
        auto hasAutoMargin = inlineAxisSizes.marginStart.isAuto() || inlineAxisSizes.marginEnd.isAuto();
        if (!placedGridItem.isReplacedElement() && preferredAspectRatio(placedGridItem.layoutBox()) && !hasAutoMargin
            && placedGridItem.inlineAxisAlignment().position() == ItemPosition::Normal) {
            // Only when the block size is automatic too. Otherwise the inline size is transferred
            // from the block size instead; see usedSizesForAspectRatioItem().
            ASSERT(hasAutomaticSizeForAspectRatio(placedGridItem, LogicalBoxAxis::Block, AspectRatioSizingLayoutPhase::ItemSizing));
            return stretchFitSize(borderAndPadding, columnsSize, usedMargins).value;
        }

        // https://drafts.csswg.org/css-grid-1/#grid-item-sizing
        // Otherwise (self-alignment is not stretch, and does not behave as stretch), a non-replaced
        // grid item with an automatic size is sized to its fit-content size in the axis.
        if (!placedGridItem.isReplacedElement()) {
            auto minContentBorderBoxWidth = BorderBoxSize { ContentBoxSize { integrationUtils.minContentWidthForGridItem(placedGridItem.layoutBox(), columnsSize) }, borderAndPadding };
            auto maxContentBorderBoxWidth = BorderBoxSize { ContentBoxSize { integrationUtils.maxContentWidthForGridItem(placedGridItem.layoutBox(), columnsSize) }, borderAndPadding };
            auto stretchFitBorderBoxWidth = stretchFitSize(borderAndPadding, columnsSize, usedMargins);
            return fitContentSize(minContentBorderBoxWidth, maxContentBorderBoxWidth, stretchFitBorderBoxWidth);
        }

        // Otherwise, a replaced grid item with an automatic size uses its natural size in the axis
        // (i.e. it is sized consistent with the block-level replaced element sizing rules).
        auto& layoutBox = placedGridItem.layoutBox();
        if (placedGridItem.isReplacedElement() && layoutBox.hasNaturalWidth())
            return BorderBoxSize { ContentBoxSize { layoutBox.naturalWidth() }, borderAndPadding }.value;

        // FIXME: Handle replaced elements with no natural size in the axis.
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(preferredSize, columnsSize, placedGridItem.usedZoom()) }, borderAndPadding }.value;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
BorderBoxSize automaticMinimumInlineSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaMaximumInlineSize, const IntegrationUtils& integrationUtils)
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

    // However, if in a given dimension the grid item spans only grid tracks that have a fixed max
    // track sizing function, then its specified size suggestion and content size suggestion in that
    // dimension (and its input from this dimension to the transferred size suggestion in the
    // opposite dimension) are further clamped to less than or equal to the stretch fit into the grid
    // area’s maximum size in that dimension, as represented by the sum of those grid tracks’ max
    // track sizing functions plus any intervening fixed gutters.
    auto clampedToGridAreaMaximumSize = [&](BorderBoxSize sizeSuggestion) {
        if (!gridAreaMaximumInlineSize)
            return sizeSuggestion;
        auto usedMargins = usedMarginsForAxis(gridItem, inlineAxisSizes);
        return std::min(sizeSuggestion, stretchFitSize(borderAndPadding, *gridAreaMaximumInlineSize, usedMargins));
    };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = inlineSpecifiedSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize))
            return clampedToGridAreaMaximumSize(*specifiedSizeSuggestion);

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = inlineTransferredSizeSuggestion(gridItem))
                return BorderBoxSize { ContentBoxSize { *transferredSizeSuggestion }, borderAndPadding };
        }
        // else its content size suggestion
        return clampedToGridAreaMaximumSize(inlineContentSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize.value_or(0_lu), integrationUtils));
    };

    auto sizeSuggestion = contentBasedMinimumSize();

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = inlineAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        sizeSuggestion = std::min(sizeSuggestion, BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding });

    return sizeSuggestion;
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
BorderBoxSize automaticMinimumBlockSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    std::optional<LayoutUnit> gridAreaBlockSize, std::optional<LayoutUnit> gridAreaMaximumBlockSize, const GridFormattingContext& formattingContext, LayoutUnit inlineAxisConstraint)
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

    // However, if in a given dimension the grid item spans only grid tracks that have a fixed max
    // track sizing function, then its specified size suggestion and content size suggestion in that
    // dimension (and its input from this dimension to the transferred size suggestion in the
    // opposite dimension) are further clamped to less than or equal to the stretch fit into the grid
    // area’s maximum size in that dimension, as represented by the sum of those grid tracks’ max
    // track sizing functions plus any intervening fixed gutters.
    auto clampedToGridAreaMaximumSize = [&](BorderBoxSize sizeSuggestion) {
        if (!gridAreaMaximumBlockSize)
            return sizeSuggestion;
        auto usedMargins = usedMarginsForAxis(gridItem, blockAxisSizes);
        return std::min(sizeSuggestion, stretchFitSize(borderAndPadding, *gridAreaMaximumBlockSize, usedMargins));
    };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = blockSpecifiedSizeSuggestion(gridItem, borderAndPadding, gridAreaBlockSize))
            return clampedToGridAreaMaximumSize(*specifiedSizeSuggestion);

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = blockTransferredSizeSuggestion(gridItem))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return clampedToGridAreaMaximumSize(blockContentSizeSuggestion(gridItem, inlineAxisConstraint, formattingContext));
    };

    auto sizeSuggestion = contentBasedMinimumSize();

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = blockAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        sizeSuggestion = std::min(sizeSuggestion, BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding });

    return sizeSuggestion;
}

LayoutUnit blockPreferredSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext& formattingContext, LayoutUnit inlineAxisConstraint, const UsedMargins& usedMargins)
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
        if (isStretchedForAutomaticSize(placedGridItem, blockAxisSizes, placedGridItem.blockAxisAlignment()))
            return stretchFitSize(borderAndPadding, rowsSize, usedMargins).value;

        // https://drafts.csswg.org/css-grid-1/#grid-item-sizing
        // Otherwise (self-alignment is not stretch, and does not behave as stretch), a non-replaced
        // grid item with an automatic size is sized to its fit-content size in the axis.
        if (!placedGridItem.isReplacedElement() && !preferredAspectRatio(placedGridItem.layoutBox())) {
            auto stretchFitBorderBoxHeight = stretchFitSize(borderAndPadding, rowsSize, usedMargins);

            auto& integrationUtils = formattingContext.integrationUtils();
            auto minContentBorderBoxHeight = BorderBoxSize::fromIntegrationFunction(integrationUtils.minContentHeightForGridItem(placedGridItem.layoutBox(), inlineAxisConstraint));
            auto maxContentBorderBoxHeight = BorderBoxSize::fromIntegrationFunction(integrationUtils.maxContentHeightForGridItem(placedGridItem.layoutBox(), inlineAxisConstraint));
            return fitContentSize(minContentBorderBoxHeight, maxContentBorderBoxHeight, stretchFitBorderBoxHeight);
        }

        // Otherwise, a replaced grid item with an automatic size uses its natural size in the axis
        // (i.e. it is sized consistent with the block-level replaced element sizing rules).
        auto& layoutBox = placedGridItem.layoutBox();
        if (placedGridItem.isReplacedElement() && layoutBox.hasNaturalHeight())
            return BorderBoxSize { ContentBoxSize { layoutBox.naturalHeight() }, borderAndPadding }.value;

        // FIXME: Handle replaced elements with no natural size in the axis and non-replaced items
        // with a preferred aspect ratio.
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
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
            // The grid area is resolved by now, so it is what the automatic minimum size is clamped
            // to, rather than the sum of the max track sizing functions used during track sizing.
            return automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, columnsSize, columnsSize, integrationUtils).value;
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
            // The grid area is resolved by now, so it is what the automatic minimum size is clamped
            // to, rather than the sum of the max track sizing functions used during track sizing.
            return automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, rowsSize, rowsSize, formattingContext, inlineAxisConstraint).value;
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
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { LayoutUnit { fixedMaximumSize->resolveZoom(gridItem.usedZoom()) } }, borderAndPadding }.value;
    // FIXME: Resolve percentage and calculated maximum sizes against the grid area.
    ASSERT_NOT_IMPLEMENTED_YET();
    return BorderBoxSize::maxSized().value;
}

LayoutUnit blockMaximumSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding)
{
    auto& maximumSize = gridItem.blockAxisSizes().maximumSize;
    if (maximumSize.isNone())
        return BorderBoxSize::maxSized().value;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { LayoutUnit { fixedMaximumSize->resolveZoom(gridItem.usedZoom()) } }, borderAndPadding }.value;
    // FIXME: Resolve percentage and calculated maximum sizes against the grid area.
    ASSERT_NOT_IMPLEMENTED_YET();
    return BorderBoxSize::maxSized().value;
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-grid-1/#layout-algorithm
// Lay out the grid items into their respective containing blocks. Each grid area's width and height are considered definite for this purpose.
LayoutUnit inlineUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit gridAreaInlineSize, const IntegrationUtils& integrationUtils, const UsedMargins& usedMargins)
{
    auto preferredSize = inlinePreferredSize(gridItem, borderAndPadding, gridAreaInlineSize, integrationUtils, usedMargins);
    auto minimumSize = inlineMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, gridAreaInlineSize, integrationUtils);
    auto maximumSize = inlineMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-grid-1/#layout-algorithm
// Lay out the grid items into their respective containing blocks. Each grid area's width and height are considered definite for this purpose.
LayoutUnit blockUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit gridAreaBlockSize, const GridFormattingContext& formattingContext, LayoutUnit gridAreaInlineSize, const UsedMargins& usedMargins)
{
    auto preferredSize = blockPreferredSize(gridItem, borderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, usedMargins);
    auto minimumSize = blockMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize);
    auto maximumSize = blockMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

static bool isDefiniteMinimumSize(const Style::MinimumSize& minimumSize, AspectRatioSizingLayoutPhase phase)
{
    switch (phase) {
    case AspectRatioSizingLayoutPhase::TrackSizing:
        ASSERT_NOT_IMPLEMENTED_YET();
        return false;
    case AspectRatioSizingLayoutPhase::ItemSizing:
        return minimumSize.isFixed() || minimumSize.isPercentOrCalculated();
    }
    ASSERT_NOT_REACHED();
    return false;
}

// A maximum size of none is not definite.
// FIXME: Percentage and calc() maximum sizes are definite too, but are not resolved yet; see inlineMaximumSize().
static bool isDefiniteMaximumSize(const Style::MaximumSize& maximumSize, AspectRatioSizingLayoutPhase phase)
{
    switch (phase) {
    case AspectRatioSizingLayoutPhase::TrackSizing:
        ASSERT_NOT_IMPLEMENTED_YET();
        return false;
    case AspectRatioSizingLayoutPhase::ItemSizing:
        return maximumSize.isFixed();
    }
    ASSERT_NOT_REACHED();
    return false;
}

// https://drafts.csswg.org/css-sizing-4/#aspect-ratio-size-transfers
// Sizing constraints in either axis (the origin axis) are transferred through the preferred aspect
// ratio and applied to any indefinite minimum, maximum, or preferred size in the other axis (the
// destination axis). A transferred minimum is capped, and a transferred maximum floored, by any
// definite preferred size in the destination axis, so a transferred limit can only change the used
// size of an axis whose preferred size is automatic.
static bool needsTransferredBlockMinimumSize(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Block, phase)
        && isDefiniteMinimumSize(gridItem.inlineAxisSizes().minimumSize, phase)
        && !isDefiniteMinimumSize(gridItem.blockAxisSizes().minimumSize, phase);
}

static bool needsTransferredBlockMaximumSize(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Block, phase)
        && isDefiniteMaximumSize(gridItem.inlineAxisSizes().maximumSize, phase)
        && !isDefiniteMaximumSize(gridItem.blockAxisSizes().maximumSize, phase);
}

static bool needsTransferredInlineMinimumSize(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Inline, phase)
        && isDefiniteMinimumSize(gridItem.blockAxisSizes().minimumSize, phase)
        && !isDefiniteMinimumSize(gridItem.inlineAxisSizes().minimumSize, phase);
}

static bool needsTransferredInlineMaximumSize(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Inline, phase)
        && isDefiniteMaximumSize(gridItem.blockAxisSizes().maximumSize, phase)
        && !isDefiniteMaximumSize(gridItem.inlineAxisSizes().maximumSize, phase);
}

// A definite minimum or maximum size in the origin axis, converted through the aspect ratio. The
// transferred minimum is capped by the destination's maximum size. The spec's other caps and floors
// cannot change the used size: an axis with a definite preferred size never receives transferred
// limits, and the minimum size always wins over the maximum size.
static LayoutUnit transferredBlockMinimumSize(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit definiteInlineMinimumSize, LayoutUnit blockMaximumSize,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    return std::min(blockSizeFromAspectRatio(gridItem, aspectRatio, definiteInlineMinimumSize, inlineBorderAndPadding, blockBorderAndPadding).value, blockMaximumSize);
}

static LayoutUnit transferredBlockMaximumSize(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit definiteInlineMaximumSize, LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    return blockSizeFromAspectRatio(gridItem, aspectRatio, definiteInlineMaximumSize, inlineBorderAndPadding, blockBorderAndPadding).value;
}

static LayoutUnit transferredInlineMinimumSize(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit definiteBlockMinimumSize, LayoutUnit inlineMaximumSize,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    return std::min(inlineSizeFromAspectRatio(gridItem, aspectRatio, definiteBlockMinimumSize, inlineBorderAndPadding, blockBorderAndPadding).value, inlineMaximumSize);
}

static LayoutUnit transferredInlineMaximumSize(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit definiteBlockMaximumSize, LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    return inlineSizeFromAspectRatio(gridItem, aspectRatio, definiteBlockMaximumSize, inlineBorderAndPadding, blockBorderAndPadding).value;
}

// The minimum or maximum size of an axis, with the corresponding definite limit of the other axis
// transferred through the aspect ratio when it applies. All sizes are border-box sizes.
static LayoutUnit blockMinimumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, AspectRatioSizingLayoutPhase phase, LayoutUnit blockMinimum, LayoutUnit blockMaximum, LayoutUnit inlineMinimum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredBlockMinimumSize(gridItem, phase))
        return blockMinimum;
    return std::max(blockMinimum, transferredBlockMinimumSize(gridItem, aspectRatio, inlineMinimum, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit blockMaximumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, AspectRatioSizingLayoutPhase phase, LayoutUnit blockMaximum, LayoutUnit inlineMaximum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredBlockMaximumSize(gridItem, phase))
        return blockMaximum;
    return std::min(blockMaximum, transferredBlockMaximumSize(gridItem, aspectRatio, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit inlineMinimumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, AspectRatioSizingLayoutPhase phase, LayoutUnit inlineMinimum, LayoutUnit inlineMaximum, LayoutUnit blockMinimum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredInlineMinimumSize(gridItem, phase))
        return inlineMinimum;
    return std::max(inlineMinimum, transferredInlineMinimumSize(gridItem, aspectRatio, blockMinimum, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit inlineMaximumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, AspectRatioSizingLayoutPhase phase, LayoutUnit inlineMaximum, LayoutUnit blockMaximum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredInlineMaximumSize(gridItem, phase))
        return inlineMaximum;
    return std::min(inlineMaximum, transferredInlineMaximumSize(gridItem, aspectRatio, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

// When both axes are automatic, the inline size is resolved first and the block size is transferred from it.
static LogicalBoxAxis ratioDeterminingAxis(const PlacedGridItem& gridItem, AspectRatioSizingLayoutPhase phase)
{
    return hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Block, phase) ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
}

// The preferred inline and block sizes, before applying min/max. The resolved preferred size in the
// ratio-determining axis gets transferred through the ratio to the other axis.
static std::pair<LayoutUnit, LayoutUnit> preferredSizesForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LogicalBoxAxis ratioDeterminingAxis,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding, LayoutUnit gridAreaInlineSize, LayoutUnit gridAreaBlockSize, const GridFormattingContext& formattingContext,
    const UsedMargins& inlineMargins, const UsedMargins& blockMargins)
{
    if (ratioDeterminingAxis == LogicalBoxAxis::Inline) {
        auto inlinePreferred = inlinePreferredSize(gridItem, inlineBorderAndPadding, gridAreaInlineSize, formattingContext.integrationUtils(), inlineMargins);
        auto blockPreferred = blockSizeFromAspectRatio(gridItem, aspectRatio, inlinePreferred, inlineBorderAndPadding, blockBorderAndPadding).value;
        return { inlinePreferred, blockPreferred };
    }

    ASSERT(hasAutomaticSizeForAspectRatio(gridItem, LogicalBoxAxis::Inline, AspectRatioSizingLayoutPhase::ItemSizing));
    auto blockPreferred = blockPreferredSize(gridItem, blockBorderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, blockMargins);
    auto inlinePreferred = inlineSizeFromAspectRatio(gridItem, aspectRatio, blockPreferred, inlineBorderAndPadding, blockBorderAndPadding).value;
    return { inlinePreferred, blockPreferred };
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-sizing-4/#aspect-ratio-automatic
std::pair<LayoutUnit, LayoutUnit> usedSizesForAspectRatioItem(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding, LayoutUnit gridAreaInlineSize, LayoutUnit gridAreaBlockSize, const GridFormattingContext& formattingContext,
    const UsedMargins& inlineMargins, const UsedMargins& blockMargins)
{
    ASSERT(sizeDependsOnAspectRatio(gridItem, AspectRatioSizingLayoutPhase::ItemSizing));
    auto aspectRatio = gridItem.layoutBox().style().logicalAspectRatio();

    auto [inlinePreferred, blockPreferred] = preferredSizesForAspectRatio(gridItem, aspectRatio, ratioDeterminingAxis(gridItem, AspectRatioSizingLayoutPhase::ItemSizing), inlineBorderAndPadding, blockBorderAndPadding,
        gridAreaInlineSize, gridAreaBlockSize, formattingContext, inlineMargins, blockMargins);

    auto inlineMinimum = inlineMinimumSize(gridItem, columnTrackSizingFunctions, inlineBorderAndPadding, gridAreaInlineSize, formattingContext.integrationUtils());
    auto inlineMaximum = inlineMaximumSize(gridItem, inlineBorderAndPadding);
    auto blockMinimum = blockMinimumSize(gridItem, rowTrackSizingFunctions, blockBorderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize);
    auto blockMaximum = blockMaximumSize(gridItem, blockBorderAndPadding);

    auto usedInlineMinimum = inlineMinimumSizeForAspectRatio(gridItem, aspectRatio, AspectRatioSizingLayoutPhase::ItemSizing, inlineMinimum, inlineMaximum, blockMinimum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedInlineMaximum = inlineMaximumSizeForAspectRatio(gridItem, aspectRatio, AspectRatioSizingLayoutPhase::ItemSizing, inlineMaximum, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedBlockMinimum = blockMinimumSizeForAspectRatio(gridItem, aspectRatio, AspectRatioSizingLayoutPhase::ItemSizing, blockMinimum, blockMaximum, inlineMinimum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedBlockMaximum = blockMaximumSizeForAspectRatio(gridItem, aspectRatio, AspectRatioSizingLayoutPhase::ItemSizing, blockMaximum, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding);

    auto usedInlineSize = std::max(usedInlineMinimum, std::min(usedInlineMaximum, inlinePreferred));
    auto usedBlockSize = std::max(usedBlockMinimum, std::min(usedBlockMaximum, blockPreferred));
    return { usedInlineSize, usedBlockSize };
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

MarginBoxSize inlineAxisMinContentContribution(const PlacedGridItem& gridItem, const IntegrationUtils& integrationUtils)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(integrationUtils.minContentLogicalWidthContribution(gridItem.layoutBox()));
    auto usedMargins = usedMarginsForAxis(gridItem, gridItem.inlineAxisSizes());
    return MarginBoxSize { borderBoxSize, usedMargins.marginStart + usedMargins.marginEnd };
}

MarginBoxSize inlineAxisMaxContentContribution(const PlacedGridItem& gridItem, const IntegrationUtils& integrationUtils)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(integrationUtils.maxContentLogicalWidthContribution(gridItem.layoutBox()));
    auto usedMargins = usedMarginsForAxis(gridItem, gridItem.inlineAxisSizes());
    return MarginBoxSize { borderBoxSize, usedMargins.marginStart + usedMargins.marginEnd };
}

MarginBoxSize blockAxisMinContentContribution(const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint, const GridFormattingContext& formattingContext)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().minContentContributionHeightForGridItem(gridItem.layoutBox(), inlineAxisConstraint));
    auto usedMargins = usedMarginsForAxis(gridItem, gridItem.blockAxisSizes());
    return MarginBoxSize { borderBoxSize, usedMargins.marginStart + usedMargins.marginEnd };
}

MarginBoxSize blockAxisMaxContentContribution(const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint, const GridFormattingContext& formattingContext)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().maxContentContributionHeightForGridItem(gridItem.layoutBox(), inlineAxisConstraint));
    auto usedMargins = usedMarginsForAxis(gridItem, gridItem.blockAxisSizes());
    return MarginBoxSize { borderBoxSize, usedMargins.marginStart + usedMargins.marginEnd };
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

}
}
}
