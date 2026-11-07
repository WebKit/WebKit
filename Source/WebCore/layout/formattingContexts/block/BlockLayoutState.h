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
#include <WebCore/PlacedFloats.h>
#include <WebCore/StyleTextBoxEdge.h>
#include <algorithm>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/EnumSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

namespace Layout {

class BlockFormattingContext;

// What text-box-trim the content laid out next is in (css-inline-3 §6.2, LocalFrameViewLayoutContext::TextBoxTrim): its first formatted line
// gets its start trimmed, and the last formatted line, in the inline content of a block container, its end.
struct TextBoxTrimState {
    bool trimsFirstFormattedLine { false };
    CheckedPtr<const RenderBlockFlow> lastFormattedLineRoot;
};

struct BlockEndChildren {
    Vector<size_t> collapsedThroughChildren;
    std::optional<size_t> lastChild;
    // Relative to the border box of the block container.
    LayoutUnit contentBoxLogicalTop;
};

struct ContainingBlockState {
    CheckedRef<const RenderBox> blockContainer;
    // The geometry of the block container itself, completed once its content is laid out (not used for the root).
    BoxGeometry geometry;
    // The content box in the inline direction, from the inline start of the block container's border box (in its own direction).
    LayoutRange inlineRange;
    // The inline start of the block container's border box from the inline start of the root's border box, which the floats are placed from.
    LayoutUnit borderBoxLogicalLeftInRoot;
    // Block offsets are relative to the border box of the root (every box in the block formatting context has the same block flow
    // direction). The top is unknown while the block-start margin can still collapse with the margins that follow it (CSS 2.2 §8.3.1).
    std::optional<LayoutUnit> borderBoxLogicalTop;
    LayoutUnit borderAndPaddingBefore;
    // Percentage block sizes of the children resolve against this, and behave as auto without it (CSS 2.2 §10.5).
    std::optional<LayoutUnit> contentBoxBlockSize;
    // -webkit-fill-available block sizes of the children resolve to this (RenderBox::availableLogicalHeight).
    LayoutUnit fillAvailableBlockSize;
    // In quirks mode percentage block sizes of replaced children resolve against this, which keeps the margins, borders and padding of the
    // boxes with an auto block size it passes through (RenderReplaced::computeReplacedLogicalHeightUsing).
    LayoutUnit replacedElementAvailableBlockSize;
    BlockMarginCollapse::BlockContainer margins;
    // Its own formatting context lays it out through the render tree and gives it its block size.
    bool isLaidOutThroughRenderTree { false };
    // The children laid out through the render tree are laid out again (RenderBlockFlow::layoutBlockChildren), like when its content box
    // inline size, border or padding changed since its last layout (RenderBlock::recomputeLogicalWidth).
    bool relayoutChildren { false };
    // The lines with content in its inline content up to the line of the last block-level box in it, that line included: the inline
    // formatting context's line count is larger when line boxes came after that box.
    size_t lineCountThroughLastBlockLevelBox { 0 };
    // Its text-box-trim applies to its content only, and what its ancestors' did for the content after it is restored at its end.
    std::optional<TextBoxTrimState> textBoxTrimStateToRestore { };
    // Its margin-trim discards the margins at the block-start edge of its content; whether an ancestor's still does after it, unless something
    // separated margins in it.
    std::optional<bool> trimsMarginsAtBlockStartEdgeToRestore { };
    // The children at the block-end edge of its content in the result: those whose margins collapsed through them after the last one that
    // did not, and that one, which margin-trim at the block-end edge places again.
    BlockEndChildren blockEndChildren { };
    // The adjoining margins at the start of its content, which margin-trim at the block-end edge goes back to when all of its content collapses
    // through it.
    std::optional<AdjoiningMargins> adjoiningMarginsAtContentStart { };
    // clear, until the block container's top is known (CSS 2.2 §9.5.2): the bottom outer edge of the lowest float it goes below, and where the
    // margins before its block-start margin put its containing block's top, which they keep when it gets clearance.
    struct Clearance {
        // The floats waiting for their block container's top go with it, from the one their block containers get.
        LayoutUnit floatsLogicalBottomFor(LayoutUnit borderBoxLogicalTop) const
        {
            return pendingFloatsLogicalBottomFromTop ? std::max(floatsLogicalBottom, borderBoxLogicalTop + *pendingFloatsLogicalBottomFromTop) : floatsLogicalBottom;
        }

