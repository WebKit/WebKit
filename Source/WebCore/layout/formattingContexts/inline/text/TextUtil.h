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

#include "Font.h"
#include "InlineItem.h"
#include "InlineLine.h"
#include "LayoutUnits.h"
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

class RenderStyle;
class TextRun;

namespace Layout {

class InlineTextBox;
class InlineTextItem;

class TextUtil {
public:
    static InlineLayoutUnit width(const InlineTextItem&, const FontCascade&, InlineLayoutUnit contentLogicalLeft);
    static InlineLayoutUnit width(const InlineTextItem&, const FontCascade&, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft);

    enum class UseTrailingWhitespaceMeasuringOptimization : uint8_t { Yes, No };
    static InlineLayoutUnit width(const InlineTextBox&, const FontCascade&, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft, UseTrailingWhitespaceMeasuringOptimization = UseTrailingWhitespaceMeasuringOptimization::Yes);

    static InlineLayoutUnit trailingWhitespaceWidth(const InlineTextBox&, const FontCascade&, size_t startPosition, size_t endPosition);

    using FallbackFontList = HashSet<const Font*>;
    enum class IncludeHyphen : uint8_t { No, Yes };
    static FallbackFontList fallbackFontsForText(StringView, const RenderStyle&, IncludeHyphen);

    struct EnclosingAscentDescent {
        InlineLayoutUnit ascent { 0.f };
        InlineLayoutUnit descent { 0.f };
    };
    static EnclosingAscentDescent enclosingGlyphBoundsForText(StringView, const RenderStyle&);

    struct WordBreakLeft {
        size_t length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
    };
    static WordBreakLeft breakWord(const InlineTextBox&, size_t start, size_t length, InlineLayoutUnit width, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft, const FontCascade&);
    static WordBreakLeft breakWord(const InlineTextItem&, const FontCascade&, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft);

    static unsigned findNextBreakablePosition(LazyLineBreakIterator&, unsigned startPosition, const RenderStyle&);
    static LineBreakIteratorMode lineBreakIteratorMode(LineBreak);

    static bool shouldPreserveSpacesAndTabs(const Box&);
    static bool shouldPreserveNewline(const Box&);
    static bool isWrappingAllowed(const RenderStyle&);
    static bool containsStrongDirectionalityText(StringView);

    static TextRun ellipsisTextRun(bool isHorizontal = true);

    static size_t firstUserPerceivedCharacterLength(const InlineTextItem&);
    static size_t firstUserPerceivedCharacterLength(const InlineTextBox&, size_t startPosition, size_t length);
    static TextDirection directionForTextContent(StringView);

    static bool hasHangablePunctuationStart(const InlineTextItem&, const RenderStyle&);
    static float hangablePunctuationStartWidth(const InlineTextItem&, const RenderStyle&);

    static bool hasHangablePunctuationEnd(const InlineTextItem&, const RenderStyle&);
    static float hangablePunctuationEndWidth(const InlineTextItem&, const RenderStyle&);

    static bool hasHangableStopOrCommaEnd(const InlineTextItem&, const RenderStyle&);
    static float hangableStopOrCommaEndWidth(const InlineTextItem&, const RenderStyle&);

};

}
}
