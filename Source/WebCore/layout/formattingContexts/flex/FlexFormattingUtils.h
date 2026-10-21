/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutUnit.h>
#include <optional>
#include <wtf/CheckedRef.h>

namespace WebCore {

enum class ContentDistribution : uint8_t;
enum class ContentPosition : uint8_t;
enum class FlowDirection : uint8_t;
enum class ItemPosition : uint8_t;
enum class LogicalBoxAxis : uint8_t;
enum class Overflow : uint8_t;
enum class OverflowAlignment : uint8_t;
enum class StretchingMode;
enum class TextDirection : bool;

namespace Style {
struct FlexBasis;
struct PreferredSize;
struct MinimumSize;
struct MaximumSize;
class ComputedStyle;
}

class RenderBox;
class RenderFlexibleBox;
class FlexLayoutItem;
class StyleContentAlignmentData;

// Stateless geometry and query helpers for the flex formatting context.
class FlexFormattingUtils {
public:
    FlexFormattingUtils(const RenderFlexibleBox&);

    LayoutUnit flowAwareBorderAndPaddingStart() const;
    LayoutUnit flowAwareBorderAndPaddingEnd() const;
    LayoutUnit flowAwareBorderAndPaddingBefore() const;

    LayoutUnit flowAwareMarginStartForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit flowAwareMarginEndForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit flowAwareMarginBeforeForFlexItem(const FlexLayoutItem&) const;

    LayoutUnit LEGACY_NODELETE crossAxisExtentForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit LEGACY_NODELETE crossAxisExtentForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit LEGACY_NODELETE mainAxisExtentForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit LEGACY_NODELETE mainAxisExtentForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit LEGACY_NODELETE mainAxisExtent() const;
    static LayoutUnit crossAxisContentExtent(const RenderFlexibleBox&);

    enum class GapType : uint8_t { BetweenLines, BetweenItems };
    static LayoutUnit computeGap(const RenderFlexibleBox&, GapType);
    LayoutUnit computeGap(GapType) const;

    LayoutUnit resolveMainAxisMarginExtentForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit usedMainAxisMarginExtentForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit crossAxisMarginExtentForFlexItem(const FlexLayoutItem&) const;

    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutUnit mainAxisScrollbarExtent() const;

    const Style::PreferredSize& LEGACY_NODELETE preferredMainSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;
    const Style::MinimumSize& LEGACY_NODELETE minMainSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;
    const Style::MaximumSize& LEGACY_NODELETE maxMainSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;
    const Style::PreferredSize& LEGACY_NODELETE preferredCrossSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;
    const Style::MinimumSize& LEGACY_NODELETE minCrossSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;
    const Style::MaximumSize& LEGACY_NODELETE maxCrossSizeLengthForFlexItem(const FlexLayoutItem&) const LIFETIME_BOUND;

    static Overflow LEGACY_NODELETE mainAxisOverflowForFlexItem(const RenderBox& flexItem);
    OverflowAlignment overflowAlignmentForFlexItem(const RenderBox& flexItem) const;
    OverflowAlignment overflowAlignmentForFlexItem(const FlexLayoutItem&) const;
    static bool LEGACY_NODELETE hasAutoMarginsInCrossAxis(const RenderBox& flexItem);
    bool LEGACY_NODELETE hasAutoMarginsInCrossAxis(const FlexLayoutItem&) const;
    // Whether the container will stretch the item along containingAxis (RenderBlock::willStretchItem for flex).
    static bool willStretchFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem, LogicalBoxAxis containingAxis, StretchingMode);
    static bool hasStretchedFlexItemWithAspectRatio(const RenderFlexibleBox&);
    bool useContentBasedMinimumSize(const FlexLayoutItem&) const;
    static bool useContentBasedMinimumBlockSize(const RenderBox& flexItem);
    double preferredAspectRatioForFlexItem(const FlexLayoutItem&) const;

    static bool flexItemHasAspectRatio(const RenderBox& flexItem);
    static bool flexItemHasAspectRatio(const FlexLayoutItem&);
    static bool canResolveFullyConstrainedLogicalHeight(const RenderFlexibleBox&);
    bool flexItemHasComputableAspectRatio(const FlexLayoutItem&) const;
    bool needToStretchFlexItemLogicalHeight(const FlexLayoutItem&) const;
    static LayoutUnit innerCrossSizeForFlexItem(const RenderBox& flexItem);
    LayoutUnit innerCrossSizeForFlexItem(const FlexLayoutItem&) const;
    LayoutUnit columnInnerMainSize(LayoutUnit hypotheticalMainSize) const;
    LayoutUnit availableAlignmentSpaceForFlexItem(LayoutUnit lineCrossAxisExtent, const RenderBox& flexItem, LayoutUnit crossSize) const;
    LayoutUnit availableAlignmentSpaceForFlexItem(LayoutUnit lineCrossAxisExtent, const FlexLayoutItem&, LayoutUnit crossSize) const;
    LayoutUnit marginBoxAscentForFlexItem(const FlexLayoutItem&, LayoutUnit crossSize) const;

