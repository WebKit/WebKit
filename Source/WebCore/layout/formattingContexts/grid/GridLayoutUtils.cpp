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

// https://drafts.csswg.org/css-grid-1/#collapsed-gutter
// The gutters on either side of a collapsed track coincide, and a collapsed track at either end of
// the grid has no gutter on its outer side, so collapsing it leaves no gutter on either side. Only
// the gutters between non-collapsed tracks remain, which may be none if every track is collapsed.
LayoutUnit totalGuttersSize(size_t tracksCount, const BitVector& collapsedTracksList, LayoutUnit gapsSize)
{
    auto nonCollapsedTracksCount = tracksCount - collapsedTracksList.bitCount();
    return nonCollapsedTracksCount ? gapsSize * (nonCollapsedTracksCount - 1) : LayoutUnit { };
}

// https://drafts.csswg.org/css-grid-2/#min-size-auto
// The maximum size of the grid area an item is placed in, as represented by the sum of its grid
// tracks' max track sizing functions plus any intervening fixed gutters. Absent unless every one of
// those tracks has a fixed max track sizing function, since the automatic minimum size is only
// clamped to the grid area in that case.
std::optional<LayoutUnit> gridAreaMaximumSize(size_t startLine, size_t endLine, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize,
    const AxisConstraint& axisConstraint)
{
    LayoutUnit maximumSize;
    for (auto trackIndex : std::views::iota(startLine, endLine)) {
        auto& trackSizingFunction = trackSizingFunctions[trackIndex];
        auto maximumBreadthFunction = trackSizingFunction.max.tryBreadth();
        if (!maximumBreadthFunction || !maximumBreadthFunction->isLength())
            return { };

        auto& maximumBreadth = maximumBreadthFunction->length();
        if (auto fixedMaximumBreadth = maximumBreadth.tryFixed()) {
            maximumSize += Style::evaluate<LayoutUnit>(*fixedMaximumBreadth, trackSizingFunction.zoom);
            continue;
        }

        if (!maximumBreadth.isPercentOrCalculated())
            return { };
        // When the constraint is indefinite, GridFormattingContext will transform any percent and
        // calc functions into auto.
        if (axisConstraint.scenario() != AxisConstraint::FreeSpaceScenario::Definite) {
            ASSERT_NOT_REACHED();
            return { };
        }
        maximumSize += Style::evaluate<LayoutUnit>(maximumBreadth, axisConstraint.availableSpace(), trackSizingFunction.zoom);
    }
    return maximumSize + totalGuttersSize(endLine - startLine, { }, gapSize);
}

Style::GridTrackSize trackSizeWithPercentagesConvertedToAuto(const Style::GridTrackSize& trackSize)
{
    return WTF::switchOn(trackSize,
        [&trackSize](const Style::GridTrackBreadth& breadth) {
            if (breadth.isPercentOrCalculated())
                return Style::GridTrackSize { CSS::Keyword::Auto { } };
            return trackSize;
        },
        [&trackSize](const Style::GridTrackSize::FitContent& fitContent) {
            // Without a limit, fit-content() is minmax(auto, max-content). Unlike an auto track,
            // it must not be stretched by Stretch auto Tracks.
            if (fitContent->value.isPercentOrCalculated())
                return Style::GridTrackSize { Style::GridTrackSize::MinMax { CSS::Keyword::Auto { }, CSS::Keyword::MaxContent { } } };
            return trackSize;
        },
        [&trackSize](const Style::GridTrackBreadth::Flex&) {
            return trackSize;
        },
        [](const Style::GridTrackSize::MinMax& minMax) {
            auto minTrackSizingFunction = !minMax->min.isPercentOrCalculated() ? minMax->min : Style::GridTrackBreadth { CSS::Keyword::Auto { } };
            auto maxTrackSizingFunction = !minMax->max.isPercentOrCalculated() ? minMax->max : Style::GridTrackBreadth { CSS::Keyword::Auto { } };
            return Style::GridTrackSize { Style::GridTrackSize::MinMax { minTrackSizingFunction, maxTrackSizingFunction } };
        });
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
        return BorderBoxSize { ContentBoxSize { std::max(0_lu, LayoutUnit { borderBoxInlineSize / aspectRatio } - blockBorderAndPadding) }, blockBorderAndPadding };
    return BorderBoxSize { ContentBoxSize { LayoutUnit { std::max(0_lu, borderBoxInlineSize - inlineBorderAndPadding) / aspectRatio } }, blockBorderAndPadding };
}

