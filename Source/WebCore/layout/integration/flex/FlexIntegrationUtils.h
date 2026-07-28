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

#include <WebCore/FlexItemContentCache.h>
#include <WebCore/LayoutUnit.h>
#include <wtf/CheckedRef.h>
#include <wtf/SetForScope.h>

namespace WebCore {

enum class LogicalBoxAxis : uint8_t;

namespace Style {
enum class MarginTrimSide : uint8_t;
struct FlexBasis;
struct MaximumSize;
struct MinimumSize;
struct PreferredSize;
}

struct FlexContainerUsedExtents;

class FlexLayoutItem;
class FlexLayoutState;
class LayoutPoint;
class RenderBox;
class RenderFlexibleBox;

namespace LayoutIntegration {

class FlexIntegrationUtils {
public:
    FlexIntegrationUtils(RenderFlexibleBox&, FlexLayoutState&, FlexItemContentCache&);

    void applyStretchedLogicalHeightToFlexItem(const FlexLayoutItem&, LayoutUnit blockSize);
    void layoutFlexItemForStretchedCrossSize(const FlexLayoutItem&, LayoutUnit crossSize, LogicalBoxAxis crossAxis);
    void layoutFlexItemWithMainSize(FlexLayoutItem&, LayoutUnit mainSize);
    FlexContainerUsedExtents updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent);
    void setFlexItemGeometry(const FlexLayoutItem&, const LayoutPoint& location, bool isHorizontalFlow);
    void updateAutoMarginsInMainAxis(const FlexLayoutItem&, LayoutUnit autoMarginOffset);
    bool updateAutoMarginsInCrossAxis(const FlexLayoutItem&, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace);
    void setFlexItemOverridingBorderBoxLogicalHeight(const FlexLayoutItem&, LayoutUnit);

    void trimMainAxisMarginStart(FlexLayoutItem&);
    void trimMainAxisMarginEnd(FlexLayoutItem&);
    void trimCrossAxisMarginStart(const FlexLayoutItem&);
    void trimCrossAxisMarginEnd(const FlexLayoutItem&);
    LayoutUnit adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const;

    bool flexItemHasPercentHeightDescendants(const RenderBox&) const;
    bool flexItemHasPercentHeightDescendants(const FlexLayoutItem&) const;

    LayoutUnit flexItemContentLogicalHeight(const FlexLayoutItem&) const;
    LayoutUnit computeBlockAxisContentSizeForFlexItem(const FlexLayoutItem&);

    template<typename SizeType> bool flexItemMainSizeIsDefinite(const FlexLayoutItem&, const SizeType&);
    template<typename SizeType> std::optional<LayoutUnit> computeMainAxisExtentForFlexItem(const FlexLayoutItem&, const SizeType&, LayoutUnit mainAxisSizeForLengthResolution);
    LayoutUnit maxContentMainAxisExtentForFlexItem(const FlexLayoutItem&);
    LayoutUnit minContentMainAxisContributionForFlexItem(const FlexLayoutItem&);
    LayoutUnit flexItemIntrinsicLogicalHeight(const FlexLayoutItem&, bool needToStretchLogicalHeight) const;
    LayoutUnit flexItemIntrinsicLogicalWidth(const FlexLayoutItem&, bool crossSizeIsDefinite);
    LayoutUnit constrainFlexItemLogicalHeightByMinMax(const FlexLayoutItem&, LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const;
    LayoutUnit constrainFlexItemLogicalWidthByMinMax(const FlexLayoutItem&, LayoutUnit logicalWidth, LayoutUnit availableWidth) const;
    template<typename SizeType> std::optional<LayoutUnit> computePercentageLogicalHeightForFlexItem(const FlexLayoutItem&, const SizeType&) const;
    template<typename SizeType> std::optional<LayoutUnit> computeLogicalHeightUsingForFlexItem(const FlexLayoutItem&, const SizeType&) const;
    template<typename SizeType> LayoutUnit computeLogicalWidthUsingForFlexItem(const FlexLayoutItem&, const SizeType&, LayoutUnit availableWidth) const;

private:
    RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }
    FlexLayoutState& flexLayoutState() const LIFETIME_BOUND;

    void setTrimmedMarginForChild(const FlexLayoutItem&, Style::MarginTrimSide);
    void invalidateFlexItemContentLogicalWidthsIfNeeded(const FlexLayoutItem&);
    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem);
    void dirtyPercentHeightDescendantsWithinFlexItem(RenderBox& flexItem);

    const CheckedRef<RenderFlexibleBox> m_flexBox;
    FlexLayoutState& m_flexLayoutState;
    FlexItemContentCache& m_flexItemContentCache;
};

// RAII that temporarily overrides a flex item's main-axis border-box size to its flex basis for the duration of a
// flex-base-size measurement, restoring it on destruction. Lives here because it mutates the render tree.
class ScopedFlexBasisAsFlexItemMainSize {
public:
    ScopedFlexBasisAsFlexItemMainSize(const FlexLayoutItem&, Style::PreferredSize&&);
    ~ScopedFlexBasisAsFlexItemMainSize();

private:
    const CheckedRef<RenderBox> m_flexItem;
    bool m_mainAxisIsInlineAxis { false };
    bool m_didOverride { false };
};

// RAII for measuring a flex item before it is stretched. When the item is going to be stretched to a definite cross
// size (flexbox 9.8 rule 1), that size is set as its cross-axis overriding size so the measurement sees the item's
// final cross size; otherwise the item has no definite cross size, so both of its overriding sizes are cleared. The
// previous overriding sizes are restored on destruction. With InvalidateContentWidths::Yes the item's preferred widths
// are invalidated so they recompute against that cross size.
class FlexItemDefiniteCrossSizeScope {
public:
    enum class InvalidateContentWidths : bool { No, Yes };
    FlexItemDefiniteCrossSizeScope(RenderBox& flexItem, InvalidateContentWidths);
    ~FlexItemDefiniteCrossSizeScope();

private:
    const CheckedRef<RenderBox> m_flexItem;
    std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalWidth;
    std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalHeight;
    bool m_shouldRestoreInlineSize { false };
    bool m_shouldRestoreBlockSize { false };
#if ASSERT_ENABLED
    bool m_didInvalidateContentLogicalWidths { false };
#endif
};

// RAII that marks the flex container as measuring this item's intrinsic width (by laying its content out), so the
// item's percentage-height content resolves against its overriding definite cross size. Consumed by
// RenderFlexibleBox::canUseFlexItemForPercentageResolution. The flex container is derived from the flex item's parent.
class FlexItemIntrinsicWidthComputationScope {
public:
    explicit FlexItemIntrinsicWidthComputationScope(RenderBox& flexItem);

private:
    SetForScope<bool> m_intrinsicWidthComputation;
};

} // namespace LayoutIntegration
} // namespace WebCore
