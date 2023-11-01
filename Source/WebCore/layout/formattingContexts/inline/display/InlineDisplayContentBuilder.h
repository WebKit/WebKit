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
#include "LayoutUnits.h"
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
    InlineDisplayContentBuilder(InlineFormattingContext&, const ConstraintsForInlineContent&, const InlineDisplay::Line&, size_t lineIndex);

    InlineDisplay::Boxes build(const LineLayoutResult&, const LineBox&);

private:
    void processNonBidiContent(const LineLayoutResult&, const LineBox&, InlineDisplay::Boxes&);
    void processBidiContent(const LineLayoutResult&, const LineBox&, InlineDisplay::Boxes&);
    void processFloatBoxes(const LineLayoutResult&);
    void collectInkOverflowForInlineBoxes(InlineDisplay::Boxes&);
    void collectInkOverflowForTextDecorations(InlineDisplay::Boxes&);
    void truncateForEllipsisPolicy(LineEndingEllipsisPolicy, const LineLayoutResult&, InlineDisplay::Boxes&);

    void appendTextDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendSoftLineBreakDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendHardLineBreakDisplayBox(const Line::Run&, const InlineRect&, InlineDisplay::Boxes&);
    void appendAtomicInlineLevelDisplayBox(const Line::Run&, const InlineRect& , InlineDisplay::Boxes&);
    void appendRootInlineBoxDisplayBox(const InlineRect&, bool linehasContent, InlineDisplay::Boxes&);
    void appendInlineBoxDisplayBox(const Line::Run&, const InlineLevelBox&, const InlineRect&, bool linehasContent, InlineDisplay::Boxes&);
    void appendSpanningInlineBoxDisplayBox(const Line::Run&, const InlineLevelBox&, const InlineRect&, bool linehasContent, InlineDisplay::Boxes&);
    void appendInlineDisplayBoxAtBidiBoundary(const Box&, InlineDisplay::Boxes&);
    void appendRubyAnnotationBox(const Box& rubyBaseLayoutBox, InlineDisplay::Boxes&);
    void handleInlineBoxEnd(const Line::Run&, const InlineDisplay::Boxes&);
    void applyRubyOverhang(InlineDisplay::Boxes&);

    void setInlineBoxGeometry(const Box&, const InlineRect&, bool isFirstInlineBoxFragment);
    void adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& accumulatedOffset, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree&, InlineDisplay::Boxes&, const LineBox&, const HashMap<const Box*, IsFirstLastIndex>&);
    size_t ensureDisplayBoxForContainer(const ElementBox&, DisplayBoxTree&, AncestorStack&, InlineDisplay::Boxes&);

    InlineRect flipLogicalRectToVisualForWritingModeWithinLine(const InlineRect& logicalRect, const InlineRect& lineLogicalRect, WritingMode) const;
    InlineRect flipRootInlineBoxRectToVisualForWritingMode(const InlineRect& rootInlineBoxLogicalRect, WritingMode) const;
    void setLeftForWritingMode(InlineDisplay::Box&, InlineLayoutUnit logicalRight, WritingMode) const;
    void setRightForWritingMode(InlineDisplay::Box&, InlineLayoutUnit logicalRight, WritingMode) const;
    InlineLayoutPoint movePointHorizontallyForWritingMode(const InlineLayoutPoint& topLeft, InlineLayoutUnit horizontalOffset, WritingMode) const;
    InlineLayoutUnit outsideListMarkerVisualPosition(const ElementBox&) const;
    void setGeometryForBlockLevelOutOfFlowBoxes(const Vector<size_t>& indexList, const LineBox&, const Line::RunList&, const Vector<int32_t>& visualOrderList = { });

    bool isLineFullyTruncatedInBlockDirection() const { return m_lineIsFullyTruncatedInBlockDirection; }

    const ConstraintsForInlineContent& constraints() const { return m_constraints; }
    const ElementBox& root() const { return m_formattingContext.root(); }
    const RenderStyle& rootStyle() const { return m_lineIndex ? root().style() : root().firstLineStyle(); }
    InlineFormattingContext& formattingContext() { return m_formattingContext; }
    const InlineFormattingContext& formattingContext() const { return m_formattingContext; }

private:
    InlineFormattingContext& m_formattingContext;
    const ConstraintsForInlineContent& m_constraints;
    const InlineDisplay::Line& m_displayLine;
    IntSize m_initialContaingBlockSize;
    const size_t m_lineIndex { 0 };
    // FIXME: This should take DisplayLine::isTruncatedInBlockDirection() for non-prefixed line-clamp.
    bool m_lineIsFullyTruncatedInBlockDirection { false };
    bool m_contentHasInkOverflow { false };
    Vector<WTF::Range<size_t>> m_interlinearRubyColumnRangeList;
};

}
}

