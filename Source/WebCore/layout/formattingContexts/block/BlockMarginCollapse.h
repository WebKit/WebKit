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

class BlockMarginCollapse {
public:
    // The box's block-start and block-end margins are adjoining (CSS 2.2 §8.3.1), given that its content did not separate them: it has no
    // block-start border or padding (its top is still unknown) and its in-flow children all collapse through.
    static bool marginsCollapseThrough(const RenderBox&, const BoxGeometry&, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode);

    class BlockContainer {
    public:
        // The margins that collapse with its block-start edge, its own included, complete once its content is laid out.
        const AdjoiningMargins& collapsedMarginsBefore() const LIFETIME_BOUND { return m_collapsedMarginsBefore; }

    private:
        friend class BlockMarginCollapse;
        AdjoiningMargins m_collapsedMarginsBefore;
        // Its block-start margin collapses with its containing block's, which was still waiting for its top when the block container started.
        bool m_collapsesWithContainingBlockMarginBefore { false };
    };

    const AdjoiningMargins& adjoiningMargins() const LIFETIME_BOUND { return m_adjoiningMargins; }
    // Where the adjoining margins start: the last edge that separated margins.
    LayoutUnit logicalTop() const { return m_logicalTop; }
    LayoutUnit collapsedLogicalBottom() const { return m_logicalTop + m_adjoiningMargins.collapsedValue(); }

    void resetAdjoiningMargins(LayoutUnit logicalTop);
    // The block-start margin of a block container joins the adjoining margins, which collapse with its block-start edge until something
    // separates them.
    void startBlockContainer(ContainingBlockState& blockContainerState, const ContainingBlockState&, LayoutUnit marginBefore);
    // The margins that collapse with the block-start edge of a block container collapse with its containing block's too when the two
    // block-start margins collapse.
    void endBlockContainer(const ContainingBlockState& blockContainerState, ContainingBlockState&);
    // The block-end margin of a box whose margins collapse through it. It also collapses with its containing block's block-start margin
    // when the box's block-start margin does.
    void addCollapsedThroughBlockEndMargin(const ContainingBlockState& blockContainerState, ContainingBlockState&, LayoutUnit marginAfter);
    // The block-end margin of a box whose margins do not collapse through it. It follows the bottom border edge of the box, and collapses
    // with the margins after its content unless something separates them.
    void addBlockEndMargin(LayoutUnit, bool collapsesWithMarginsAfterContent, LayoutUnit borderBoxLogicalBottom);

private:
    AdjoiningMargins m_adjoiningMargins;
    LayoutUnit m_logicalTop;
};

}
}
