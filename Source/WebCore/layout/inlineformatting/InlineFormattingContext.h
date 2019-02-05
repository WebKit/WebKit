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
#include "InlineRun.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InlineContainer;
class InlineFormattingState;
class InlineRunProvider;

// This class implements the layout logic for inline formatting contexts.
// https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
class InlineFormattingContext : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingContext);
public:
    InlineFormattingContext(const Box& formattingContextRoot, InlineFormattingState&);

    void layout() const override;

private:
    class Line {
    public:
        void init(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimalHeight);
        void close();

        void appendContent(const InlineRunProvider::Run&, const LayoutSize&);

        void adjustLogicalLeft(LayoutUnit delta);
        void adjustLogicalRight(LayoutUnit delta);

        bool hasContent() const { return !m_inlineRuns.isEmpty(); }
        bool isClosed() const { return m_closed; }
        bool isFirstLine() const { return m_isFirstLine; }
        Vector<InlineRun>& runs() { return m_inlineRuns; }

        LayoutUnit contentLogicalRight() const;
        LayoutUnit contentLogicalLeft() const { return m_logicalRect.left(); }
        LayoutUnit availableWidth() const { return m_availableWidth; }
        Optional<InlineRunProvider::Run::Type> lastRunType() const { return m_lastRunType; }

        LayoutUnit logicalTop() const { return m_logicalRect.top(); }
        LayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
        LayoutUnit logicalHeight() const { return logicalBottom() - logicalTop(); }

    private:
        struct TrailingTrimmableContent {
            LayoutUnit width;
            unsigned length;
        };
        Optional<TrailingTrimmableContent> m_trailingTrimmableContent;
        Optional<InlineRunProvider::Run::Type> m_lastRunType;
        bool m_lastRunCanExpand { false };

        Display::Box::Rect m_logicalRect;
        LayoutUnit m_availableWidth;

        Vector<InlineRun> m_inlineRuns;
        bool m_isFirstLine { true };
        bool m_closed { true };
    };
    enum class IsLastLine { No, Yes };

    class Geometry : public FormattingContext::Geometry {
    public:
        static HeightAndMargin inlineBlockHeightAndMargin(const LayoutState&, const Box&);
        static WidthAndMargin inlineBlockWidthAndMargin(LayoutState&, const Box&);
        static void alignRuns(TextAlignMode, Line&, IsLastLine);
        static void computeExpansionOpportunities(Line&, const InlineRunProvider::Run&, InlineRunProvider::Run::Type lastRunType);
        static LayoutUnit runWidth(const InlineContent&, const InlineItem&, ItemPosition from, unsigned length, LayoutUnit contentLogicalLeft); 

    private:
        static void justifyRuns(Line&);
    };

    void layoutInlineContent(const InlineRunProvider&) const;
    void initializeNewLine(Line&) const;
    void closeLine(Line&, IsLastLine) const;
    void appendContentToLine(Line&, const InlineRunProvider::Run&, const LayoutSize&) const;
    void postProcessInlineRuns(Line&, IsLastLine) const;
    void createFinalRuns(Line&) const;
    void splitInlineRunIfNeeded(const InlineRun&, InlineRuns& splitRuns) const;

    void layoutFormattingContextRoot(const Box&) const;
    void computeWidthAndHeightForReplacedInlineBox(const Box&) const;
    void computeMarginBorderAndPadding(const InlineContainer&) const;
    void computeHeightAndMargin(const Box&) const;
    void computeWidthAndMargin(const Box&) const;
    void computeFloatPosition(const FloatingContext&, Line&, const Box&) const;
    void placeInFlowPositionedChildren(unsigned firstRunIndex) const;

    void collectInlineContent(InlineRunProvider&) const;
    InstrinsicWidthConstraints instrinsicWidthConstraints() const override;

    InlineFormattingState& formattingState() const { return downcast<InlineFormattingState>(FormattingContext::formattingState()); }
};

}
}
#endif
