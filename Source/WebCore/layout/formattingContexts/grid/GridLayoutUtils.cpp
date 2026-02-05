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

namespace WebCore {
namespace Layout {
namespace GridLayoutUtils {

LayoutUnit computeGapValue(const Style::GapGutter& gap)
{
    if (gap.isNormal())
        return { };

    // Only handle fixed length gaps for now
    if (auto fixedGap = gap.tryFixed())
        return Style::evaluate<LayoutUnit>(*fixedGap, 0_lu, Style::ZoomNeeded { });

    ASSERT_NOT_REACHED();
    return { };
}

static bool spansAutoMinTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].min.isAuto())
            return true;
    }
    return false;
}

static bool spansFlexMaxTrackSizingFunction(WTF::Range<size_t> spannedTrackIndexes, const TrackSizingFunctionsList& trackSizingFunctions)
{
    for (auto trackIndex : std::views::iota(spannedTrackIndexes.begin(), spannedTrackIndexes.end())) {
        if (trackSizingFunctions[trackIndex].max.isFlex())
            return true;
    }
    return false;
}

static std::optional<LayoutUnit> inlineSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding)
{
    auto& preferredSize = gridItem.inlineAxisSizes().preferredSize;
    return WTF::switchOn(preferredSize,
        [&](const Style::PreferredSize::Fixed fixedSize) -> std::optional<LayoutUnit> {
            return Style::evaluate<LayoutUnit>(fixedSize, gridItem.usedZoom()) + borderAndPadding;
        },
        [](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    );
}

static std::optional<LayoutUnit> inlineTransferredSizeSuggestion(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static LayoutUnit inlineContentSizeSuggestion(const PlacedGridItem& gridItem, const IntegrationUtils& integrationUtils)
{
    ASSERT(!gridItem.hasPreferredAspectRatio(), "Grid items with preferred aspect ratio not supported yet.");
    return integrationUtils.minContentWidth(gridItem.layoutBox());
}

static std::optional<LayoutUnit> blockSpecifiedSizeSuggestion(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding)
{
    auto& preferredSize = gridItem.blockAxisSizes().preferredSize;
    return WTF::switchOn(preferredSize,
        [&](const Style::PreferredSize::Fixed fixedSize) -> std::optional<LayoutUnit> {
            return Style::evaluate<LayoutUnit>(fixedSize, gridItem.usedZoom()) + borderAndPadding;
        },
        [](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
    });
}

static std::optional<LayoutUnit> blockTransferredSizeSuggestion(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static LayoutUnit blockContentSizeSuggestion(const PlacedGridItem& gridItem, const IntegrationUtils& integrationUtils)
{
    ASSERT(!gridItem.hasPreferredAspectRatio(), "Grid items with preferred aspect ratio not supported yet.");
    return integrationUtils.minContentHeight(gridItem.layoutBox());
}

static bool hasScrollableInlineOverflow(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return false;
}

static bool hasScrollableBlockOverflow(const PlacedGridItem&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return false;
}

LayoutUnit usedInlineSizeForGridItem(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit columnsSize)
{
    auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();
    ASSERT(inlineAxisSizes.minimumSize.isFixed() && (inlineAxisSizes.maximumSize.isFixed() || inlineAxisSizes.maximumSize.isNone()));

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
        // necessary to make its outer size as close to filling the alignment container as possible
        // while still respecting the constraints imposed by min-height/min-width/max-height/max-width.
        auto& marginStart = inlineAxisSizes.marginStart;
        auto& marginEnd = inlineAxisSizes.marginEnd;
        if ((alignmentPosition == ItemPosition::Normal) && !placedGridItem.hasPreferredAspectRatio() && !placedGridItem.isReplacedElement()
            && !marginStart.isAuto() && !marginEnd.isAuto()) {
            auto& usedZoom = placedGridItem.usedZoom();

            auto minimumSize = LayoutUnit { inlineAxisSizes.minimumSize.tryFixed()->resolveZoom(usedZoom) };
            auto maximumSize = [&inlineAxisSizes, &usedZoom] {
                auto& computedMaximumSize = inlineAxisSizes.maximumSize;
                if (computedMaximumSize.isNone())
                    return LayoutUnit::max();
                return LayoutUnit { computedMaximumSize.tryFixed()->resolveZoom(usedZoom) };
            };

            auto stretchedWidth = columnsSize - LayoutUnit { marginStart.tryFixed()->resolveZoom(usedZoom) } - LayoutUnit { marginEnd.tryFixed()->resolveZoom(usedZoom) } - borderAndPadding;
            return std::max(minimumSize, std::min(maximumSize(), stretchedWidth));
        }

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return Style::evaluate<LayoutUnit>(preferredSize, columnsSize, placedGridItem.usedZoom()) + borderAndPadding;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
static LayoutUnit automaticMinimumInlineSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    const IntegrationUtils& integrationUtils)
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
    if (hasScrollableInlineOverflow(gridItem))
        return { };

    auto gridItemColumnStartLine = gridItem.columnStartLine();
    auto gridItemColumnEndLine = gridItem.columnEndLine();
    if (!spansAutoMinTrackSizingFunction({ gridItemColumnStartLine, gridItemColumnEndLine }, trackSizingFunctions))
        return { };

    auto gridItemColumnSpanCount = gridItemColumnEndLine - gridItemColumnStartLine;
    if (gridItemColumnSpanCount > 1 && spansFlexMaxTrackSizingFunction({ gridItemColumnStartLine, gridItemColumnEndLine }, trackSizingFunctions))
        return { };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = inlineSpecifiedSizeSuggestion(gridItem, borderAndPadding))
            return *specifiedSizeSuggestion;

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = inlineTransferredSizeSuggestion(gridItem))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return inlineContentSizeSuggestion(gridItem, integrationUtils);
    };

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = inlineAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        return std::min(contentBasedMinimumSize(), Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()));
    return contentBasedMinimumSize();
}