    FlowDirection LEGACY_NODELETE crossAxisDirection() const;
    FlowDirection LEGACY_NODELETE transformedBlockFlowDirection() const;
    static bool LEGACY_NODELETE isHorizontalFlow(const RenderFlexibleBox&);
    static bool LEGACY_NODELETE isColumnFlow(const RenderFlexibleBox&);
    bool LEGACY_NODELETE isColumnOrRowReverse() const;
    static bool LEGACY_NODELETE isWrapReverse(const RenderFlexibleBox&);
    static bool LEGACY_NODELETE isMultiline(const RenderFlexibleBox&);
    static bool LEGACY_NODELETE isBalance(const RenderFlexibleBox&);
    static size_t LEGACY_NODELETE minimumLineCount(const RenderFlexibleBox&);
    bool LEGACY_NODELETE isLeftToRightFlow() const;
    static bool LEGACY_NODELETE mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem);
    static Style::FlexBasis flexBasisForFlexItem(const RenderBox& flexItem);
    Style::FlexBasis flexBasisForFlexItem(const FlexLayoutItem&) const;
    static ItemPosition alignmentForFlexItem(const RenderBox& flexItem);
    ItemPosition alignmentForFlexItem(const FlexLayoutItem&) const;
    static bool hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem);
    bool hasDefiniteCrossSizeForFlexItem(const FlexLayoutItem&) const;
    static bool LEGACY_NODELETE hasDefiniteLogicalWidthForAspectRatioCrossSize(const RenderFlexibleBox&);
    static std::optional<TextDirection> LEGACY_NODELETE leftRightAxisDirectionFromStyle(const Style::ComputedStyle&);

    static const StyleContentAlignmentData& LEGACY_NODELETE contentAlignmentNormalBehavior();
    static ContentPosition LEGACY_NODELETE resolveLeftRightAlignment(const StyleContentAlignmentData&, const Style::ComputedStyle&, bool isReversed);
    static LayoutUnit initialJustifyContentOffset(const Style::ComputedStyle&, LayoutUnit availableFreeSpace, unsigned numberOfFlexItems, bool isReversed);
    static LayoutUnit LEGACY_NODELETE justifyContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution, unsigned numberOfFlexItems);
    static LayoutUnit LEGACY_NODELETE alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition, std::optional<LayoutUnit> ascent, std::optional<LayoutUnit> maxAscent, bool isWrapReverse);
    static LayoutUnit LEGACY_NODELETE contentAlignmentStartOverflow(LayoutUnit availableFreeSpace, ContentPosition, ContentDistribution, OverflowAlignment safety, bool isReverse);
    static LayoutUnit LEGACY_NODELETE initialContentAlignmentOffset(LayoutUnit availableFreeSpace, ContentPosition, ContentDistribution, OverflowAlignment safety, unsigned numberOfAlignmentSubjects, bool isReversed);
    static LayoutUnit LEGACY_NODELETE alignContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution, unsigned numberOfLines);

private:
    static bool isStretchedFlexItem(const RenderBox& flexItem);
    static LayoutUnit crossAxisMarginExtentForFlexItem(const RenderBox& flexItem);
    static LayoutUnit innerCrossSizeForFlexItem(const RenderBox& flexItem, LayoutUnit crossAxisMarginExtent);
    LayoutUnit usedMainAxisMarginExtentForFlexItem(const RenderBox& flexItem) const;
    static const Style::PreferredSize& LEGACY_NODELETE preferredMainSizeLengthForFlexItem(const RenderBox& flexItem LIFETIME_BOUND);
    static const Style::MinimumSize& LEGACY_NODELETE minMainSizeLengthForFlexItem(const RenderBox& flexItem LIFETIME_BOUND);
    static const Style::PreferredSize& LEGACY_NODELETE preferredCrossSizeLengthForFlexItem(const RenderBox& flexItem LIFETIME_BOUND);
    static bool useContentBasedMinimumSize(const RenderBox& flexItem);

    const RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }

    const CheckedRef<const RenderFlexibleBox> m_flexBox;
};

} // namespace WebCore
