/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

namespace WebCore {
namespace Layout {

class Line;
struct InlineItemRange;

class RubyFormattingContext {
public:
    RubyFormattingContext(const InlineFormattingContext& parentFormattingContext);

    struct InlineLayoutResult {
        InlineContentBreaker::IsEndOfLine isEndOfLine { InlineContentBreaker::IsEndOfLine::No };
        size_t committedCount { 0 };
    };
    InlineLayoutResult layoutInlineAxis(const InlineItemRange&, const InlineItems&, Line&, InlineLayoutUnit availableWidth);

    struct OverUnder {
        InlineLayoutUnit over { 0.f };
        InlineLayoutUnit under { 0.f };
    };
    OverUnder annotationVerticalExtent(const Box& rubyBaseLayoutBox);
    InlineLayoutPoint annotationPosition(const Box& rubyBaseLayoutBox);

    InlineLayoutUnit overhangForAnnotationBefore(const Box& rubyBaseLayoutBox, size_t rubyBaseContentStartIndex, const InlineDisplay::Boxes&);
    InlineLayoutUnit overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, size_t rubyBaseContentEndIndex, const InlineDisplay::Boxes&);

    static std::optional<size_t> nextWrapOpportunity(size_t inlineItemIndex, std::optional<size_t> previousInlineItemIndex, const InlineItemRange&, const InlineItems&);

private:
    size_t layoutRubyBaseInlineAxis(Line&, const Box& rubyBaseLayoutBox, size_t rubyBaseContentStart, const InlineItems&);
    void applyRubyAlign(Line&, WTF::Range<size_t> baseRunRange, const Box& rubyBaseLayoutBox, InlineLayoutUnit baseContentLogicalWidth);
    std::optional<bool> annotationOverlapCheck(const InlineDisplay::Box&, const InlineLayoutRect& overhangingRect) const;
    void placeRubyContent(WTF::Range<size_t> candidateRange, const InlineItems&, Line&);
    InlineLayoutUnit logicaWidthForRubyRange(WTF::Range<size_t> candidateRange, const InlineItems&, InlineLayoutUnit lineContentLogicalRight) const;

    const InlineFormattingContext& parentFormattingContext() const { return m_parentFormattingContext; }

private:
    const InlineFormattingContext& m_parentFormattingContext;
};

}
}