        LayoutUnit floatsLogicalBottom { LayoutUnit::min() };
        std::optional<LayoutUnit> pendingFloatsLogicalBottomFromTop;
        LayoutUnit marginsBeforeLogicalBottom;
    };
    std::optional<Clearance> clearance { };
    // The last in-flow children are boxes whose margins collapse through them, one of them with clearance: the margins after them do not
    // collapse with the block container's block-end margin (CSS 2.2 §8.3.1).
    bool endsWithClearedBoxCollapsedThrough { false };

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
        // Inline content that 'text-wrap-style' constrains was laid out as if it were 'stable' (RenderLayoutState::LineClamp).
        bool hasConstrainedTextWrapContent { false };
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
    // The content takes its lines off the budget as it gets laid out (LineClampUpdater).
    void setLineClamp(std::optional<LineClamp> lineClamp) { m_lineClamp = lineClamp; }
    // The clamp point is between this in-flow block-level box and its next sibling, with no line box right before it (see the auto clamp point).
    const RenderBox* clampAfterBox() const { return m_clampAfterBox.get(); }
    void setClampAfterBox(const RenderBox* clampAfterBox) { m_clampAfterBox = clampAfterBox; }
    TextBoxTrim textBoxTrim() const { return m_textBoxTrim; }
    Style::TextBoxEdge textBoxEdge() const { return m_textBoxEdge; }
    // The sides of the inline content laid out next to trim, and how.
    void setTextBoxTrim(TextBoxTrim textBoxTrim) { m_textBoxTrim = textBoxTrim; }
    void setTextBoxEdge(Style::TextBoxEdge textBoxEdge) { m_textBoxEdge = textBoxEdge; }

    const TextBoxTrimState& textBoxTrimState() const LIFETIME_BOUND { return m_textBoxTrimState; }
    void setTextBoxTrimState(TextBoxTrimState state) { m_textBoxTrimState = WTF::move(state); }
    // The block container with the last formatted line of a box with trim-end and block-level content, which only its first layout tells.
    const RenderBlockFlow* lastFormattedLineRoot(const RenderBox& box) const
    {
        for (auto& [trimmingBox, lastFormattedLineRoot] : m_lastFormattedLineRoots) {
            if (trimmingBox.ptr() == &box)
                return lastFormattedLineRoot.ptr();
        }
        return nullptr;
    }
    // The children at the block-end edge of the content of the boxes in the result whose block-end margin collapses with them, which margin-trim
    // at the block-end edge of an ancestor places again.
    Vector<std::pair<size_t, BlockEndChildren>>& blockEndChildren() LIFETIME_BOUND { return m_blockEndChildren; }

    using LastFormattedLineRoots = Vector<std::pair<CheckedRef<const RenderBox>, CheckedRef<const RenderBlockFlow>>>;
    const LastFormattedLineRoots& lastFormattedLineRoots() const LIFETIME_BOUND { return m_lastFormattedLineRoots; }
    void setLastFormattedLineRoots(LastFormattedLineRoots&& lastFormattedLineRoots) { m_lastFormattedLineRoots = WTF::move(lastFormattedLineRoots); }

    std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom() const { return m_intrusiveInitialLetterLogicalBottom; }
    const std::optional<LineGrid>& lineGrid() const LIFETIME_BOUND { return m_lineGrid; }

    // The margins coming into the inline content; the inline formatting context carries its own copy (InlineLayoutState::marginState).
    const MarginState& marginState() const LIFETIME_BOUND { return m_marginState; }

    // The block containers whose content is being laid out, from the root to the innermost one.
    Vector<ContainingBlockState>& containingBlockStates() LIFETIME_BOUND { return m_containingBlockStates; }
    BlockMarginCollapse& marginCollapse() LIFETIME_BOUND { return m_marginCollapse; }

    // The floats placed in the block formatting context so far, in the logical coordinates of the root's border box (CSS 2.2 §9.5).
    const PlacedFloats* placedFloats() const { return m_placedFloats ? &*m_placedFloats : nullptr; }
    PlacedFloats& ensurePlacedFloats(const ElementBox& root) LIFETIME_BOUND
    {
        if (!m_placedFloats)
            m_placedFloats.emplace(root);
        return *m_placedFloats;
    }
    // The floats in the inline content of a block container whose top was not known yet, placed from the top the margins collapsed so far
    // gave it: its top ends up where the margins that collapse with it put it (CSS 2.2 §8.3.1), and they move with it.
    struct PendingFloats {
        CheckedRef<const RenderBox> blockContainer;
        Vector<CheckedRef<const Box>> floatBoxes;
        LayoutUnit provisionalBorderBoxLogicalTop;
        // Floats whose top is known were there: the pending floats went beside or below them at the provisional top.
        bool isPlacedAgainstOtherFloats { false };
    };
    Vector<PendingFloats>& pendingFloats() LIFETIME_BOUND { return m_pendingFloats; }

private:
    std::optional<LineClamp> m_lineClamp;
    CheckedPtr<const RenderBox> m_clampAfterBox;
    TextBoxTrim m_textBoxTrim;
    Style::TextBoxEdge m_textBoxEdge;
    TextBoxTrimState m_textBoxTrimState;
    LastFormattedLineRoots m_lastFormattedLineRoots;
    Vector<std::pair<size_t, BlockEndChildren>> m_blockEndChildren;
    std::optional<LayoutUnit> m_intrusiveInitialLetterLogicalBottom;
    std::optional<LineGrid> m_lineGrid;
    MarginState m_marginState;
    Vector<ContainingBlockState> m_containingBlockStates;
    BlockMarginCollapse m_marginCollapse;
    std::optional<PlacedFloats> m_placedFloats;
    Vector<PendingFloats> m_pendingFloats;
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