// https://drafts.csswg.org/css-grid-1/#min-size-auto
static LayoutUnit automaticMinimumBlockSize(const PlacedGridItem& gridItem, LayoutUnit borderAndPadding, const TrackSizingFunctionsList& trackSizingFunctions,
    const IntegrationUtils& integrationUtils)
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
    if (hasScrollableBlockOverflow(gridItem))
        return { };

    auto gridItemRowStartLine = gridItem.rowStartLine();
    auto gridItemRowEndLine = gridItem.rowEndLine();
    if (!spansAutoMinTrackSizingFunction({ gridItemRowStartLine, gridItemRowEndLine }, trackSizingFunctions))
        return { };

    auto gridItemRowSpanCount = gridItemRowEndLine - gridItemRowStartLine;
    if (gridItemRowSpanCount > 1 && spansFlexMaxTrackSizingFunction({ gridItemRowStartLine, gridItemRowEndLine }, trackSizingFunctions))
        return { };

    // The content-based minimum size for a grid item in a given dimension is its
    auto contentBasedMinimumSize = [&] {
        // specified size suggestion if it exists
        if (auto specifiedSizeSuggestion = blockSpecifiedSizeSuggestion(gridItem, borderAndPadding))
            return *specifiedSizeSuggestion;

        // otherwise its transferred size suggestion if that exists and the element is replaced
        if (gridItem.isReplacedElement()) {
            if (auto transferredSizeSuggestion = blockTransferredSizeSuggestion(gridItem))
                return *transferredSizeSuggestion;
        }
        // else its content size suggestion
        return blockContentSizeSuggestion(gridItem, integrationUtils);
    };

    // In all cases, the size suggestion is additionally clamped by the maximum size in
    // the affected axis, if it’s definite
    auto& maximumSize = blockAxisSizes.maximumSize;
    if (auto fixedMaximumSize = maximumSize.tryFixed())
        return std::min(contentBasedMinimumSize(), Style::evaluate<LayoutUnit>(*fixedMaximumSize, gridItem.usedZoom()));
    return contentBasedMinimumSize();
}

LayoutUnit usedBlockSizeForGridItem(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize)
{
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    auto& preferredSize = blockAxisSizes.preferredSize;
    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        auto alignmentPosition = placedGridItem.blockAxisAlignment().position();

        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box’s computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box’s used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible
        // while still respecting the constraints imposed by min-height/min-width/max-height/max-width.
        auto& marginStart = blockAxisSizes.marginStart;
        auto& marginEnd = blockAxisSizes.marginEnd;
        if ((alignmentPosition == ItemPosition::Normal) && !placedGridItem.hasPreferredAspectRatio() && !placedGridItem.isReplacedElement()
            && !marginStart.isAuto() && !marginEnd.isAuto()) {
            auto& usedZoom = placedGridItem.usedZoom();

            auto minimumSize = LayoutUnit { blockAxisSizes.minimumSize.tryFixed()->resolveZoom(usedZoom) };
            auto maximumSize = [&blockAxisSizes, &usedZoom] {
                auto& computedMaximumSize = blockAxisSizes.maximumSize;
                if (computedMaximumSize.isNone())
                    return LayoutUnit::max();
                return LayoutUnit { computedMaximumSize.tryFixed()->resolveZoom(usedZoom) };
            };
            auto stretchedBlockSize = rowsSize - LayoutUnit { marginStart.tryFixed()->resolveZoom(usedZoom) } - LayoutUnit { marginEnd.tryFixed()->resolveZoom(usedZoom) } - borderAndPadding;
            return std::max(minimumSize, std::min(maximumSize(), stretchedBlockSize));
        }
    }

    if (preferredSize.isFixed() || preferredSize.isPercent() || preferredSize.isCalculated())
        return Style::evaluate<LayoutUnit>(preferredSize, rowsSize, placedGridItem.usedZoom()) + borderAndPadding;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

