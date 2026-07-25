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
    FlexIntegrationUtils(RenderFlexibleBox&);

    RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }
    FlexLayoutState& flexLayoutState() const;

    void applyStretchedLogicalHeightToFlexItem(const FlexLayoutItem&, LayoutUnit blockSize);
    void layoutFlexItemForStretchedCrossSize(const FlexLayoutItem&, LayoutUnit crossSize, LogicalBoxAxis crossAxis);
    void layoutFlexItemWithMainSize(FlexLayoutItem&, LayoutUnit mainSize);
    FlexContainerUsedExtents updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent);
    void setFlexItemGeometry(const FlexLayoutItem&, const LayoutPoint& location, bool isHorizontalFlow);
    void setFlexItemOverridingBorderBoxLogicalHeight(const FlexLayoutItem&, LayoutUnit);
    void invalidateFlexItemContentLogicalWidthsIfNeeded(const FlexLayoutItem&);

    void setTrimmedMarginForChild(const FlexLayoutItem&, Style::MarginTrimSide);
    LayoutUnit adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const;

    void addItemAtFlexLineStart(const FlexLayoutItem&);
    void addItemAtFlexLineEnd(const FlexLayoutItem&);
    void addItemOnFirstFlexLine(const FlexLayoutItem&);
    void addItemOnLastFlexLine(const FlexLayoutItem&);
    bool flexItemHasPercentHeightDescendants(const FlexLayoutItem&) const;

    LayoutUnit flexItemContentLogicalHeight(const FlexLayoutItem&) const;
    LayoutUnit computeBlockAxisContentSizeForFlexItem(const FlexLayoutItem&);
    template<typename SizeType> bool flexItemMainSizeIsDefinite(const FlexLayoutItem&, const SizeType&);
    template<typename SizeType> std::optional<LayoutUnit> computeMainAxisExtentForFlexItem(const FlexLayoutItem&, const SizeType&, LayoutUnit mainAxisSizeForLengthResolution);
    template<typename SizeType> std::optional<LayoutUnit> computeMainAxisExtentForFlexItemWithCrossAxisOverride(const FlexLayoutItem&, const SizeType&, LayoutUnit mainAxisSizeForLengthResolution);
    LayoutUnit maxContentMainAxisContributionForFlexItem(const FlexLayoutItem&);
    LayoutUnit minContentMainAxisContributionForFlexItem(const FlexLayoutItem&);

private:
    const CheckedRef<RenderFlexibleBox> m_flexBox;
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

// RAII that defines a scope in which a box's overriding sizes are either replaced (in one axis, when a size is
// given) or cleared (both axes, when nullopt), restoring the previous overriding sizes on destruction.
class OverridingSizesScope {
public:
    enum class Axis { Inline, Block, Both };

    OverridingSizesScope(RenderBox&, Axis, std::optional<LayoutUnit> size = std::nullopt);
    ~OverridingSizesScope();

private:
    const CheckedRef<RenderBox> m_box;
    Axis m_axis;
    std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalWidth;
    std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalHeight;
};

// RAII that marks the flex container as measuring a flex item's intrinsic main-axis size and, while doing so, applies
// the container's definite cross size as the item's cross-axis override (clearing all overrides otherwise). With
// InvalidateContentWidths::Yes the item's preferred widths are invalidated so min/maxContentLogicalWidthContribution()
// recompute with the override in place. The flex container is derived from the flex item's parent.
class ScopedCrossAxisOverrideForFlexItem {
public:
    enum class InvalidateContentWidths : bool { No, Yes };
    ScopedCrossAxisOverrideForFlexItem(RenderBox& flexItem, InvalidateContentWidths);
    ~ScopedCrossAxisOverrideForFlexItem();

private:
    SetForScope<bool> m_intrinsicWidthComputation;
    std::optional<OverridingSizesScope> m_overridingScope;
#if ASSERT_ENABLED
    RenderBox& m_flexItem;
    bool m_didInvalidateContentLogicalWidths { false };
#endif
};

} // namespace LayoutIntegration
} // namespace WebCore
