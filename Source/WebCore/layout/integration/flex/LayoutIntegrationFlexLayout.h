/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <WebCore/FlexFormattingContext.h>
#include <WebCore/FlexIntegrationUtils.h>
#include <WebCore/FlexItemContentCache.h>
#include <WebCore/FlexLayoutState.h>
#include <wtf/CheckedRef.h>
#include <wtf/SetForScope.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class RenderFlexibleBox;

namespace LayoutIntegration {

class FlexLayout {
public:
    FlexLayout(RenderFlexibleBox&);

    void layout(RelayoutChildren);

    // The container's in-flow children in order-modified document order, rebuilt every layout. Weak pointers because
    // painting/hit-testing/baseline queries read the list after layout, when a child may have been removed.
    using FlexItemList = Vector<SingleThreadWeakPtr<RenderBox>>;

    void paint(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction);

    // Widens the container's allowed layout overflow by how far content-alignment pushed the items past the
    // content-box start edges. Takes RenderBox's writing-mode-only allowance, which only the renderer can compute.
    LayoutOptionalOutsets adjustAllowedLayoutOverflow(LayoutOptionalOutsets) const;

    std::optional<LayoutUnit> firstLineBaseline() const;
    std::optional<LayoutUnit> lastLineBaseline() const;

    // Sets the static position of an out-of-flow flex item; returns true if it changed.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    // How far the flex algorithm has got, engaged for its duration only. Absent outside of it, which is how
    // RenderFlexibleBox tells that the flex algorithm is not the one asking.
    std::optional<LayoutPhase> layoutPhase() const { return m_flexLayoutState ? std::make_optional(m_flexLayoutState->phase()) : std::nullopt; }

    // Whether the item's height is definite by the phase the flex algorithm is in (9.8: the used sizes become
    // definite as the steps that produce them run). Absent when the algorithm is not running -- only
    // RenderFlexibleBox knows which of its own layout passes is then in play.
    std::optional<bool> isFlexItemHeightDefiniteInLayoutPhase(const RenderBox& flexItem) const;

    // CSS Flexbox 9.8: whether the item's post-flexing size is definite, which is what makes a percentage
    // resolved against it meaningful.
    bool hasDefiniteSizeForPercentResolution(const RenderBox& flexItem);

    void setFlexItemContentLogicalHeightFromLayout(const RenderBox& flexItem, LayoutUnit);
    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);
    void flexItemWillBeRemoved(const RenderBox& flexItem);

private:
    void buildFlexItemList();
    FlexLayoutItems buildFlexLayoutItems(RelayoutChildren, const FlexLayoutConstraints&);
    FlexLayoutConstraints flexLayoutConstraints() const;
    LayoutUnit mainAxisAvailableSpace() const;
    void prepareOutOfFlowBoxForPositionedLayout(RenderBox&);
    CheckedPtr<const RenderBox> flexItemForFirstBaseline() const;
    CheckedPtr<const RenderBox> flexItemForLastBaseline() const;
    CheckedPtr<const RenderBox> baselineFlexItemInLine(size_t lineStart, size_t itemCount, bool reverse) const;
    LayoutUnit staticMainAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticCrossAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticInlinePositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticBlockPositionForPositionedFlexItem(const RenderBox&);

    RenderFlexibleBox& flexBox() const LIFETIME_BOUND { return m_flexBox; }

    const CheckedRef<RenderFlexibleBox> m_flexBox;
    FlexItemList m_flexItems;
    // What the last run of the flex algorithm produced. Absent until it has run: the container can be painted,
    // hit-tested and queried for baselines before its first layout.
    std::optional<FlexFormattingContext::Result> m_flexLayoutResult;
    std::optional<FlexLayoutState> m_flexLayoutState;
    FlexItemContentCache m_flexItemContentCache;
};

}
}
