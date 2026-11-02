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

class BlockFormattingContext;
class BoxGeometry;
class ElementBox;
class InlineLayoutState;
class LayoutState;
struct BlockLayoutResult;

class IntegrationUtils {
public:
    IntegrationUtils(LayoutState&);
    // The block formatting context the inline content is in lays out the block-level boxes in it, and adds them to the result it is building.
    IntegrationUtils(LayoutState&, BlockFormattingContext&, BlockLayoutResult&);

    void layoutWithFormattingContextForBox(const ElementBox&, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth = { }, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight = { }) const;
    std::pair<LayoutUnit, LayoutUnit> borderAndPaddingForGridItem(const ElementBox&, std::optional<LayoutUnit> gridAreaInlineSize) const;
    void layoutGridItem(const ElementBox&, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit maxContentWidth(const ElementBox&) const;
    LayoutUnit maxContentWidthForGridItem(const ElementBox&, std::optional<LayoutUnit> gridAreaInlineSize) const;
    LayoutUnit minContentWidth(const ElementBox&) const;
    LayoutUnit minContentHeight(const ElementBox&) const;
    LayoutUnit minContentWidthForGridItem(const ElementBox&, std::optional<LayoutUnit> gridAreaInlineSize) const;
    LayoutUnit minContentHeightForGridItem(const ElementBox&, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit maxContentHeightForGridItem(const ElementBox&, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit minContentContributionHeightForGridItem(const ElementBox&, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit maxContentContributionHeightForGridItem(const ElementBox&, LayoutUnit gridAreaInlineSize) const;
    LayoutUnit minContentLogicalWidthContribution(const ElementBox&) const;
    LayoutUnit maxContentLogicalWidthContribution(const ElementBox&) const;
    std::optional<LayoutUnit> firstLineBaselineForGridItem(const ElementBox&) const;
    std::optional<LayoutUnit> lastLineBaselineForGridItem(const ElementBox&) const;
    void layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLineLogicalTopLeft, InlineLayoutState&) const;
    static const Style::ComputedStyle& LEGACY_NODELETE firstNonAnonymousAncestorStyle(const Box&);

    // What of RenderBlockFlow::MarginInfo crosses into the inline content; the rest stays with the block container (MarginInfo(const RenderBlockFlow&)).
    static BlockLayoutState::MarginState LEGACY_NODELETE toMarginState(const RenderBlockFlow::MarginInfo&);
    static void updateMarginInfo(RenderBlockFlow::MarginInfo&, const BlockLayoutState::MarginState&);
    static std::pair<LayoutRect, LayoutRect> LEGACY_NODELETE toMarginAndBorderBoxVisualRect(const BoxGeometry& logicalGeometry, const LayoutSize& containerSize, WritingMode);

private:
    const CheckedRef<LayoutState> m_globalLayoutState;
    // FIXME: The block formatting context lays out the block-level boxes itself once it splits the inline content at them.
    BlockFormattingContext* const m_blockFormattingContext { nullptr };
    BlockLayoutResult* const m_blockLayoutResult { nullptr };
};

}
}