static BorderBoxSize inlineSizeFromAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit borderBoxBlockSize, LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (gridItem.layoutBox().style().boxSizingForAspectRatio() == BoxSizing::BorderBox)
        return BorderBoxSize { ContentBoxSize { std::max(0_lu, LayoutUnit { borderBoxBlockSize * aspectRatio } - inlineBorderAndPadding) }, inlineBorderAndPadding };
    return BorderBoxSize { ContentBoxSize { LayoutUnit { std::max(0_lu, borderBoxBlockSize - blockBorderAndPadding) * aspectRatio } }, inlineBorderAndPadding };
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

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
bool hasFitContentBlockSize(const PlacedGridItem& placedGridItem)
{
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    if (!blockAxisSizes.preferredSize.isAuto())
        return false;

    if (isStretchedForAutomaticSize(placedGridItem, blockAxisSizes, placedGridItem.blockAxisAlignment()))
        return false;

    return !placedGridItem.isReplacedElement() && !preferredAspectRatio(placedGridItem.layoutBox());
}

// Whether the grid item's size in the axis is automatic for the purposes of its preferred aspect ratio,
// once its grid area is definite. A stretched size fills the grid area, so it is not automatic.
static bool hasAutomaticSizeDuringItemSizing(const PlacedGridItem& gridItem, LogicalBoxAxis axis)
{
    auto& axisSizes = axis == LogicalBoxAxis::Inline ? gridItem.inlineAxisSizes() : gridItem.blockAxisSizes();
    auto& axisAlignment = axis == LogicalBoxAxis::Inline ? gridItem.inlineAxisAlignment() : gridItem.blockAxisAlignment();
    return axisSizes.preferredSize.isAuto() && !isStretchedForAutomaticSize(gridItem, axisSizes, axisAlignment);
}

bool sizeDependsOnAspectRatio(const PlacedGridItem& gridItem)
{
    if (gridItem.isReplacedElement() || !preferredAspectRatio(gridItem.layoutBox()))
        return false;

    // A preferred aspect ratio only ever has an effect if at least one of the box's sizes is automatic.
    // When neither is, any transferred minimum or maximum is capped or floored by the definite
    // preferred size in its destination axis, so it cannot change the used size either.
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Inline) || hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Block);
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

static bool LEGACY_NODELETE spansAutoMinTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].min.isAuto())
            return true;
    }
    return false;
}

static bool LEGACY_NODELETE spansFlexMaxTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].max.isFlex())
            return true;
    }
    return false;
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

// https://www.w3.org/TR/css-grid-2/#specified-size-suggestion
// If the item's preferred size in the relevant axis is definite, then the specified size suggestion is that size. It is otherwise undefined.
static std::optional<BorderBoxSize> inlineSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> containingBlockSize, const UsedMargins& usedMargins)
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
        return stretchFitSize(borderAndPadding, *containingBlockSize, usedMargins);
    }

    ASSERT_NOT_REACHED();
    return { };
}

// A minimum or maximum size is definite when it is fixed, or when it is a percentage or calc() and the grid area size is known.
static std::optional<BorderBoxSize> definiteMinimumSize(const PlacedGridItem& gridItem, const Style::MinimumSize& minimumSize, LayoutUnit borderAndPadding, std::optional<LayoutUnit> gridAreaSize)
{
    if (auto fixedMinimumSize = minimumSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMinimumSize, gridItem.usedZoom()) }, borderAndPadding };
    if (minimumSize.isPercentOrCalculated() && gridAreaSize)
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(minimumSize, *gridAreaSize, gridItem.usedZoom()) }, borderAndPadding };
    return { };
}

