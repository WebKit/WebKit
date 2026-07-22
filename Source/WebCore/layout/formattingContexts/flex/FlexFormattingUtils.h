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
enum class Overflow : uint8_t;
enum class OverflowAlignment : uint8_t;
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
class StyleContentAlignmentData;

// Stateless geometry and query helpers for the flex formatting context (mirrors the render-side
// FlexLayoutUtils). These read the flex container and its items through the container reference;
// RenderFlexibleBox befriends this class so the helpers can reach the container's flow-direction
// queries. Shared helpers are duplicated from FlexLayoutUtils so the formatting context is self-contained.
class FlexFormattingUtils {
public:
    FlexFormattingUtils(const RenderFlexibleBox&);

    LayoutUnit flowAwareBorderStart() const;
    LayoutUnit flowAwareBorderEnd() const;
    LayoutUnit flowAwareBorderBefore() const;
    LayoutUnit flowAwareBorderAfter() const;
    LayoutUnit flowAwarePaddingStart() const;
    LayoutUnit flowAwarePaddingEnd() const;
    LayoutUnit flowAwarePaddingBefore() const;
    LayoutUnit flowAwarePaddingAfter() const;

    LayoutUnit flowAwareMarginStartForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit flowAwareMarginEndForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit flowAwareMarginBeforeForFlexItem(const RenderBox& flexItem) const;

    LayoutUnit crossAxisExtentForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit mainAxisExtentForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit mainAxisExtent() const;
    static LayoutUnit crossAxisContentExtent(const RenderFlexibleBox&);

    enum class GapType : uint8_t { BetweenLines, BetweenItems };
    static LayoutUnit computeGap(const RenderFlexibleBox&, GapType);
    LayoutUnit computeGap(GapType) const;

    LayoutUnit mainAxisMarginExtentForFlexItem(const RenderBox& flexItem) const;
    static LayoutUnit crossAxisMarginExtentForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    LayoutUnit crossAxisMarginExtentForFlexItem(const RenderBox& flexItem) const;

    LayoutUnit crossAxisScrollbarExtent() const;
    LayoutUnit mainAxisScrollbarExtent() const;

    static const Style::PreferredSize& preferredMainSizeLengthForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem LIFETIME_BOUND);
    const Style::PreferredSize& preferredMainSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;
    static const Style::MinimumSize& minMainSizeLengthForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem LIFETIME_BOUND);
    const Style::MinimumSize& minMainSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;
    const Style::MaximumSize& maxMainSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;
    static const Style::PreferredSize& preferredCrossSizeLengthForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem LIFETIME_BOUND);
    const Style::PreferredSize& preferredCrossSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;
    const Style::MinimumSize& minCrossSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;
    const Style::MaximumSize& maxCrossSizeLengthForFlexItem(const RenderBox& flexItem) const LIFETIME_BOUND;

    static Overflow mainAxisOverflowForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    OverflowAlignment overflowAlignmentForFlexItem(const RenderBox& flexItem) const;
    static bool hasAutoMarginsInCrossAxis(const RenderFlexibleBox&, const RenderBox& flexItem);
    bool hasAutoMarginsInCrossAxis(const RenderBox& flexItem) const;
    static bool useContentBasedMinimumSize(const RenderFlexibleBox&, const RenderBox& flexItem);
    bool useContentBasedMinimumSize(const RenderBox& flexItem) const;
    double preferredAspectRatioForFlexItem(const RenderBox& flexItem) const;

    static bool flexItemHasAspectRatio(const RenderBox& flexItem);
    static bool canResolveFullyConstrainedLogicalHeight(const RenderFlexibleBox&);
    bool flexItemHasComputableAspectRatio(const RenderBox& flexItem) const;
    bool needToStretchFlexItemLogicalHeight(const RenderBox& flexItem) const;
    static LayoutUnit innerCrossSizeForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    LayoutUnit innerCrossSizeForFlexItem(const RenderBox& flexItem) const;
    LayoutUnit columnInnerMainSize(LayoutUnit hypotheticalMainSize) const;
    LayoutUnit availableAlignmentSpaceForFlexItem(LayoutUnit lineCrossAxisExtent, const RenderBox& flexItem, LayoutUnit crossSize) const;
    LayoutUnit marginBoxAscentForFlexItem(const RenderBox& flexItem, LayoutUnit crossSize) const;

    FlowDirection crossAxisDirection() const;
    FlowDirection transformedBlockFlowDirection() const;
    static bool NODELETE isHorizontalFlow(const RenderFlexibleBox&);
    static bool NODELETE isColumnFlow(const RenderFlexibleBox&);
    bool isColumnOrRowReverse() const;
    static bool isWrapReverse(const RenderFlexibleBox&);
    static bool isMultiline(const RenderFlexibleBox&);
    bool isLeftToRightFlow() const;
    static bool mainAxisIsFlexItemInlineAxis(const RenderFlexibleBox&, const RenderBox& flexItem);
    bool mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem) const;
    static Style::FlexBasis flexBasisForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    Style::FlexBasis flexBasisForFlexItem(const RenderBox& flexItem) const;
    static ItemPosition alignmentForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    ItemPosition alignmentForFlexItem(const RenderBox& flexItem) const;
    static bool hasDefiniteCrossSizeForFlexItem(const RenderFlexibleBox&, const RenderBox& flexItem);
    bool hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem) const;
    static bool hasDefiniteLogicalWidthForAspectRatioCrossSize(const RenderFlexibleBox&);
    static std::optional<TextDirection> leftRightAxisDirectionFromStyle(const Style::ComputedStyle&);

    bool shouldTrimMainAxisMarginStart() const;
    bool shouldTrimMainAxisMarginEnd() const;
    bool shouldTrimCrossAxisMarginStart() const;
    bool shouldTrimCrossAxisMarginEnd() const;

    static const StyleContentAlignmentData& contentAlignmentNormalBehavior();
    static ContentPosition resolveLeftRightAlignment(ContentPosition, const StyleContentAlignmentData&, const Style::ComputedStyle&, bool isReversed);
    static LayoutUnit initialJustifyContentOffset(const Style::ComputedStyle&, LayoutUnit availableFreeSpace, unsigned numberOfFlexItems, bool isReversed);
    static LayoutUnit justifyContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution, unsigned numberOfFlexItems);
    static LayoutUnit alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition, std::optional<LayoutUnit> ascent, std::optional<LayoutUnit> maxAscent, bool isWrapReverse);
    static LayoutUnit contentAlignmentStartOverflow(LayoutUnit availableFreeSpace, ContentPosition, ContentDistribution, OverflowAlignment safety, bool isReverse);
    static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition, ContentDistribution, OverflowAlignment safety, unsigned numberOfLines, bool isReversed);
    static LayoutUnit alignContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution, unsigned numberOfLines);

private:
    const RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }

    const CheckedRef<const RenderFlexibleBox> m_flexBox;
};

} // namespace WebCore
