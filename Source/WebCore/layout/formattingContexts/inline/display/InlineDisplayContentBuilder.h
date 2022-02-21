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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineLineBuilder.h"
#include "LayoutUnits.h"

namespace WebCore {
namespace Layout {

struct AncestorStack;
class ContainerBox;
struct DisplayBoxTree;
struct IsFirstLastIndex;
class InlineFormattingState;
class LineBox;

class InlineDisplayContentBuilder {
public:
    InlineDisplayContentBuilder(const ContainerBox& formattingContextRoot, InlineFormattingState&);

    DisplayBoxes build(const LineBuilder::LineContent&, const LineBox&, const InlineDisplay::Line&, const size_t lineIndex);

    static void computeIsFirstIsLastBoxForInlineContent(DisplayBoxes&);

private:
    void processNonBidiContent(const LineBuilder::LineContent&, const LineBox&, const InlineDisplay::Line&, DisplayBoxes&);
    void processBidiContent(const LineBuilder::LineContent&, const LineBox&, const InlineDisplay::Line&, DisplayBoxes&);
    void processOverflownRunsForEllipsis(DisplayBoxes&, InlineLayoutUnit lineBoxRight);
    void collectInkOverflowForInlineBoxes(DisplayBoxes&);

    void appendTextDisplayBox(const Line::Run&, const InlineRect&, DisplayBoxes&);
    void appendSoftLineBreakDisplayBox(const Line::Run&, const InlineRect&, DisplayBoxes&);
    void appendHardLineBreakDisplayBox(const Line::Run&, const InlineRect&, DisplayBoxes&);
    void appendAtomicInlineLevelDisplayBox(const Line::Run&, const InlineRect& , DisplayBoxes&);
    void appendInlineBoxDisplayBox(const Line::Run&, const InlineLevelBox&, const InlineRect&, bool linehasContent, DisplayBoxes&);
    void appendSpanningInlineBoxDisplayBox(const Line::Run&, const InlineLevelBox&, const InlineRect&, DisplayBoxes&);
    void appendInlineDisplayBoxAtBidiBoundary(const Box&, DisplayBoxes&);

    void setInlineBoxGeometry(const Box&, const InlineRect&, bool isFirstInlineBoxFragment);
    void adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& accumulatedOffset, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree&, DisplayBoxes&, const LineBox&, const HashMap<const Box*, IsFirstLastIndex>&);
    size_t ensureDisplayBoxForContainer(const ContainerBox&, DisplayBoxTree&, AncestorStack&, DisplayBoxes&);

    InlineRect flipLogicalRectToVisualForWritingModeWithinLine(const InlineRect& logicalRect, const InlineRect& lineLogicalRect, WritingMode) const;
    InlineRect flipLogicalRectToVisualForWritingMode(const InlineRect& logicalRect, WritingMode) const;
    InlineRect flipRootInlineBoxRectToVisualForWritingMode(const InlineRect& rootInlineBoxLogicalRect, const InlineDisplay::Line&, WritingMode) const;
    void setLeftForWritingMode(InlineDisplay::Box&, InlineLayoutUnit logicalRight, WritingMode) const;
    void setRightForWritingMode(InlineDisplay::Box&, InlineLayoutUnit logicalRight, WritingMode) const;
    InlineLayoutPoint movePointHorizontallyForWritingMode(const InlineLayoutPoint& topLeft, InlineLayoutUnit horizontalOffset, WritingMode) const;

    const ContainerBox& root() const { return m_formattingContextRoot; }
    InlineFormattingState& formattingState() const { return m_formattingState; } 

    const ContainerBox& m_formattingContextRoot;
    InlineFormattingState& m_formattingState;
    size_t m_lineIndex { 0 };
    bool m_contentHasInkOverflow { false };
};

}
}

#endif