static std::optional<BorderBoxSize> definiteMaximumSize(const PlacedGridItem& gridItem, const Style::MaximumSize& maximumSize, LayoutUnit borderAndPadding, std::optional<LayoutUnit> gridAreaSize)
{
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding };
    if (maximumSize.isPercentOrCalculated() && gridAreaSize)
        return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(maximumSize, *gridAreaSize, gridItem.usedZoom()) }, borderAndPadding };
    return { };
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
// The content size suggestion is the min-content size in the relevant axis, clamped, if it has a preferred aspect ratio,
// by any definite opposite-axis minimum and maximum sizes converted through the aspect ratio.
static BorderBoxSize inlineContentSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize, const IntegrationUtils& integrationUtils)
{
    auto& layoutBox = gridItem.layoutBox();
    auto contentSizeSuggestion = BorderBoxSize { ContentBoxSize { integrationUtils.minContentWidthForGridItem(layoutBox, gridAreaInlineSize) }, borderAndPadding };

    auto aspectRatio = preferredAspectRatio(layoutBox);
    if (!aspectRatio)
        return contentSizeSuggestion;

    auto blockBorderAndPadding = integrationUtils.borderAndPaddingForGridItem(layoutBox, gridAreaInlineSize).second;
    auto& blockAxisSizes = gridItem.blockAxisSizes();
    if (auto blockMaximumSize = definiteMaximumSize(gridItem, blockAxisSizes.maximumSize, blockBorderAndPadding, gridAreaBlockSize))
        contentSizeSuggestion = std::min(contentSizeSuggestion, inlineSizeFromAspectRatio(gridItem, *aspectRatio, blockMaximumSize->value, borderAndPadding, blockBorderAndPadding));
    if (auto blockMinimumSize = definiteMinimumSize(gridItem, blockAxisSizes.minimumSize, blockBorderAndPadding, gridAreaBlockSize))
        contentSizeSuggestion = std::max(contentSizeSuggestion, inlineSizeFromAspectRatio(gridItem, *aspectRatio, blockMinimumSize->value, borderAndPadding, blockBorderAndPadding));
    return contentSizeSuggestion;
}

// https://www.w3.org/TR/css-grid-2/#specified-size-suggestion
// If the item's preferred size in the relevant axis is definite, then the specified size suggestion is that size. It is otherwise undefined.
static std::optional<BorderBoxSize> blockSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> containingBlockSize, const UsedMargins& usedMargins)
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
        return stretchFitSize(borderAndPadding, *containingBlockSize, usedMargins);
    }

    ASSERT_NOT_REACHED();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#transferred-size-suggestion
static std::optional<BorderBoxSize> inlineTransferredSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize,
    std::optional<LayoutUnit> gridAreaMaximumBlockSize, const IntegrationUtils& integrationUtils)
{
    // If the item has a preferred aspect ratio
    auto& layoutBox = gridItem.layoutBox();
    auto aspectRatio = preferredAspectRatio(layoutBox);
    if (!aspectRatio)
        return { };

    // and its preferred size in the opposite axis is definite
    auto blockBorderAndPadding = integrationUtils.borderAndPaddingForGridItem(layoutBox, gridAreaInlineSize).second;
    auto blockMargins = usedMarginsForAxis(gridItem, gridItem.blockAxisSizes());
    auto definiteBlockSize = blockSpecifiedSizeSuggestion(gridItem, blockBorderAndPadding, gridAreaBlockSize, blockMargins);
    if (!definiteBlockSize)
        return { };

    // then the transferred size suggestion is that size (clamped by the opposite-axis minimum and maximum sizes if they are definite)
    auto transferredBlockSize = *definiteBlockSize;
    auto& blockAxisSizes = gridItem.blockAxisSizes();
    if (auto maximumSize = definiteMaximumSize(gridItem, blockAxisSizes.maximumSize, blockBorderAndPadding, gridAreaBlockSize))
        transferredBlockSize = std::min(transferredBlockSize, *maximumSize);
    if (auto minimumSize = definiteMinimumSize(gridItem, blockAxisSizes.minimumSize, blockBorderAndPadding, gridAreaBlockSize))
        transferredBlockSize = std::max(transferredBlockSize, *minimumSize);

    // That input from the opposite axis is further clamped to less than or equal to the stretch fit into the opposite axis' grid
    // area maximum size when the item spans only grid tracks that have a fixed max track sizing function
    if (gridAreaMaximumBlockSize)
        transferredBlockSize = std::min(transferredBlockSize, stretchFitSize(blockBorderAndPadding, *gridAreaMaximumBlockSize, blockMargins));

    // converted through the aspect ratio.
    return inlineSizeFromAspectRatio(gridItem, *aspectRatio, transferredBlockSize.value, borderAndPadding, blockBorderAndPadding);
}

