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
    const FlexItemList& flexItems() const LIFETIME_BOUND { return m_flexItems; }

    void paint(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
    bool hitTest(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction);

    // Widens the container's allowed layout overflow by how far content-alignment pushed the items past the
    // content-box start edges. Takes RenderBox's writing-mode-only allowance, which only the renderer can compute.
    LayoutOptionalOutsets adjustAllowedLayoutOverflow(LayoutOptionalOutsets) const;

    std::optional<LayoutUnit> firstLineBaseline() const;
    std::optional<LayoutUnit> lastLineBaseline() const;

    // Sets the static position of an out-of-flow flex item; returns true if it changed.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    // The state of the flex algorithm, engaged for its duration only. Absent outside of it, which is how
    // RenderFlexibleBox tells that the flex algorithm is not the one asking.
    bool isInLayout() const { return !!m_flexLayoutState; }
    std::optional<LayoutPhase> layoutPhase() const { return m_flexLayoutState ? std::make_optional(m_flexLayoutState->phase()) : std::nullopt; }

    bool isFlexBoxBlockSizeDefinite() const { return m_flexLayoutState && m_flexLayoutState->isFlexBoxBlockSizeDefinite(); }
    bool isFlexBoxBlockSizeIndefinite() const { return m_flexLayoutState && m_flexLayoutState->isFlexBoxBlockSizeIndefinite(); }
    void setFlexBoxBlockSizeIsDefinite(bool isDefinite) { ASSERT(m_flexLayoutState); m_flexLayoutState->setFlexBoxBlockSizeIsDefinite(isDefinite); }

    // Which items had a margin trimmed, so RenderFlexibleBox::isChildEligibleForMarginTrim can answer for a given
    // side. Seeded before layout (the first/last item's inline margins affect the container's intrinsic widths) and
    // filled in by the flex algorithm as it trims.
    void initializeMarginTrimState();
    bool isFlexItemEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox& flexItem) const;
    void addItemAtFlexLineStart(const RenderBox& flexItem) { m_marginTrimItems.itemsAtFlexLineStart.add(flexItem); }
    void addItemAtFlexLineEnd(const RenderBox& flexItem) { m_marginTrimItems.itemsAtFlexLineEnd.add(flexItem); }
    void addItemOnFirstFlexLine(const RenderBox& flexItem) { m_marginTrimItems.itemsOnFirstFlexLine.add(flexItem); }
    void addItemOnLastFlexLine(const RenderBox& flexItem) { m_marginTrimItems.itemsOnLastFlexLine.add(flexItem); }

    void setFlexItemContentLogicalHeightFromLayout(const RenderBox& flexItem, LayoutUnit);
    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);
    void flexItemWillBeRemoved(const RenderBox& flexItem);

private:
    void prepareFlexItemsAndMargins();
    FlexLayoutItems collectFlexItems(RelayoutChildren, const FlexLayoutConstraints&);
    FlexLayoutConstraints flexLayoutConstraints() const;
    LayoutUnit mainAxisAvailableSpace() const;
    void prepareFlexItemForPositionedLayout(RenderBox&);
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
    struct MarginTrimItems {
        SingleThreadWeakHashSet<const RenderBox> itemsAtFlexLineStart;
        SingleThreadWeakHashSet<const RenderBox> itemsAtFlexLineEnd;
        SingleThreadWeakHashSet<const RenderBox> itemsOnFirstFlexLine;
        SingleThreadWeakHashSet<const RenderBox> itemsOnLastFlexLine;
    } m_marginTrimItems;
    // How far content-alignment pushed the items past the container's content-box start edges, in the flex
    // container's own axes. Set by the flex algorithm, read back by adjustAllowedLayoutOverflow.
    LayoutUnit m_alignContentStartOverflow;
    LayoutUnit m_justifyContentStartOverflow;
    std::optional<FlexLayoutState> m_flexLayoutState;
    FlexItemContentCache m_flexItemContentCache;

    size_t m_numberOfFlexItemsOnFirstLine { 0 };
    size_t m_numberOfFlexItemsOnLastLine { 0 };
};

}
}
