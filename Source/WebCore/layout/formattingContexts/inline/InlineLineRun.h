/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#include <WebCore/InlineDisplayBox.h>
#include <WebCore/InlineItem.h>
#include <WebCore/InlineLineTypes.h>
#include <WebCore/InlineTextItem.h>
#include <WebCore/StyleComputedStyle.h>
#include <unicode/ubidi.h>
#include <wtf/Markable.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

class InlineContentAligner;
class InlineFormattingContext;
class InlineSoftLineBreakItem;
class Line;
class RubyFormattingContext;

enum class LineShapingBoundary : uint8_t { NotApplicable, Start, Inside, End };

struct LineRun {
    enum class Type : uint8_t {
        Text,
        NonBreakingSpace,
        WordSeparator,
        HardLineBreak,
        SoftLineBreak,
        WordBreakOpportunity,
        AtomicInlineBox,
        ListMarker,
        InlineBoxStart,
        InlineBoxEnd,
        LineSpanningInlineBoxStart,
        OutOfFlow,
        Block
    };

    bool isText() const { return m_type == Type::Text || isWordSeparator() || isNonBreakingSpace(); }
    bool isNonBreakingSpace() const { return m_type == Type::NonBreakingSpace; }
    bool isWordSeparator() const { return m_type == Type::WordSeparator; }
    bool isAtomicInlineBox() const { return m_type == Type::AtomicInlineBox; }
    bool isListMarker() const { return m_type == Type::ListMarker; }
    bool isListMarkerOrItsContent() const;
    bool isLineBreak() const { return isHardLineBreak() || isSoftLineBreak(); }
    bool isSoftLineBreak() const  { return m_type == Type::SoftLineBreak; }
    bool isHardLineBreak() const { return m_type == Type::HardLineBreak; }
    bool isWordBreakOpportunity() const { return m_type == Type::WordBreakOpportunity; }
    bool isInlineBox() const { return isInlineBoxStart() || isLineSpanningInlineBoxStart() || isInlineBoxEnd(); }
    bool isInlineBoxStart() const { return m_type == Type::InlineBoxStart; }
    bool isLineSpanningInlineBoxStart() const { return m_type == Type::LineSpanningInlineBoxStart; }
    bool isInlineBoxEnd() const { return m_type == Type::InlineBoxEnd; }
    bool isOutOfFlow() const { return m_type == Type::OutOfFlow; }
    bool isBlock() const { return m_type == Type::Block; }

    bool isContentful() const { return (isText() && textContent().length) || isAtomicInlineBox() || isLineBreak() || isListMarker() || isBlock(); }
    static bool isContentfulOrHasDecoration(const LineRun&, const InlineFormattingContext&);

    const Box& layoutBox() const { return *m_layoutBox; }
    struct Text {
        size_t start { 0 };
        size_t length { 0 };
        bool needsHyphen { false };
    };
    const Text& textContent() const LIFETIME_BOUND { return m_textContent; }

    InlineLayoutUnit logicalWidth() const { return m_logicalWidth; }
    InlineLayoutUnit logicalLeft() const { return m_logicalLeft; }
    InlineLayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

    const InlineDisplay::Box::Expansion& expansion() const LIFETIME_BOUND { return m_expansion; }

    bool hasTrailingWhitespace() const { return m_trailingWhitespace.type != TrailingWhitespace::Type::NotApplicable; }
    InlineLayoutUnit trailingWhitespaceWidth() const { return m_trailingWhitespace.width; }
    bool isWhitespaceOnly() const { return hasTrailingWhitespace() && m_trailingWhitespace.length == m_textContent.length; }

    struct GlyphOverflow {
        bool isEmpty() const { return !top && !bottom; }

        uint8_t top : 5 { 0 };
        uint8_t bottom : 3 { 0 };
    };
    GlyphOverflow glyphOverflow() const { return m_glyphOverflow; }

    inline TextDirection inlineDirection() const;
    InlineLayoutUnit LEGACY_NODELETE letterSpacing() const;
    bool LEGACY_NODELETE hasTextCombine() const;
    InlineLayoutUnit textSpacingAdjustment() const { return m_textSpacingAdjustment; }

    UBiDiLevel bidiLevel() const { return m_bidiLevel; }