// https://drafts.csswg.org/css-grid-1/#transferred-size-suggestion
static std::optional<BorderBoxSize> blockTransferredSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, LayoutUnit gridAreaInlineSize, std::optional<LayoutUnit> gridAreaMaximumInlineSize,
    const GridFormattingContext& formattingContext)
{
    // If the item has a preferred aspect ratio
    auto& layoutBox = gridItem.layoutBox();
    auto aspectRatio = preferredAspectRatio(layoutBox);
    if (!aspectRatio)
        return { };

    // and its preferred size in the opposite axis is definite
    auto inlineBorderAndPadding = formattingContext.integrationUtils().borderAndPaddingForGridItem(layoutBox, gridAreaInlineSize).first;
    auto inlineMargins = usedMarginsForAxis(gridItem, gridItem.inlineAxisSizes());
    auto definiteInlineSize = inlineSpecifiedSizeSuggestion(gridItem, inlineBorderAndPadding, gridAreaInlineSize, inlineMargins);
    if (!definiteInlineSize)
        return { };

    // then the transferred size suggestion is that size (clamped by the opposite-axis minimum and maximum sizes if they are definite)
    auto transferredInlineSize = *definiteInlineSize;
    auto& inlineAxisSizes = gridItem.inlineAxisSizes();
    if (auto maximumSize = definiteMaximumSize(gridItem, inlineAxisSizes.maximumSize, inlineBorderAndPadding, gridAreaInlineSize))
        transferredInlineSize = std::min(transferredInlineSize, *maximumSize);
    if (auto minimumSize = definiteMinimumSize(gridItem, inlineAxisSizes.minimumSize, inlineBorderAndPadding, gridAreaInlineSize))
        transferredInlineSize = std::max(transferredInlineSize, *minimumSize);

    // That input from the opposite axis is further clamped to less than or equal to the stretch fit into the opposite axis' grid
    // area maximum size when the item spans only grid tracks that have a fixed max track sizing function
    if (gridAreaMaximumInlineSize)
        transferredInlineSize = std::min(transferredInlineSize, stretchFitSize(inlineBorderAndPadding, *gridAreaMaximumInlineSize, inlineMargins));

    // converted through the aspect ratio.
    return blockSizeFromAspectRatio(gridItem, *aspectRatio, transferredInlineSize.value, inlineBorderAndPadding, borderAndPadding);
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
// The content size suggestion is the min-content size in the relevant axis, clamped, if it has a preferred aspect ratio,
// by any definite opposite-axis minimum and maximum sizes converted through the aspect ratio.
// https://drafts.csswg.org/css-sizing-3/#sizing-values
// For a box’s block size, unless otherwise specified, this [min-content] is equivalent to its automatic size.
static BorderBoxSize blockContentSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit gridAreaInlineSize, const GridFormattingContext& formattingContext)
{
    auto& layoutBox = gridItem.layoutBox();
    auto& integrationUtils = formattingContext.integrationUtils();
    auto contentSizeSuggestion = BorderBoxSize::fromIntegrationFunction(integrationUtils.minContentHeightForGridItem(layoutBox, gridAreaInlineSize));

    auto aspectRatio = preferredAspectRatio(layoutBox);
    if (!aspectRatio)
        return contentSizeSuggestion;

    auto [inlineBorderAndPadding, blockBorderAndPadding] = integrationUtils.borderAndPaddingForGridItem(layoutBox, gridAreaInlineSize);
    auto& inlineAxisSizes = gridItem.inlineAxisSizes();
    auto& inlineMaximumSize = inlineAxisSizes.maximumSize;
    if (inlineMaximumSize.isFixed() || inlineMaximumSize.isPercentOrCalculated()) {
        auto borderBoxInlineMaximumSize = BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(inlineMaximumSize, gridAreaInlineSize, gridItem.usedZoom()) }, inlineBorderAndPadding };
        contentSizeSuggestion = std::min(contentSizeSuggestion, blockSizeFromAspectRatio(gridItem, *aspectRatio, borderBoxInlineMaximumSize.value, inlineBorderAndPadding, blockBorderAndPadding));
    }
    auto& inlineMinimumSize = inlineAxisSizes.minimumSize;
    if (inlineMinimumSize.isFixed() || inlineMinimumSize.isPercentOrCalculated()) {
        auto borderBoxInlineMinimumSize = BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(inlineMinimumSize, gridAreaInlineSize, gridItem.usedZoom()) }, inlineBorderAndPadding };
        contentSizeSuggestion = std::max(contentSizeSuggestion, blockSizeFromAspectRatio(gridItem, *aspectRatio, borderBoxInlineMinimumSize.value, inlineBorderAndPadding, blockBorderAndPadding));
    }
    return contentSizeSuggestion;
}

