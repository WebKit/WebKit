/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/BlockLayoutState.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/RenderBlockFlow.h>

#include <optional>
#include <wtf/CheckedRef.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class InlineLayoutState;
class LayoutState;

// Border plus padding of a grid item along the grid container's axes, whatever the item's own writing mode.
struct GridItemBorderAndPadding {
    LayoutUnit inlineAxis;
    LayoutUnit blockAxis;
};

// The overriding border-box size to lay a grid item out at, along the grid container's axes. An absent block size lets the item size itself.
struct GridItemOverridingBorderBoxSize {
    LayoutUnit inlineAxis;
    std::optional<LayoutUnit> blockAxis;
};

enum class GridItemIntrinsicSize : uint8_t {
    MinContent,
    MaxContent,
    MinContentContribution,
    MaxContentContribution
};

class IntegrationUtils {
public:
    IntegrationUtils(const LayoutState&);

    void layoutWithFormattingContextForBox(const ElementBox&, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth = { }, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight = { }) const;
    GridItemBorderAndPadding borderAndPaddingForGridItem(const ElementBox&, std::optional<LayoutUnit> gridAreaInlineSize) const;
    void layoutGridItem(const ElementBox&, GridItemOverridingBorderBoxSize, LayoutUnit gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize) const;
    // The grid item's laid-out border-box size along the grid container's `axis`. Call it after the item's layout.
    LayoutUnit borderBoxSizeForGridItem(const ElementBox&, LogicalBoxAxis) const;
    // Border-box sizes along the grid container's inline / block axis. The stretched block size is the container-block size the item is stretched to; it only
    // applies to inline contributions, where it can be transferred through the item's preferred aspect ratio.
    LayoutUnit inlineIntrinsicSizeForGridItem(const ElementBox&, GridItemIntrinsicSize, std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize, std::optional<LayoutUnit> stretchedBlockSize) const;
    LayoutUnit blockIntrinsicSizeForGridItem(const ElementBox&, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit maxContentWidth(const ElementBox&) const;
    LayoutUnit minContentWidth(const ElementBox&) const;
    LayoutUnit minContentHeight(const ElementBox&) const;
    LayoutUnit minContentLogicalWidthContribution(const ElementBox&) const;
    LayoutUnit maxContentLogicalWidthContribution(const ElementBox&) const;
    std::optional<LayoutUnit> firstLineBaselineForGridItem(const ElementBox&) const;
    std::optional<LayoutUnit> lastLineBaselineForGridItem(const ElementBox&) const;
    void layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLineLogicalTopLeft, const InlineLayoutState&) const;
    NODELETE static const Style::ComputedStyle& firstNonAnonymousAncestorStyle(const Box&);

    NODELETE static BlockLayoutState::MarginState toMarginState(const RenderBlockFlow::MarginInfo&);
    static RenderBlockFlow::MarginInfo toMarginInfo(const Layout::BlockLayoutState::MarginState&);
    NODELETE static std::pair<LayoutRect, LayoutRect> toMarginAndBorderBoxVisualRect(const BoxGeometry& logicalGeometry, const LayoutSize& containerSize, WritingMode);

private:
    const CheckedRef<const LayoutState> m_globalLayoutState;
};

}
}

