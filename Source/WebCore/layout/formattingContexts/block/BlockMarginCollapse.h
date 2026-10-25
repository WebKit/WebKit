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

#include <WebCore/AdjoiningMargins.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/WritingMode.h>
#include <optional>

namespace WebCore {

class RenderBox;

namespace Layout {

class BoxGeometry;
struct ContainingBlockState;

// Quirks mode: adjoining margins, and whether the ones that are not zero are all quirky (the user agent's default margins, like p's).
struct MarginsWithQuirk {
    // A block-end margin that collapses through a box does not decide whether they are quirky.
    void add(LayoutUnit, std::optional<bool> isQuirky);
    void add(const MarginsWithQuirk&);
    bool areQuirky() const { return nonZeroMarginsAreQuirky.value_or(true); }

    AdjoiningMargins margins;
    std::optional<bool> nonZeroMarginsAreQuirky;
};

class BlockMarginCollapse {
public:
    // The box's block-start and block-end margins are adjoining (CSS 2.2 §8.3.1), given that its content did not separate them: it has no
    // block-start border or padding (its top is still unknown) and its in-flow children all collapse through.
    static bool marginsCollapseThrough(const RenderBox&, const BoxGeometry&, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);

    class BlockContainer {
    public:
        // The margins that collapse with its block-start edge, its own included, complete once its content is laid out.
        const AdjoiningMargins& collapsedMarginsBefore() const LIFETIME_BOUND { return m_collapsedMarginsBefore.margins; }
        // Like RenderBlockFlow::collapseMarginsWithChildInfo, which keeps the box's own quirk unless a margin that is not zero decides.
        bool collapsedMarginsBeforeAreQuirky() const { return m_collapsedMarginsBefore.nonZeroMarginsAreQuirky.value_or(m_marginBeforeIsQuirky); }

    private:
        friend class BlockMarginCollapse;
        MarginsWithQuirk m_collapsedMarginsBefore;
        bool m_marginBeforeIsQuirky { false };
        // Its block-start margin collapses with its containing block's, which was still waiting for its top when the block container started.
        bool m_collapsesWithContainingBlockMarginBefore { false };
    };

    static bool isQuirkContainer(const RenderBox&);

    const AdjoiningMargins& adjoiningMargins() const LIFETIME_BOUND { return m_adjoiningMargins; }
    // Where the adjoining margins start: the last edge that separated margins.
    LayoutUnit logicalTop() const { return m_logicalTop; }
    LayoutUnit collapsedLogicalBottom() const { return m_logicalTop + m_adjoiningMargins.collapsedValue(); }
    // The same for a box whose margins collapse through it: in a quirk container, the margins of the child count when they are not all quirky.
    LayoutUnit collapsedLogicalBottomForCollapsedThroughBox() const;
    // Whether the margin after the last box that did not collapse through is quirky (RenderBlockFlow::MarginInfo::hasMarginAfterQuirk).
    bool endsWithQuirk() const { return m_endsWithQuirk.value_or(false); }

    void resetAdjoiningMargins(LayoutUnit logicalTop);
    // The same at the bottom of the last line box of inline content. WebKit compatibility: like on the render tree path, the block-end margin
    // of its block container keeps its own quirk.
    void resetAdjoiningMarginsAfterLineBoxes(LayoutUnit logicalTop);
    // The block-start margin of a block container joins the adjoining margins, which collapse with its block-start edge until something
    // separates them.
    void startBlockContainer(ContainingBlockState& blockContainerState, const ContainingBlockState&, LayoutUnit marginBefore);
    // The margins that collapse with the block-start edge of a block container collapse with its containing block's too when the two
    // block-start margins collapse.
    void endBlockContainer(const ContainingBlockState& blockContainerState, ContainingBlockState&);
    // The root's margins do not collapse with its children's, but the root can be a quirk container.
    void startBlockFormattingContextRoot(const ContainingBlockState& rootState);
    void endBlockFormattingContextRoot(const ContainingBlockState& rootState);
    // The block-end margin of a box whose margins collapse through it. It also collapses with its containing block's block-start margin
    // when the box's block-start margin does.
    void addCollapsedThroughBlockEndMargin(const ContainingBlockState& blockContainerState, ContainingBlockState&, LayoutUnit marginAfter);
    // The block-end margin of a box whose margins do not collapse through it. It follows the bottom border edge of the box, and collapses
    // with the margins after its content unless something separates them.
    // Returns whether the margins that collapse with it are quirky.
    bool addBlockEndMargin(LayoutUnit, bool isQuirky, bool collapsesWithMarginsAfterContent, LayoutUnit borderBoxLogicalBottom);
    // The block container separates margins. Unless it is the quirk container itself (its block-start border and padding), this ends the
    // quirk container's block-start side.
    void endQuirkContainerBlockStartSide(const ContainingBlockState& blockContainerState);

private:
    // The margins that collapse with the block-start edge of the block container collapse with its containing block's too, and so does its
    // block-end margin when its margins collapse through it.
    static bool marginsBeforeCollapseWithContainingBlock(const ContainingBlockState& blockContainerState, const ContainingBlockState&);
    void resolveQuirkContainerChildMargins();

    AdjoiningMargins m_adjoiningMargins;
    LayoutUnit m_logicalTop;
    // Unset after line boxes, until the block-end margin of their block container.
    std::optional<bool> m_endsWithQuirk { false };
    // Quirks mode: body is a quirk container (RenderBlockFlow::MarginInfo). At its block-start side, until something in it separates margins,
    // the margins that collapse with the block-start edge of its current child are kept here, apart from the adjoining margins, and dropped
    // when they are all quirky. A body inside another one stays on the render tree path.
    std::optional<MarginsWithQuirk> m_quirkContainerChildMargins;
};

}
}