// https://drafts.csswg.org/css-overflow-3/#overflow-properties
// The scroll, auto, and hidden values are known as the scrollable values of overflow.
static bool LEGACY_NODELETE hasScrollableInlineComputedOverflowValue(const PlacedGridItem& gridItem)
{
    auto computedOverflow = gridItem.layoutBox().style().overflowX();
    return computedOverflow == Overflow::Hidden || computedOverflow == Overflow::Scroll || computedOverflow == Overflow::Auto;
}

// https://drafts.csswg.org/css-overflow-3/#overflow-properties
// The scroll, auto, and hidden values are known as the scrollable values of overflow.
static bool LEGACY_NODELETE hasScrollableBlockComputedOverflowValue(const PlacedGridItem& gridItem)
{
    auto computedOverflow = gridItem.layoutBox().style().overflowY();
    return computedOverflow == Overflow::Hidden || computedOverflow == Overflow::Scroll || computedOverflow == Overflow::Auto;
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
            ASSERT(hasAutomaticSizeDuringItemSizing(placedGridItem, LogicalBoxAxis::Block));
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
    std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaMaximumInlineSize, const IntegrationUtils& integrationUtils, std::optional<LayoutUnit> gridAreaBlockSize,
    std::optional<LayoutUnit> gridAreaMaximumBlockSize, const UsedMargins& usedMargins)
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
        return std::min(sizeSuggestion, stretchFitSize(borderAndPadding, *gridAreaMaximumInlineSize, usedMargins));
    };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = inlineSpecifiedSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize, usedMargins))
            return clampedToGridAreaMaximumSize(*specifiedSizeSuggestion);

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = inlineTransferredSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize, gridAreaBlockSize, gridAreaMaximumBlockSize, integrationUtils))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return clampedToGridAreaMaximumSize(inlineContentSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize, gridAreaBlockSize, integrationUtils));
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
    std::optional<LayoutUnit> gridAreaBlockSize, std::optional<LayoutUnit> gridAreaMaximumBlockSize, const GridFormattingContext& formattingContext, LayoutUnit gridAreaInlineSize,
    std::optional<LayoutUnit> gridAreaMaximumInlineSize, const UsedMargins& usedMargins)
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
        return std::min(sizeSuggestion, stretchFitSize(borderAndPadding, *gridAreaMaximumBlockSize, usedMargins));
    };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = blockSpecifiedSizeSuggestion(gridItem, borderAndPadding, gridAreaBlockSize, usedMargins))
            return clampedToGridAreaMaximumSize(*specifiedSizeSuggestion);

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = blockTransferredSizeSuggestion(gridItem, borderAndPadding, gridAreaInlineSize, gridAreaMaximumInlineSize, formattingContext))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return clampedToGridAreaMaximumSize(blockContentSizeSuggestion(gridItem, gridAreaInlineSize, formattingContext));
    };

    auto sizeSuggestion = contentBasedMinimumSize();

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = blockAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        sizeSuggestion = std::min(sizeSuggestion, BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()) }, borderAndPadding });

    return sizeSuggestion;
}

