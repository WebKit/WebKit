/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "InlineFormattingContext.h"
#include "InlineLineBuilder.h"
#include <WebCore/LayoutUnits.h>
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

struct AncestorStack;
class ElementBox;
struct DisplayBoxTree;
struct IsFirstLastIndex;
class LineBox;

class InlineDisplayContentBuilder {
public:
    InlineDisplayContentBuilder(InlineFormattingContext&, const ConstraintsForInlineContent&, const LineBox&, const InlineDisplay::Line&);

    InlineDisplay::Boxes build(const LineLayoutResult&);
    InlineDisplay::Boxes buildTextOnlyContent(const LineLayoutResult&);

private:
    void processNonBidiContent(const LineLayoutResult&, InlineDisplay::Boxes&);
    void processBidiContent(const LineLayoutResult&, InlineDisplay::Boxes&);
    bool processBidiLinesWithNoContent(const LineLayoutResult&, InlineDisplay::Boxes&);
    void collectInkOverflowForInlineBoxes(std::span<InlineDisplay::Box>);
    void collectInkOverflowForTextDecorations(std::span<InlineDisplay::Box>);
    void truncateForEllipsisPolicy(LineEndingTruncationPolicy, const LineLayoutResult&, InlineDisplay::Boxes&);

    void appendTextDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendSoftLineBreakDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&) const;
    void appendHardLineBreakDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&) const;
    void appendAtomicInlineLevelDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendBlockLevelDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendRootInlineBoxDisplayBox(const InlineRect&, bool lineHasContent, InlineDisplay::Boxes&) const;
    void appendInlineBoxDisplayBox(const Line::Run&, const InlineLevelBox&, const InlineRect&, InlineDisplay::Boxes&);
    void appendInlineDisplayBoxAtBidiBoundary(const Box&, InlineDisplay::Boxes&);
    void insertRubyAnnotationBoxes(const Vector<size_t>& rubyBaseStartIndexListWithAnnotation, InlineDisplay::Boxes&);

    size_t processRubyBase(size_t rubyBaseStart, std::span<InlineDisplay::Box>, Vector<WTF::Range<size_t>>& interlinearRubyColumnRangeList, Vector<size_t>& rubyBaseStartIndexListWithAnnotation);
    Vector<size_t> processRubyContent(std::span<InlineDisplay::Box>, const LineLayoutResult&);

    inline InlineRect mapInlineRectLogicalToVisual(const InlineRect& logicalRect, const InlineRect& containerLogicalRect, WritingMode);

    void setInlineBoxGeometry(const Box& inlineBox, Layout::BoxGeometry&, const InlineRect&, bool isFirstInlineBoxFragment);
    void adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& accumulatedOffset, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree&, std::span<InlineDisplay::Box>, const HashMap<const Box*, IsFirstLastIndex>&);
    size_t ensureDisplayBoxForContainer(const ElementBox&, DisplayBoxTree&, AncestorStack&, InlineDisplay::Boxes&);

    template <typename BoxType, typename LayoutUnitType>
    void NODELETE setLogicalLeft(BoxType&, LayoutUnitType logicalLeft, WritingMode) const;
    void setLogicalRight(InlineDisplay::Box&, InlineLayoutUnit logicalRight, WritingMode) const;
    InlineLayoutPoint movePointHorizontallyForWritingMode(const InlineLayoutPoint& topLeft, InlineLayoutUnit horizontalOffset, WritingMode) const;
    InlineLayoutUnit outsideListMarkerVisualPosition(const ElementBox&) const;
    void setGeometryForBlockLevelOutOfFlowBoxes(const Vector<size_t>& indexList, const Line::RunList&, const Vector<int32_t>& visualOrderList = { });

    bool isLineFullyTruncatedInBlockDirection() const { return m_lineIsFullyTruncatedInBlockDirection; }

    bool isFirstFormattedLine() const { return lineBox().isFirstFormattedLine(); }

    const LineBox& lineBox() const LIFETIME_BOUND { return m_lineBox; }
    size_t lineIndex() const { return lineBox().lineIndex(); }
    const ConstraintsForInlineContent& constraints() const LIFETIME_BOUND { return m_constraints; }
    const ElementBox& root() const { return m_formattingContext.root(); }
    const RenderStyle& rootStyle() const LIFETIME_BOUND { return lineIndex() ? root().style() : root().firstLineStyle(); }
    InlineFormattingContext& formattingContext() LIFETIME_BOUND { return m_formattingContext; }
    const InlineFormattingContext& formattingContext() const LIFETIME_BOUND { return m_formattingContext; }

private:
    InlineFormattingContext& m_formattingContext;
    const ConstraintsForInlineContent& m_constraints;
    const LineBox& m_lineBox;
    const InlineDisplay::Line& m_displayLine;
    IntSize m_initialContaingBlockSize;
    // FIXME: This should take DisplayLine::isFullyTruncatedInBlockDirection() for non-prefixed line-clamp.
    bool m_lineIsFullyTruncatedInBlockDirection { false };
    bool m_contentHasInkOverflow { false };
    bool m_hasSeenRubyBase { false };
    bool m_hasSeenTextDecoration { false };
    bool m_hasSeenNestedInlineBoxesWithDifferentFontCascade { false };
};

inline InlineRect InlineDisplayContentBuilder::mapInlineRectLogicalToVisual(const InlineRect& logicalRect, const InlineRect& containerLogicalRect, WritingMode writingMode)
{
    InlineRect visualRect = logicalRect;
    switch (writingMode.computedWritingMode()) {
    case StyleWritingMode::HorizontalTb:
        return visualRect;

    case StyleWritingMode::HorizontalBt:
        visualRect.setTop(containerLogicalRect.height() - logicalRect.bottom());
        return visualRect;

    case StyleWritingMode::VerticalRl:
    case StyleWritingMode::SidewaysRl:
        visualRect.setLeft(logicalRect.top());
        visualRect.setTop(logicalRect.left());
        visualRect.setWidth(logicalRect.height());
        visualRect.setHeight(logicalRect.width());
        return visualRect;

    case StyleWritingMode::VerticalLr:
        visualRect.setLeft(containerLogicalRect.height() - logicalRect.bottom());
        visualRect.setTop(logicalRect.left());
        visualRect.setWidth(logicalRect.height());
        visualRect.setHeight(logicalRect.width());
        return visualRect;

    case StyleWritingMode::SidewaysLr:
        visualRect.setLeft(logicalRect.top());
        visualRect.setTop(containerLogicalRect.width() - logicalRect.right());
        visualRect.setWidth(logicalRect.height());
        visualRect.setHeight(logicalRect.width());
        return visualRect;

    default:
        ASSERT_NOT_REACHED();
        return visualRect;
    }
}

}
}

