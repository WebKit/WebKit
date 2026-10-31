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

#include <WebCore/AdjoiningMargins.h>
#include <WebCore/BlockMarginCollapse.h>
#include <WebCore/Font.h>
#include <WebCore/LayoutBoxGeometry.h>
#include <WebCore/LayoutRange.h>
#include <WebCore/LayoutUnits.h>
#include <WebCore/StyleTextBoxEdge.h>
#include <algorithm>
#include <wtf/CheckedRef.h>
#include <wtf/EnumSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBox;

namespace Layout {

class BlockFormattingContext;

struct ContainingBlockState {
    CheckedRef<const RenderBox> blockContainer;
    // The geometry of the block container itself, completed once its content is laid out (not used for the root).
    BoxGeometry geometry;
    // The content box in the inline direction, from the inline start of the block container's border box (in its own direction).
    LayoutRange inlineRange;
    // Block offsets are relative to the border box of the root (every box in the block formatting context has the same block flow
    // direction). The top is unknown while the block-start margin can still collapse with the margins that follow it (CSS 2.2 §8.3.1).
    std::optional<LayoutUnit> borderBoxLogicalTop;
    LayoutUnit borderAndPaddingBefore;
    // Percentage block sizes of the children resolve against this, and behave as auto without it (CSS 2.2 §10.5).
    std::optional<LayoutUnit> contentBoxBlockSize;
    // -webkit-fill-available block sizes of the children resolve to this (RenderBox::availableLogicalHeight).
    LayoutUnit fillAvailableBlockSize;
    BlockMarginCollapse::BlockContainer margins;
    // Its own formatting context lays it out through the render tree and gives it its block size.
    bool isLaidOutThroughRenderTree { false };
    // The children laid out through the render tree are laid out again (RenderBlockFlow::layoutBlockChildren), like when its content box
    // inline size, border or padding changed since its last layout (RenderBlock::recomputeLogicalWidth).
    bool relayoutChildren { false };
    // The lines with content in its inline content up to the line of the last block-level box in it, that line included: the inline
    // formatting context's line count is larger when line boxes came after that box.
    size_t lineCountThroughLastBlockLevelBox { 0 };

    LayoutUnit contentBoxLogicalTop() const { return *borderBoxLogicalTop + borderAndPaddingBefore; }
};

// This class holds block level information shared across child inline formatting contexts, and what the block formatting context changes
// as it lays out its content.
class BlockLayoutState {
public:
    struct LineClamp {
        size_t maximumLines { 0 };
        bool shouldDiscardOverflow { false };
        bool isLegacy { true };
        bool isStableTextWrap { false };
    };
    enum class TextBoxTrimSide : bool {
        Start,
        End
    };
    using TextBoxTrim = EnumSet<TextBoxTrimSide>;

    struct LineGrid {
        LayoutSize layoutOffset;
        LayoutSize gridOffset;
        InlineLayoutUnit columnWidth;
        LayoutUnit rowHeight;
        LayoutUnit topRowOffset;
        Ref<const Font> primaryFont;
        std::optional<LayoutSize> paginationOrigin;
        LayoutUnit pageLogicalTop;
    };

    // The margins coming into the inline content of one block container, which its inline formatting context carries across the lines and
    // the block-level boxes of the content (InlineLayoutState::marginState), and which the parent reads back once the content is laid out.
    struct MarginState {
        // While the margins collapse with the block container's block-start margin, they do not move the lines.
        bool marginsCollapseWithBlockContainerTop() const { return atBlockContainerStart && blockContainerMarginBeforeCollapsesWithContent; }
        // What the margins move the next line down by.
        LayoutUnit marginToPlace() const { return adjoiningMargins.collapsedValue() - marginBeforeIncludedInClearance; }
        void resetAdjoiningMargins()
        {
            adjoiningMargins = { };
            marginBeforeIncludedInClearance = { };
        }

        // The margins nothing has separated yet (CSS 2.2 §8.3.1).
        AdjoiningMargins adjoiningMargins;
        // The block-start margin of a box whose margins collapse through it and which has clearance: the content after the box starts past it
        // (CSS 2.2 §9.5.2), so it is not left to place.
        LayoutUnit marginBeforeIncludedInClearance;
        // Nothing has separated the margins from the block-start edge of the block container yet.
        bool atBlockContainerStart : 1 { true };
        // The block container's block-start margin collapses with the margins of its content (it has no block-start border or padding and
        // establishes no block formatting context).
        bool blockContainerMarginBeforeCollapsesWithContent : 1 { false };
        // The block container's block-end margin collapses with the margins after its content, which ends with a block-level box.
        bool blockContainerMarginAfterCollapsesWithContent : 1 { false };
        // FIXME: The quirks of the render tree path, which the block-level boxes in the content change (RenderBlockFlow::MarginInfo).
        bool marginBeforeIsQuirky : 1 { false };
        bool marginAfterIsQuirky : 1 { false };
        bool marginBeforeQuirkIsDetermined : 1 { false };
    };

    BlockLayoutState(MarginState, std::optional<LineClamp> = { }, TextBoxTrim = { }, Style::TextBoxEdge = CSS::Keyword::Auto { }, std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom = { }, std::optional<LineGrid> lineGrid = { });

    std::optional<LineClamp> lineClamp() const { return m_lineClamp; }
    TextBoxTrim textBoxTrim() const { return m_textBoxTrim; }
    Style::TextBoxEdge textBoxEdge() const { return m_textBoxEdge; }

    std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom() const { return m_intrusiveInitialLetterLogicalBottom; }
    const std::optional<LineGrid>& lineGrid() const LIFETIME_BOUND { return m_lineGrid; }

    // The margins coming into the inline content; the inline formatting context carries its own copy (InlineLayoutState::marginState).
    const MarginState& marginState() const LIFETIME_BOUND { return m_marginState; }

    // The block containers whose content is being laid out, from the root to the innermost one.
    Vector<ContainingBlockState>& containingBlockStates() LIFETIME_BOUND { return m_containingBlockStates; }
    BlockMarginCollapse& marginCollapse() LIFETIME_BOUND { return m_marginCollapse; }

private:
    std::optional<LineClamp> m_lineClamp;
    TextBoxTrim m_textBoxTrim;
    Style::TextBoxEdge m_textBoxEdge;
    std::optional<LayoutUnit> m_intrusiveInitialLetterLogicalBottom;
    std::optional<LineGrid> m_lineGrid;
    MarginState m_marginState;
    Vector<ContainingBlockState> m_containingBlockStates;
    BlockMarginCollapse m_marginCollapse;
};

inline BlockLayoutState::BlockLayoutState(MarginState marginState, std::optional<LineClamp> lineClamp, TextBoxTrim textBoxTrim, Style::TextBoxEdge textBoxEdge, std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom, std::optional<LineGrid> lineGrid)
    : m_lineClamp(lineClamp)
    , m_textBoxTrim(textBoxTrim)
    , m_textBoxEdge(textBoxEdge)
    , m_intrusiveInitialLetterLogicalBottom(intrusiveInitialLetterLogicalBottom)
    , m_lineGrid(lineGrid)
    , m_marginState(marginState)
{
}

}
}