    bool isShapingBoundaryStart() const { return m_shapingBoundary == LineShapingBoundary::Start; }
    bool isShapingBoundaryEnd() const { return m_shapingBoundary == LineShapingBoundary::End; }
    bool isInsideShapingBoundary() const { return m_shapingBoundary == LineShapingBoundary::Inside; }
    bool isShapingBoundary() const { return m_shapingBoundary != LineShapingBoundary::NotApplicable; }

    // FIXME: Maybe add create functions intead?
    LineRun(const InlineItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalLeft);
    LineRun(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f);
    LineRun(const InlineTextItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f, std::optional<LineShapingBoundary> = std::nullopt);

private:
    friend class Line;
    friend class InlineContentAligner;
    friend class RubyFormattingContext;

    LineRun(const InlineSoftLineBreakItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalLeft);
    LineRun(const InlineItem&, const Style::ComputedStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment = 0.f);

    const Style::ComputedStyle& style() const LIFETIME_BOUND { return m_style; }
    void expand(const InlineTextItem&, InlineLayoutUnit logicalWidth);
    void moveHorizontally(InlineLayoutUnit offset) { m_logicalLeft += offset; }
    void shrinkHorizontally(InlineLayoutUnit width) { m_logicalWidth -= width; }
    void setExpansion(InlineDisplay::Box::Expansion expansion) { m_expansion = expansion; }
    void setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth);
    void setBidiLevel(UBiDiLevel bidiLevel) { m_bidiLevel = bidiLevel; }

    struct TrailingWhitespace {
        enum class Type : uint8_t {
            NotApplicable,
            NotCollapsible,
            Collapsible,
            Collapsed
        };
        Type type { Type::NotApplicable };
        size_t length : 24 { 0 };
        InlineLayoutUnit width { 0.f };
    };
    bool hasCollapsibleTrailingWhitespace() const { return hasTrailingWhitespace() && (m_trailingWhitespace.type == TrailingWhitespace::Type::Collapsible || hasCollapsedTrailingWhitespace()); }
    bool hasCollapsedTrailingWhitespace() const { return hasTrailingWhitespace() && m_trailingWhitespace.type == TrailingWhitespace::Type::Collapsed; }
    static std::optional<TrailingWhitespace::Type> LEGACY_NODELETE trailingWhitespaceType(const InlineTextItem&);
    InlineLayoutUnit removeTrailingWhitespace();

    std::optional<LineRun> LEGACY_NODELETE detachTrailingWhitespace();

    bool LEGACY_NODELETE hasTrailingLetterSpacing() const;
    InlineLayoutUnit LEGACY_NODELETE trailingLetterSpacing() const;
    InlineLayoutUnit LEGACY_NODELETE removeTrailingLetterSpacing();

    // Members are ordered by descending alignment to minimize padding.
    // 8-byte aligned:
    TrailingWhitespace m_trailingWhitespace { };
    Markable<size_t> m_lastNonWhitespaceContentStart { };
    const Box* m_layoutBox { nullptr };
    const Style::ComputedStyle& m_style;
    Text m_textContent;
    // 4-byte aligned:
    InlineLayoutUnit m_logicalLeft { 0 };
    InlineLayoutUnit m_logicalWidth { 0 };
    InlineLayoutUnit m_textSpacingAdjustment { 0 };
    InlineDisplay::Box::Expansion m_expansion;
    // 1-byte:
    Type m_type { Type::Text };
    LineShapingBoundary m_shapingBoundary { LineShapingBoundary::NotApplicable };
    UBiDiLevel m_bidiLevel { UBIDI_DEFAULT_LTR };
    GlyphOverflow m_glyphOverflow;
};

using LineRunList = Vector<LineRun, 1>;

struct LineResult {
    LineRunList runs;
    InlineLayoutUnit contentLogicalWidth { 0.f };
    InlineLayoutUnit contentLogicalRight { 0.f };
    bool isContentful { false };
    bool isHangingTrailingContentWhitespace { false };
    InlineLayoutUnit hangingTrailingContentWidth { 0.f };
    InlineLayoutUnit hangablePunctuationStartWidth { 0.f };
    bool contentNeedsBidiReordering { false };
    size_t nonSpanningInlineLevelBoxCount { 0 };
};

inline void LineRun::setNeedsHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    ASSERT(isText());
    m_textContent.needsHyphen = true;
    m_logicalWidth += hyphenLogicalWidth;
}

inline TextDirection LineRun::inlineDirection() const
{
    return m_style.writingMode().bidiDirection();
}

}
}