LayoutUnit blockPreferredSize(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext& formattingContext, LayoutUnit gridAreaInlineSize, const UsedMargins& usedMargins)
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
            auto minContentBorderBoxHeight = BorderBoxSize::fromIntegrationFunction(integrationUtils.minContentHeightForGridItem(placedGridItem.layoutBox(), gridAreaInlineSize));
            auto maxContentBorderBoxHeight = BorderBoxSize::fromIntegrationFunction(integrationUtils.maxContentHeightForGridItem(placedGridItem.layoutBox(), gridAreaInlineSize));
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
    LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils& integrationUtils, LayoutUnit gridAreaBlockSize, const UsedMargins& usedMargins)
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
            return automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, columnsSize, columnsSize, integrationUtils, gridAreaBlockSize, gridAreaBlockSize, usedMargins).value;
        },
        [](const auto&) -> LayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        });
}

LayoutUnit blockMinimumSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit borderAndPadding, LayoutUnit rowsSize, const GridFormattingContext& formattingContext, LayoutUnit gridAreaInlineSize, const UsedMargins& usedMargins)
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
            return automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, rowsSize, rowsSize, formattingContext, gridAreaInlineSize, gridAreaInlineSize, usedMargins).value;
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
LayoutUnit inlineUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit gridAreaInlineSize, const IntegrationUtils& integrationUtils, LayoutUnit gridAreaBlockSize, const UsedMargins& usedMargins)
{
    auto preferredSize = inlinePreferredSize(gridItem, borderAndPadding, gridAreaInlineSize, integrationUtils, usedMargins);
    auto minimumSize = inlineMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, gridAreaInlineSize, integrationUtils, gridAreaBlockSize, usedMargins);
    auto maximumSize = inlineMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// https://drafts.csswg.org/css-grid-1/#layout-algorithm
// Lay out the grid items into their respective containing blocks. Each grid area's width and height are considered definite for this purpose.
LayoutUnit blockUsedSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit gridAreaBlockSize, const GridFormattingContext& formattingContext, LayoutUnit gridAreaInlineSize, const UsedMargins& usedMargins)
{
    auto preferredSize = blockPreferredSize(gridItem, borderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, usedMargins);
    auto minimumSize = blockMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, usedMargins);
    auto maximumSize = blockMaximumSize(gridItem, borderAndPadding);
    return std::max(minimumSize, std::min(maximumSize, preferredSize));
}

// The grid area is definite, so percentage and calc() minimum sizes can be resolved against it.
static bool isDefiniteMinimumSizeDuringItemSizing(const Style::MinimumSize& minimumSize)
{
    return minimumSize.isFixed() || minimumSize.isPercentOrCalculated();
}

// A maximum size of none is not definite.
// FIXME: Percentage and calc() maximum sizes are definite too, but are not resolved yet; see inlineMaximumSize().
static bool isDefiniteMaximumSizeDuringItemSizing(const Style::MaximumSize& maximumSize)
{
    return maximumSize.isFixed();
}

// https://drafts.csswg.org/css-sizing-4/#aspect-ratio-size-transfers
// Sizing constraints in either axis (the origin axis) are transferred through the preferred aspect
// ratio and applied to any indefinite minimum, maximum, or preferred size in the other axis (the
// destination axis). A transferred minimum is capped, and a transferred maximum floored, by any
// definite preferred size in the destination axis, so a transferred limit can only change the used
// size of an axis whose preferred size is automatic.
static bool needsTransferredBlockMinimumSize(const PlacedGridItem& gridItem)
{
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Block)
        && isDefiniteMinimumSizeDuringItemSizing(gridItem.inlineAxisSizes().minimumSize)
        && !isDefiniteMinimumSizeDuringItemSizing(gridItem.blockAxisSizes().minimumSize);
}

static bool needsTransferredBlockMaximumSize(const PlacedGridItem& gridItem)
{
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Block)
        && isDefiniteMaximumSizeDuringItemSizing(gridItem.inlineAxisSizes().maximumSize)
        && !isDefiniteMaximumSizeDuringItemSizing(gridItem.blockAxisSizes().maximumSize);
}

static bool needsTransferredInlineMinimumSize(const PlacedGridItem& gridItem)
{
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Inline)
        && isDefiniteMinimumSizeDuringItemSizing(gridItem.blockAxisSizes().minimumSize)
        && !isDefiniteMinimumSizeDuringItemSizing(gridItem.inlineAxisSizes().minimumSize);
}

