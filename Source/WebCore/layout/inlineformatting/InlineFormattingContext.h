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

#include "DisplayBox.h"
#include "FormattingContext.h"
#include "InlineFormattingState.h"
#include "InlineLine.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class FloatingState;
struct LineContent;
struct LineInput;

// This class implements the layout logic for inline formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
class InlineFormattingContext : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingContext);
public:
    InlineFormattingContext(const Box& formattingContextRoot, InlineFormattingState&);
    void layout() override;

private:
    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;

    class InlineLayout {
    public:
        InlineLayout(const InlineFormattingContext&);
        void layout(const InlineItems&, LayoutUnit widthConstraint) const;
        LayoutUnit computedIntrinsicWidth(const InlineItems&, LayoutUnit widthConstraint) const;

    private:
        LayoutState& layoutState() const { return m_layoutState; }
        LineContent placeInlineItems(const LineInput&) const;
        void createDisplayRuns(const Line::Content&, const Vector<WeakPtr<InlineItem>>& floats, LayoutUnit widthConstraint) const;
        void alignRuns(TextAlignMode, InlineRuns&, unsigned firstRunIndex, LayoutUnit availableWidth) const;

    private:
        LayoutState& m_layoutState;
        const Container& m_formattingRoot;
    };

    class Quirks : public FormattingContext::Quirks {
    public:
        Quirks(LayoutState&);

        bool lineDescentNeedsCollapsing(const Line::Content&) const;
        Line::InitialConstraints::HeightAndBaseline lineHeightConstraints(const Box& formattingRoot) const;
    };
    InlineFormattingContext::Quirks quirks() const { return Quirks(layoutState()); }

    class Geometry : public FormattingContext::Geometry {
    public:
        Geometry(LayoutState&);

        HeightAndMargin inlineBlockHeightAndMargin(const Box&) const;
        WidthAndMargin inlineBlockWidthAndMargin(const Box&, UsedHorizontalValues);
    };
    InlineFormattingContext::Geometry geometry() const { return Geometry(layoutState()); }

    void layoutFormattingContextRoot(const Box&, UsedHorizontalValues);
    void computeMarginBorderAndPaddingForInlineContainer(const Container&, UsedHorizontalValues);
    void initializeMarginBorderAndPaddingForGenericInlineBox(const Box&);
    void computeIntrinsicWidthForFormattingRoot(const Box&);
    void computeWidthAndHeightForReplacedInlineBox(const Box&, UsedHorizontalValues);
    void computeHorizontalMargin(const Box&, UsedHorizontalValues);
    void computeHeightAndMargin(const Box&);
    void computeWidthAndMargin(const Box&, UsedHorizontalValues);

    void collectInlineContent() const;

    InlineFormattingState& formattingState() const { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }
    // FIXME: Come up with a structure that requires no friending.
    friend class Line;
};

inline InlineFormattingContext::Geometry::Geometry(LayoutState& layoutState)
    : FormattingContext::Geometry(layoutState)
{
}

inline InlineFormattingContext::Quirks::Quirks(LayoutState& layoutState)
    : FormattingContext::Quirks(layoutState)
{
}

}
}
#endif