LayoutUnit usedInlineMinimumSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit borderAndPadding, LayoutUnit columnsSize, const IntegrationUtils& integrationUtils)
{
    auto& minimumSize = gridItem.inlineAxisSizes().minimumSize;
    return WTF::switchOn(minimumSize,
        [&](const Style::MinimumSize::Fixed& fixed) {
            return Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) + borderAndPadding;
        },
        [&](const Style::MinimumSize::Percentage& percentage) {
            return Style::evaluate<LayoutUnit>(percentage, columnsSize) + borderAndPadding;
        },
        [&](const Style::MinimumSize::Calc& calculated) {
            return Style::evaluate<LayoutUnit>(calculated, columnsSize, gridItem.usedZoom()) + borderAndPadding;
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            return automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, integrationUtils);
        },
        [](const auto&) -> LayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        });
}

LayoutUnit usedBlockMinimumSize(const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions,
    LayoutUnit borderAndPadding, LayoutUnit rowsSize, const IntegrationUtils& integrationUtils)
{
    auto& minimumSize = gridItem.blockAxisSizes().minimumSize;
    return WTF::switchOn(minimumSize,
        [&](const Style::MinimumSize::Fixed& fixed) {
            return Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) + borderAndPadding;
        },
        [&](const Style::MinimumSize::Percentage& percentage) {
            return Style::evaluate<LayoutUnit>(percentage, rowsSize) + borderAndPadding;
        },
        [&](const Style::MinimumSize::Calc& calculated) {
            return Style::evaluate<LayoutUnit>(calculated, rowsSize, gridItem.usedZoom()) + borderAndPadding;
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            return automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, integrationUtils);
        },
        [](const auto&) -> LayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        });
}

LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes& trackSizes, LayoutUnit gap)
{
    auto trackSizesBefore = trackSizes.subspan(0, gridLineIndex);
    auto sumOfTrackSizes = std::reduce(trackSizesBefore.begin(), trackSizesBefore.end());

    // For grid line i, there are i-1 gaps before it (between the i tracks)
    auto numberOfGaps = gridLineIndex > 0 ? gridLineIndex - 1 : 0;

    return sumOfTrackSizes + (numberOfGaps * gap);
}

LayoutUnit gridAreaDimensionSize(size_t startLine, size_t endLine, const TrackSizes& trackSizes, LayoutUnit gap)
{
    ASSERT(endLine > startLine);

    auto startPosition = computeGridLinePosition(startLine, trackSizes, gap);
    auto endPosition = computeGridLinePosition(endLine, trackSizes, gap);
    ASSERT(endPosition >= startPosition);

    return endPosition - startPosition;
}

LayoutUnit inlineAxisMinContentContribution(const ElementBox& gridItem, const IntegrationUtils& integrationUtils)
{
    return integrationUtils.preferredMinWidth(gridItem);
}

LayoutUnit inlineAxisMaxContentContribution(const ElementBox& gridItem, const IntegrationUtils& integrationUtils)
{
    return integrationUtils.preferredMaxWidth(gridItem);
}

GridItemSizingFunctions inlineAxisGridItemSizingFunctions()
{
    return { inlineAxisMinContentContribution, inlineAxisMaxContentContribution };
}

LayoutUnit blockAxisMinContentContribution(const ElementBox&, const IntegrationUtils&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

LayoutUnit blockAxisMaxContentContribution(const ElementBox&, const IntegrationUtils&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

GridItemSizingFunctions blockAxisGridItemSizingFunctions()
{
    return { blockAxisMinContentContribution, blockAxisMaxContentContribution };
}

bool preferredSizeBehavesAsAuto(const Style::PreferredSize& preferredSize)
{
    return WTF::switchOn(preferredSize,
        [](const CSS::Keyword::Auto&) {
            return true;
        },
        [](const auto&) {
            ASSERT_NOT_IMPLEMENTED_YET();
            return false;
    });
}

bool preferredSizeDependsOnContainingBlockSize(const Style::PreferredSize&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return false;
}

}
}
}
