/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingGeometry.h"
#include "BlockFormattingQuirks.h"
#include "BlockFormattingState.h"
#include "FormattingContext.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMalloc.h>

namespace WebCore {

class LayoutUnit;

namespace Layout {

class Box;
class BlockMarginCollapse;
class FloatingContext;

// This class implements the layout logic for block formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#block-formatting
class BlockFormattingContext : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const ContainerBox& formattingContextRoot, BlockFormattingState&);

    void layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent&) override;
    LayoutUnit usedContentHeight() const override;

    const BlockFormattingState& formattingState() const { return downcast<BlockFormattingState>(FormattingContext::formattingState()); }
    const BlockFormattingGeometry& formattingGeometry() const final { return m_blockFormattingGeometry; }
    const BlockFormattingQuirks& formattingQuirks() const override { return m_blockFormattingQuirks; }

protected:
    struct ConstraintsPair {
        ConstraintsForInFlowContent formattingContextRoot;
        ConstraintsForInFlowContent containingBlock;
    };
    void placeInFlowPositionedChildren(const ContainerBox&, const HorizontalConstraints&);

    void computeWidthAndMargin(const FloatingContext&, const Box&, const ConstraintsPair&);
    void computeHeightAndMargin(const Box&, const ConstraintsForInFlowContent&);

    void computeStaticHorizontalPosition(const Box&, const HorizontalConstraints&);
    void computeStaticVerticalPosition(const Box&, LayoutUnit containingBlockContentBoxTop);
    void computePositionToAvoidFloats(const FloatingContext&, const Box&, const ConstraintsPair&);
    void computeVerticalPositionForFloatClear(const FloatingContext&, const Box&);

    void precomputeVerticalPositionForBoxAndAncestors(const Box&, const ConstraintsPair&);

    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;

    LayoutUnit verticalPositionWithMargin(const Box&, const UsedVerticalMargin&, LayoutUnit containingBlockContentBoxTop) const;

    std::optional<LayoutUnit> usedAvailableWidthForFloatAvoider(const FloatingContext&, const Box&, const ConstraintsPair&);
    void updateMarginAfterForPreviousSibling(const Box&);

    BlockFormattingState& formattingState() { return downcast<BlockFormattingState>(FormattingContext::formattingState()); }
    BlockMarginCollapse marginCollapse() const;

#if ASSERT_ENABLED
    void setPrecomputedMarginBefore(const Box& layoutBox, const PrecomputedMarginBefore& precomputedMarginBefore) { m_precomputedMarginBeforeList.set(&layoutBox, precomputedMarginBefore); }
    PrecomputedMarginBefore precomputedMarginBefore(const Box& layoutBox) const { return m_precomputedMarginBeforeList.get(&layoutBox); }
    bool hasPrecomputedMarginBefore(const Box& layoutBox) const { return m_precomputedMarginBeforeList.contains(&layoutBox); }
#endif

private:
    HashMap<const Box*, PrecomputedMarginBefore> m_precomputedMarginBeforeList;
    const BlockFormattingGeometry m_blockFormattingGeometry;
    const BlockFormattingQuirks m_blockFormattingQuirks;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(BlockFormattingContext, isBlockFormattingContext())

#endif
