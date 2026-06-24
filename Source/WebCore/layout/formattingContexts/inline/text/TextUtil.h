/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#include <WebCore/Font.h>
#include <WebCore/InlineItem.h>
#include <WebCore/InlineLine.h>
#include <WebCore/LayoutUnits.h>
#include <WebCore/TextSpacing.h>
#include <wtf/Range.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

namespace Style {
class ComputedStyle;
}

namespace TextSpacing {
struct SpacingState;
}

struct GlyphOverflow;
class FontCascade;
class TextRun;

namespace Layout {

struct ExpansionInfo;
class InlineTextBox;
class InlineTextItem;

class TextUtil {
public:
    enum class UseTrailingWhitespaceMeasuringOptimization : bool { No, Yes };
    static InlineLayoutUnit width(const InlineTextItem&, const FontCascade&, InlineLayoutUnit contentLogicalLeft);
    static InlineLayoutUnit width(const InlineTextItem&, const FontCascade&, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft, UseTrailingWhitespaceMeasuringOptimization = UseTrailingWhitespaceMeasuringOptimization::Yes, TextSpacing::SpacingState spacingState = { }, GlyphOverflow* = nullptr);
    static InlineLayoutUnit width(const InlineTextBox&, const FontCascade&, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft, UseTrailingWhitespaceMeasuringOptimization = UseTrailingWhitespaceMeasuringOptimization::Yes, TextSpacing::SpacingState spacingState = { }, GlyphOverflow* = nullptr);

    static InlineLayoutUnit trailingWhitespaceWidth(const InlineTextBox&, const FontCascade&, size_t startPosition, size_t endPosition);
    static InlineLayoutUnit singleSpaceWidth(const FontCascade&, bool canUseSimplifiedContentMeasuring);

    using FallbackFontList = SingleThreadWeakHashSet<const Font>;
    enum class IncludeHyphen : bool { No, Yes };
    static FallbackFontList fallbackFontsForText(StringView, const Style::ComputedStyle&, IncludeHyphen);

    struct EnclosingAscentDescent {
        InlineLayoutUnit ascent { 0.f };
        InlineLayoutUnit descent { 0.f };
    };
    enum class ShouldUseSimpleGlyphOverflowCodePath : bool { No, Yes };
    static EnclosingAscentDescent enclosingGlyphBoundsForText(StringView, const Style::ComputedStyle&, ShouldUseSimpleGlyphOverflowCodePath);

    struct WordBreakLeft {
        size_t length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
    };
    static WordBreakLeft breakWord(const InlineTextBox&, size_t start, size_t length, InlineLayoutUnit width, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft, const FontCascade&);
    static WordBreakLeft breakWord(const InlineTextItem&, const FontCascade&, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft);

    static bool mayBreakInBetween(const InlineTextItem& previousInlineItem, const InlineTextItem& nextInlineItem);
    // FIXME: Remove when computeInlineIntrinsicLogicalWidths is all IFC.
    static bool mayBreakInBetween(String previousContent, const Style::ComputedStyle& previousContentStyle, String nextContent, const Style::ComputedStyle& nextContentStyle);
    static unsigned findNextBreakablePosition(CachedLineBreakIteratorFactory&, unsigned startPosition, const Style::ComputedStyle&);
    static TextBreakIterator::LineMode::Behavior NODELETE lineBreakIteratorMode(LineBreak);
    static TextBreakIterator::ContentAnalysis NODELETE contentAnalysis(WordBreak);

    static bool NODELETE shouldPreserveSpacesAndTabs(const Box&);
    static bool NODELETE shouldPreserveNewline(const Box&);
    static bool NODELETE isWrappingAllowed(const Style::ComputedStyle&);
    static bool NODELETE shouldTrailingWhitespaceHang(const Style::ComputedStyle&);

    static bool isStrongDirectionalityCharacter(char32_t);
    static bool containsStrongDirectionalityText(StringView);

    static AtomString ellipsisTextInInlineDirection(bool isHorizontal = true);

    static InlineLayoutUnit hyphenWidth(const Style::ComputedStyle&);

    static size_t firstUserPerceivedCharacterLength(const InlineTextItem&);
    static size_t firstUserPerceivedCharacterLength(const InlineTextBox&, size_t startPosition, size_t length);
    static TextDirection directionForTextContent(StringView);

    static bool hasHangablePunctuationStart(const InlineTextItem&, const Style::ComputedStyle&);
    static float hangablePunctuationStartWidth(const InlineTextItem&, const Style::ComputedStyle&);

    static bool hasHangablePunctuationEnd(const InlineTextItem&, const Style::ComputedStyle&);
    static float hangablePunctuationEndWidth(const InlineTextItem&, const Style::ComputedStyle&);

    static bool hasHangableStopOrCommaEnd(const InlineTextItem&, const Style::ComputedStyle&);
    static float hangableStopOrCommaEndWidth(const InlineTextItem&, const Style::ComputedStyle&);

    static bool canUseSimplifiedTextMeasuring(StringView, const FontCascade&, bool whitespaceIsCollapsed, const Style::ComputedStyle* firstLineStyle);
    static bool hasPositionDependentContentWidth(StringView);

    static char32_t NODELETE lastBaseCharacterFromText(StringView);
};

} // namespace Layout
} // namespace WebCore