static bool needsTransferredInlineMaximumSize(const PlacedGridItem& gridItem)
{
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Inline)
        && isDefiniteMaximumSizeDuringItemSizing(gridItem.blockAxisSizes().maximumSize)
        && !isDefiniteMaximumSizeDuringItemSizing(gridItem.inlineAxisSizes().maximumSize);
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
static LayoutUnit blockMinimumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit blockMinimum, LayoutUnit blockMaximum, LayoutUnit inlineMinimum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredBlockMinimumSize(gridItem))
        return blockMinimum;
    return std::max(blockMinimum, transferredBlockMinimumSize(gridItem, aspectRatio, inlineMinimum, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit blockMaximumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit blockMaximum, LayoutUnit inlineMaximum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredBlockMaximumSize(gridItem))
        return blockMaximum;
    return std::min(blockMaximum, transferredBlockMaximumSize(gridItem, aspectRatio, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit inlineMinimumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit inlineMinimum, LayoutUnit inlineMaximum, LayoutUnit blockMinimum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredInlineMinimumSize(gridItem))
        return inlineMinimum;
    return std::max(inlineMinimum, transferredInlineMinimumSize(gridItem, aspectRatio, blockMinimum, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

static LayoutUnit inlineMaximumSizeForAspectRatio(const PlacedGridItem& gridItem, double aspectRatio, LayoutUnit inlineMaximum, LayoutUnit blockMaximum,
    LayoutUnit inlineBorderAndPadding, LayoutUnit blockBorderAndPadding)
{
    if (!needsTransferredInlineMaximumSize(gridItem))
        return inlineMaximum;
    return std::min(inlineMaximum, transferredInlineMaximumSize(gridItem, aspectRatio, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding));
}

// When both axes are automatic, the inline size is resolved first and the block size is transferred from it.
static LogicalBoxAxis ratioDeterminingAxis(const PlacedGridItem& gridItem)
{
    return hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Block) ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
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

    ASSERT(hasAutomaticSizeDuringItemSizing(gridItem, LogicalBoxAxis::Inline));
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
    ASSERT(sizeDependsOnAspectRatio(gridItem));
    auto aspectRatio = gridItem.layoutBox().style().logicalAspectRatio();

    auto [inlinePreferred, blockPreferred] = preferredSizesForAspectRatio(gridItem, aspectRatio, ratioDeterminingAxis(gridItem), inlineBorderAndPadding, blockBorderAndPadding,
        gridAreaInlineSize, gridAreaBlockSize, formattingContext, inlineMargins, blockMargins);

    auto inlineMinimum = inlineMinimumSize(gridItem, columnTrackSizingFunctions, inlineBorderAndPadding, gridAreaInlineSize, formattingContext.integrationUtils(), gridAreaBlockSize, inlineMargins);
    auto inlineMaximum = inlineMaximumSize(gridItem, inlineBorderAndPadding);
    auto blockMinimum = blockMinimumSize(gridItem, rowTrackSizingFunctions, blockBorderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, blockMargins);
    auto blockMaximum = blockMaximumSize(gridItem, blockBorderAndPadding);

    auto usedInlineMinimum = inlineMinimumSizeForAspectRatio(gridItem, aspectRatio, inlineMinimum, inlineMaximum, blockMinimum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedInlineMaximum = inlineMaximumSizeForAspectRatio(gridItem, aspectRatio, inlineMaximum, blockMaximum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedBlockMinimum = blockMinimumSizeForAspectRatio(gridItem, aspectRatio, blockMinimum, blockMaximum, inlineMinimum, inlineBorderAndPadding, blockBorderAndPadding);
    auto usedBlockMaximum = blockMaximumSizeForAspectRatio(gridItem, aspectRatio, blockMaximum, inlineMaximum, inlineBorderAndPadding, blockBorderAndPadding);

    auto usedInlineSize = std::max(usedInlineMinimum, std::min(usedInlineMaximum, inlinePreferred));
    auto usedBlockSize = std::max(usedBlockMinimum, std::min(usedBlockMaximum, blockPreferred));
    return { usedInlineSize, usedBlockSize };
}

LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes& trackSizes, const BitVector& collapsedTracksList, LayoutUnit gap)
{
    auto trackSizesBefore = trackSizes.subspan(0, gridLineIndex);
    auto sumOfTrackSizes = std::reduce(trackSizesBefore.begin(), trackSizesBefore.end());

    // https://drafts.csswg.org/css-grid-1/#gutters
    // A grid line used as an item's start edge is preceded by gridLineIndex tracks, and a
    // gutter follows each of those tracks. So the line is offset by gridLineIndex gutters.
    auto numberOfGaps = gridLineIndex;

    // https://drafts.csswg.org/css-grid-1/#collapsed-gutter
    // A collapsed track's gutters coincide with the gutter on its other side, so no gutter follows it.
    for (auto collapsedTrackIndex : collapsedTracksList) {
        if (collapsedTrackIndex >= gridLineIndex)
            break;
        --numberOfGaps;
    }
    // If only collapsed tracks follow the line, the last non-collapsed track before it has no gutter
    // on the other side of them to coincide with, so its gutter collapses away too.
    auto tracksAfter = std::views::iota(gridLineIndex, trackSizes.size());
    auto onlyCollapsedTracksFollow = !tracksAfter.empty() && std::ranges::all_of(tracksAfter, [&](size_t trackIndex) {
        return collapsedTracksList.get(trackIndex);
    });
    if (numberOfGaps && onlyCollapsedTracksFollow)
        --numberOfGaps;

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

MarginBoxSize blockAxisMinContentContribution(const PlacedGridItem& gridItem, LayoutUnit gridAreaInlineSize, const GridFormattingContext& formattingContext)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().minContentContributionHeightForGridItem(gridItem.layoutBox(), gridAreaInlineSize));
    auto usedMargins = usedMarginsForAxis(gridItem, gridItem.blockAxisSizes());
    return MarginBoxSize { borderBoxSize, usedMargins.marginStart + usedMargins.marginEnd };
}

MarginBoxSize blockAxisMaxContentContribution(const PlacedGridItem& gridItem, LayoutUnit gridAreaInlineSize, const GridFormattingContext& formattingContext)
{
    auto borderBoxSize = BorderBoxSize::fromIntegrationFunction(formattingContext.integrationUtils().maxContentContributionHeightForGridItem(gridItem.layoutBox(), gridAreaInlineSize));
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

// https://drafts.csswg.org/css-align-3/#baseline-terms
Vector<BaselineSharingGroup> baselineSharingGroups(const PlacedGridItems& gridItems, LogicalBoxAxis alignmentAxis)
{
    Vector<BaselineSharingGroup> groups;
    for (auto [gridItemIndex, gridItem] : WTF::indexedRange(gridItems)) {
        auto& alignment = alignmentAxis == LogicalBoxAxis::Inline ? gridItem.inlineAxisAlignment() : gridItem.blockAxisAlignment();
        auto position = alignment.position();
        if (position != ItemPosition::Baseline && position != ItemPosition::LastBaseline)
            continue;

        // FIXME: Items whose size depends on an intrinsically-sized track do not participate in
        // baseline alignment and use their fallback alignment instead.
        // https://drafts.csswg.org/css-grid-1/#column-align

        // Items that span multiple tracks participate in first/last baseline alignment within their
        // start-most/end-most track.
        auto baselineAlignmentPreference = position == ItemPosition::Baseline ? Style::BaselineAlignmentPreferenceKind::First : Style::BaselineAlignmentPreferenceKind::Last;
        auto [startLine, endLine] = alignmentAxis == LogicalBoxAxis::Inline
            ? std::pair { gridItem.columnStartLine(), gridItem.columnEndLine() }
            : std::pair { gridItem.rowStartLine(), gridItem.rowEndLine() };
        auto trackIndex = baselineAlignmentPreference == Style::BaselineAlignmentPreferenceKind::First ? startLine : endLine - 1;

        auto groupIndex = groups.findIf([&](auto& group) {
            return group.trackIndex == trackIndex && group.baselineAlignmentPreference == baselineAlignmentPreference;
        });
        if (groupIndex == notFound)
            groups.append({ trackIndex, baselineAlignmentPreference, { gridItemIndex } });
        else
            groups[groupIndex].gridItemIndexes.append(gridItemIndex);
    }
    return groups;
}

}
}
}
