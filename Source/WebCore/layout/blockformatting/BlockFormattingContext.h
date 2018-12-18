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

#include "FormattingContext.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class LayoutUnit;

namespace Layout {

class BlockFormattingState;
class Box;
class FloatingContext;

// This class implements the layout logic for block formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#block-formatting
class BlockFormattingContext : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const Box& formattingContextRoot, FormattingState& formattingState);

    void layout() const override;

private:
    void layoutFormattingContextRoot(FloatingContext&, const Box&) const;
    void placeInFlowPositionedChildren(const Container&) const;

    void computeWidthAndMargin(const Box&) const;
    void computeHeightAndMargin(const Box&) const;

    void computeStaticPosition(const Box&) const;
    void computeFloatingPosition(const FloatingContext&, const Box&) const;
    void computePositionToAvoidFloats(const FloatingContext&, const Box&) const;
    void computeVerticalPositionForFloatClear(const FloatingContext&, const Box&) const;

    void computeEstimatedMarginBeforeForAncestors(const Box&) const;
    void computeEstimatedMarginBefore(const Box&) const;

    void precomputeVerticalPositionForFormattingRootIfNeeded(const Box&) const;

    InstrinsicWidthConstraints instrinsicWidthConstraints() const override;

    // This class implements positioning and sizing for boxes participating in a block formatting context.
    class Geometry : public FormattingContext::Geometry {
    public:
        static HeightAndMargin inFlowHeightAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static WidthAndMargin inFlowWidthAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { });

        static Point staticPosition(const LayoutState&, const Box&);

        static bool instrinsicWidthConstraintsNeedChildrenWidth(const Box&);
        static InstrinsicWidthConstraints instrinsicWidthConstraints(const LayoutState&, const Box&);

        static LayoutUnit estimatedMarginBefore(const LayoutState&, const Box&);
        static LayoutUnit estimatedMarginAfter(const LayoutState&, const Box&);

    private:
        // This class implements margin collapsing for block formatting context.
        class MarginCollapse {
        public:
            static LayoutUnit marginBefore(const LayoutState&, const Box&);
            static LayoutUnit marginAfter(const LayoutState&, const Box&);

            static bool marginBeforeCollapsesWithParentMarginAfter(const Box&);
            static bool marginAfterCollapsesWithParentMarginAfter(const LayoutState&, const Box&);

        private:
            static LayoutUnit collapsedMarginAfterFromLastChild(const LayoutState&, const Box&);
            static LayoutUnit nonCollapsedMarginAfter(const LayoutState&, const Box&);

            static LayoutUnit computedNonCollapsedMarginBefore(const LayoutState&, const Box&);
            static LayoutUnit computedNonCollapsedMarginAfter(const LayoutState&, const Box&);

            static LayoutUnit collapsedMarginBeforeFromFirstChild(const LayoutState&, const Box&);
            static LayoutUnit nonCollapsedMarginBefore(const LayoutState&, const Box&);

            static bool marginBeforeCollapsesWithParentMarginBefore(const LayoutState&, const Box&);
            static bool marginBeforeCollapsesWithPreviousSibling(const Box&);
            static bool marginAfterCollapsesWithNextSibling(const Box&);
            static bool marginAfterCollapsesWithSiblingMarginBeforeWithClearance(const LayoutState&, const Box&);
            static bool marginAfterCollapsesWithParentMarginBefore(const LayoutState&, const Box&);
            static bool marginsCollapseThrough(const LayoutState&, const Box&);
        };

        static HeightAndMargin inFlowNonReplacedHeightAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedHeight = { });
        static WidthAndMargin inFlowNonReplacedWidthAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { });
        static WidthAndMargin inFlowReplacedWidthAndMargin(const LayoutState&, const Box&, std::optional<LayoutUnit> usedWidth = { });
        static Point staticPositionForOutOfFlowPositioned(const LayoutState&, const Box&);
    };

    class Quirks {
    public:
        static bool needsStretching(const LayoutState&, const Box&);
        static HeightAndMargin stretchedHeight(const LayoutState&, const Box&, HeightAndMargin);

        static bool shouldIgnoreMarginBefore(const LayoutState&, const Box&);
    };
};

}
}
#endif
