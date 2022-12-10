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

#include "config.h"
#include "TextUtil.h"

#include "BreakLines.h"
#include "FontCascade.h"
#include "InlineTextItem.h"
#include "Latin1TextIterator.h"
#include "LayoutInlineTextBox.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include <unicode/ubidi.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {
namespace Layout {

static inline InlineLayoutUnit spaceWidth(const FontCascade& fontCascade, bool canUseSimplifiedContentMeasuring)
{
    if (canUseSimplifiedContentMeasuring)
        return fontCascade.primaryFont().spaceWidth();
    return fontCascade.widthOfSpaceString();
}

InlineLayoutUnit TextUtil::width(const InlineTextBox& inlineTextBox, const FontCascade& fontCascade, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft, UseTrailingWhitespaceMeasuringOptimization useTrailingWhitespaceMeasuringOptimization)
{
    if (from == to)
        return 0;

    if (inlineTextBox.isCombined())
        return fontCascade.size();

    auto text = inlineTextBox.content();
    ASSERT(to <= text.length());
    auto hasKerningOrLigatures = fontCascade.enableKerning() || fontCascade.requiresShaping();
    // The "non-whitespace" + "whitespace" pattern is very common for inline content and since most of the "non-whitespace" runs end up with
    // their "whitespace" pair on the line (notable exception is when trailing whitespace is trimmed).
    // Including the trailing whitespace here enables us to cut the number of text measures when placing content on the line.
    auto extendedMeasuring = useTrailingWhitespaceMeasuringOptimization == UseTrailingWhitespaceMeasuringOptimization::Yes && hasKerningOrLigatures && to < text.length() && text[to] == space;
    if (extendedMeasuring)
        ++to;
    auto width = 0.f;
    auto useSimplifiedContentMeasuring = inlineTextBox.canUseSimplifiedContentMeasuring();
    if (useSimplifiedContentMeasuring)
        width = fontCascade.widthForSimpleText(StringView(text).substring(from, to - from));
    else {
        WebCore::TextRun run(StringView(text).substring(from, to - from), contentLogicalLeft);
        auto& style = inlineTextBox.style();
        if (!style.collapseWhiteSpace() && style.tabSize())
            run.setTabSize(true, style.tabSize());
        width = fontCascade.width(run);
    }

    if (extendedMeasuring)
        width -= (spaceWidth(fontCascade, useSimplifiedContentMeasuring) + fontCascade.wordSpacing());

    return std::isnan(width) ? 0.0f : std::isinf(width) ? maxInlineLayoutUnit() : width;
}

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, const FontCascade& fontCascade, InlineLayoutUnit contentLogicalLeft)
{
    return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.end(), contentLogicalLeft);
}

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, const FontCascade& fontCascade, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    RELEASE_ASSERT(from >= inlineTextItem.start());
    RELEASE_ASSERT(to <= inlineTextItem.end());

    if (inlineTextItem.isWhitespace()) {
        auto& inlineTextBox = inlineTextItem.inlineTextBox();
        auto useSimplifiedContentMeasuring = inlineTextBox.canUseSimplifiedContentMeasuring();
        auto length = from - to;
        auto singleWhiteSpace = length == 1 || !TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);

        if (singleWhiteSpace) {
            auto width = spaceWidth(fontCascade, useSimplifiedContentMeasuring);
            return std::isnan(width) ? 0.0f : std::isinf(width) ? maxInlineLayoutUnit() : width;
        }
    }
    return width(inlineTextItem.inlineTextBox(), fontCascade, from, to, contentLogicalLeft);
}

InlineLayoutUnit TextUtil::trailingWhitespaceWidth(const InlineTextBox& inlineTextBox, const FontCascade& fontCascade, size_t startPosition, size_t endPosition)
{
    auto text = inlineTextBox.content();
    ASSERT(endPosition > startPosition + 1);
    ASSERT(text[endPosition - 1] == space);
    return width(inlineTextBox, fontCascade, startPosition, endPosition, { }, UseTrailingWhitespaceMeasuringOptimization::Yes) - 
        width(inlineTextBox, fontCascade, startPosition, endPosition - 1, { }, UseTrailingWhitespaceMeasuringOptimization::No);
}

template <typename TextIterator>
static void fallbackFontsForRunWithIterator(HashSet<const Font*>& fallbackFonts, const FontCascade& fontCascade, const TextRun& run, TextIterator& textIterator)
{
    auto isRTL = run.rtl();
    auto isSmallCaps = fontCascade.isSmallCaps();
    auto& primaryFont = fontCascade.primaryFont();

    UChar32 currentCharacter = 0;
    unsigned clusterLength = 0;
    while (textIterator.consume(currentCharacter, clusterLength)) {

        auto addFallbackFontForCharacterIfApplicable = [&](auto character) {
            if (isSmallCaps && character != u_toupper(character))
                character = u_toupper(character);

            auto glyphData = fontCascade.glyphDataForCharacter(character, isRTL);
            if (glyphData.glyph && glyphData.font && glyphData.font != &primaryFont) {
                auto isNonSpacingMark = U_MASK(u_charType(character)) & U_GC_MN_MASK;
                // If we include the synthetic bold expansion, then even zero-width glyphs will have their fonts added.
                if (isNonSpacingMark || glyphData.font->widthForGlyph(glyphData.glyph, Font::SyntheticBoldInclusion::Exclude))
                    fallbackFonts.add(glyphData.font);
            }
        };
        addFallbackFontForCharacterIfApplicable(currentCharacter);
        textIterator.advance(clusterLength);
    }
}

TextUtil::FallbackFontList TextUtil::fallbackFontsForText(StringView textContent, const RenderStyle& style, IncludeHyphen includeHyphen)
{
    TextUtil::FallbackFontList fallbackFonts;

    auto collectFallbackFonts = [&](const auto& textRun) {
        if (textRun.text().isEmpty())
            return;

        if (textRun.is8Bit()) {
            auto textIterator = Latin1TextIterator { textRun.data8(0), 0, textRun.length(), textRun.length() };
            fallbackFontsForRunWithIterator(fallbackFonts, style.fontCascade(), textRun, textIterator);
            return;
        }
        auto textIterator = SurrogatePairAwareTextIterator { textRun.data16(0), 0, textRun.length(), textRun.length() };
        fallbackFontsForRunWithIterator(fallbackFonts, style.fontCascade(), textRun, textIterator);
    };

    if (includeHyphen == IncludeHyphen::Yes)
        collectFallbackFonts(TextRun { StringView(style.hyphenString().string()), { }, { }, ExpansionBehavior::defaultBehavior(), style.direction() });
    collectFallbackFonts(TextRun { textContent, { }, { }, ExpansionBehavior::defaultBehavior(), style.direction() });
    return fallbackFonts;
}

template <typename TextIterator>
static TextUtil::EnclosingAscentDescent enclosingGlyphBoundsForRunWithIterator(const FontCascade& fontCascade, bool isRTL, TextIterator& textIterator)
{
    auto enclosingAscent = std::optional<InlineLayoutUnit> { };
    auto enclosingDescent = std::optional<InlineLayoutUnit> { };
    auto isSmallCaps = fontCascade.isSmallCaps();
    auto& primaryFont = fontCascade.primaryFont();

    UChar32 currentCharacter = 0;
    unsigned clusterLength = 0;
    while (textIterator.consume(currentCharacter, clusterLength)) {

        auto computeTopAndBottomForCharacter = [&](auto character) {
            if (isSmallCaps && character != u_toupper(character))
                character = u_toupper(character);

            auto glyphData = fontCascade.glyphDataForCharacter(character, isRTL);
            auto& font = glyphData.font ? *glyphData.font : primaryFont;
            // FIXME: This may need some adjustment for ComplexTextController. See glyphOrigin.
            auto bounds = font.boundsForGlyph(glyphData.glyph);

            enclosingAscent = std::min(enclosingAscent.value_or(bounds.y()), bounds.y());
            enclosingDescent = std::max(enclosingDescent.value_or(bounds.maxY()), bounds.maxY());
        };
        computeTopAndBottomForCharacter(currentCharacter);
        textIterator.advance(clusterLength);
    }
    return { enclosingAscent.value_or(0.f), enclosingDescent.value_or(0.f) };
}

TextUtil::EnclosingAscentDescent TextUtil::enclosingGlyphBoundsForText(StringView textContent, const RenderStyle& style)
{
    if (textContent.isEmpty())
        return { };

    if (textContent.is8Bit()) {
        auto textIterator = Latin1TextIterator { textContent.characters8(), 0, textContent.length(), textContent.length() };
        return enclosingGlyphBoundsForRunWithIterator(style.fontCascade(), !style.isLeftToRightDirection(), textIterator);
    }
    auto textIterator = SurrogatePairAwareTextIterator { textContent.characters16(), 0, textContent.length(), textContent.length() };
    return enclosingGlyphBoundsForRunWithIterator(style.fontCascade(), !style.isLeftToRightDirection(), textIterator);
}

TextUtil::WordBreakLeft TextUtil::breakWord(const InlineTextItem& inlineTextItem, const FontCascade& fontCascade, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft)
{
    return breakWord(inlineTextItem.inlineTextBox(), inlineTextItem.start(), inlineTextItem.length(), textWidth, availableWidth, contentLogicalLeft, fontCascade);
}

TextUtil::WordBreakLeft TextUtil::breakWord(const InlineTextBox& inlineTextBox, size_t startPosition, size_t length, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft, const FontCascade& fontCascade)
{
    ASSERT(availableWidth >= 0);
    ASSERT(length);
    auto text = inlineTextBox.content();

    if (inlineTextBox.canUseSimpleFontCodePath()) {

        auto findBreakingPositionInSimpleText = [&] {
            auto userPerceivedCharacterBoundaryAlignedIndex = [&] (auto index) -> size_t {
                if (text.is8Bit())
                    return index;
                auto alignedStartIndex = index;
                U16_SET_CP_START(text, startPosition, alignedStartIndex);
                ASSERT(alignedStartIndex >= startPosition);
                return alignedStartIndex;
            };

            auto nextUserPerceivedCharacterIndex = [&] (auto index) -> size_t {
                if (text.is8Bit())
                    return index + 1;
                U16_FWD_1(text, index, length);
                return index;
            };

            auto trySimplifiedBreakingPosition = [&] (auto start) -> std::optional<WordBreakLeft> {
                auto mayUseSimplifiedBreakingPositionForFixedPitch = fontCascade.isFixedPitch() && inlineTextBox.canUseSimplifiedContentMeasuring();
                if (!mayUseSimplifiedBreakingPositionForFixedPitch)
                    return { };
                // FIXME: Check if we could bring webkit.org/b/221581 back for system monospace fonts.
                auto monospaceCharacterWidth = fontCascade.widthOfSpaceString();
                size_t estimatedCharacterCount = floorf(availableWidth / monospaceCharacterWidth);
                auto end = userPerceivedCharacterBoundaryAlignedIndex(std::min(start + estimatedCharacterCount, start + length - 1));
                auto underflowWidth = TextUtil::width(inlineTextBox, fontCascade, start, end, contentLogicalLeft);
                if (underflowWidth > availableWidth || underflowWidth + monospaceCharacterWidth < availableWidth) {
                    // This does not look like a real fixed pitch font. Let's just fall back to regular bisect.
                    // In some edge cases (float precision) using monospaceCharacterWidth here may produce an incorrect off-by-one visual overflow.
                    return { };
                }
                return { WordBreakLeft { end - start, underflowWidth } };
            };
            if (auto leftSide = trySimplifiedBreakingPosition(startPosition))
                return *leftSide;

            auto left = startPosition;
            auto right = left + length - 1;
            // Pathological case of (extremely)long string and narrow lines.
            // Adjust the range so that we can pick a reasonable midpoint.
            auto averageCharacterWidth = InlineLayoutUnit { textWidth / length };
            // Overshot the midpoint so that biscection starts at the left side of the content.
            size_t startOffset = 2 * availableWidth / averageCharacterWidth;
            right = userPerceivedCharacterBoundaryAlignedIndex(std::min(left + startOffset, right));
            // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
            auto leftSideWidth = InlineLayoutUnit { 0 };
            while (left < right) {
                auto middle = userPerceivedCharacterBoundaryAlignedIndex((left + right) / 2);
                ASSERT(middle >= left && middle < right);
                auto endOfMiddleCharacter = nextUserPerceivedCharacterIndex(middle);
                auto width = TextUtil::width(inlineTextBox, fontCascade, startPosition, endOfMiddleCharacter, contentLogicalLeft);
                if (width < availableWidth) {
                    left = endOfMiddleCharacter;
                    leftSideWidth = width;
                } else if (width > availableWidth)
                    right = middle;
                else {
                    right = endOfMiddleCharacter;
                    leftSideWidth = width;
                    break;
                }
            }
            RELEASE_ASSERT(right >= startPosition);
            return WordBreakLeft { right - startPosition, leftSideWidth };
        };
        return findBreakingPositionInSimpleText();
    }

    auto graphemeClusterIterator = NonSharedCharacterBreakIterator { StringView { text }.substring(startPosition, length) };
    auto leftSide = TextUtil::WordBreakLeft { };
    for (auto clusterStartPosition = ubrk_next(graphemeClusterIterator); clusterStartPosition != UBRK_DONE; clusterStartPosition = ubrk_next(graphemeClusterIterator)) {
        auto width = TextUtil::width(inlineTextBox, fontCascade, startPosition, startPosition + clusterStartPosition, contentLogicalLeft);
        if (width > availableWidth)
            return leftSide;
        leftSide = { static_cast<size_t>(clusterStartPosition), width };
    }
    // This content is not supposed to fit availableWidth.
    ASSERT_NOT_REACHED();
    return leftSide;
}

unsigned TextUtil::findNextBreakablePosition(LazyLineBreakIterator& lineBreakIterator, unsigned startPosition, const RenderStyle& style)
{
    auto keepAllWordsForCJK = style.wordBreak() == WordBreak::KeepAll;
    auto breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;

    if (keepAllWordsForCJK) {
        if (breakNBSP)
            return nextBreakablePositionKeepingAllWords(lineBreakIterator, startPosition);
        return nextBreakablePositionKeepingAllWordsIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
        if (breakNBSP)
            return WebCore::nextBreakablePosition(lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (breakNBSP)
        return nextBreakablePositionWithoutShortcut(lineBreakIterator, startPosition);
    return nextBreakablePositionIgnoringNBSPWithoutShortcut(lineBreakIterator, startPosition);
}

bool TextUtil::shouldPreserveSpacesAndTabs(const Box& layoutBox)
{
    // https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = layoutBox.style().whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

bool TextUtil::shouldPreserveNewline(const Box& layoutBox)
{
    auto whitespace = layoutBox.style().whiteSpace();
    // https://www.w3.org/TR/css-text-3/#white-space-property
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces || whitespace == WhiteSpace::PreLine; 
}

bool TextUtil::isWrappingAllowed(const RenderStyle& style)
{
    // Do not try to wrap overflown 'pre' and 'no-wrap' content to next line.
    return style.whiteSpace() != WhiteSpace::Pre && style.whiteSpace() != WhiteSpace::NoWrap;
}

LineBreakIteratorMode TextUtil::lineBreakIteratorMode(LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto:
    case LineBreak::AfterWhiteSpace:
    case LineBreak::Anywhere:
        return LineBreakIteratorMode::Default;
    case LineBreak::Loose:
        return LineBreakIteratorMode::Loose;
    case LineBreak::Normal:
        return LineBreakIteratorMode::Normal;
    case LineBreak::Strict:
        return LineBreakIteratorMode::Strict;
    }
    ASSERT_NOT_REACHED();
    return LineBreakIteratorMode::Default;
}

bool TextUtil::containsStrongDirectionalityText(StringView text)
{
    if (text.is8Bit())
        return false;

    auto length = text.length();
    for (size_t position = 0; position < length;) {
        UChar32 character;
        U16_NEXT(text.characters16(), position, length, character);

        auto bidiCategory = u_charDirection(character);
        bool hasBidiContent = bidiCategory == U_RIGHT_TO_LEFT
            || bidiCategory == U_RIGHT_TO_LEFT_ARABIC
            || bidiCategory == U_RIGHT_TO_LEFT_EMBEDDING
            || bidiCategory == U_RIGHT_TO_LEFT_OVERRIDE
            || bidiCategory == U_LEFT_TO_RIGHT_EMBEDDING
            || bidiCategory == U_LEFT_TO_RIGHT_OVERRIDE
            || bidiCategory == U_POP_DIRECTIONAL_FORMAT;
        if (hasBidiContent)
            return true;
    }

    return false;
}

size_t TextUtil::firstUserPerceivedCharacterLength(const InlineTextBox& inlineTextBox, size_t startPosition, size_t length)
{
    auto textContent = inlineTextBox.content();
    RELEASE_ASSERT(!textContent.isEmpty());

    if (textContent.is8Bit())
        return 1;
    if (inlineTextBox.canUseSimpleFontCodePath()) {
        UChar32 character;
        size_t endOfCodePoint = startPosition;
        U16_NEXT(textContent.characters16(), endOfCodePoint, textContent.length(), character);
        ASSERT(endOfCodePoint > startPosition);
        return endOfCodePoint - startPosition;
    }
    auto graphemeClustersIterator = NonSharedCharacterBreakIterator { textContent };
    auto nextPosition = ubrk_following(graphemeClustersIterator, startPosition);
    if (nextPosition == UBRK_DONE)
        return length;
    return nextPosition - startPosition;
}

size_t TextUtil::firstUserPerceivedCharacterLength(const InlineTextItem& inlineTextItem)
{
    return firstUserPerceivedCharacterLength(inlineTextItem.inlineTextBox(), inlineTextItem.start(), inlineTextItem.length());
}

TextDirection TextUtil::directionForTextContent(StringView content)
{
    if (content.is8Bit())
        return TextDirection::LTR;
    return ubidi_getBaseDirection(content.characters16(), content.length()) == UBIDI_RTL ? TextDirection::RTL : TextDirection::LTR;
}

TextRun TextUtil::ellipsisTextRun(bool isHorizontal)
{
    if (isHorizontal) {
        static MainThreadNeverDestroyed<const AtomString> horizontalEllipsisStr(&horizontalEllipsis, 1);
        return TextRun { horizontalEllipsisStr->string() };
    }
    static MainThreadNeverDestroyed<const AtomString> verticalEllipsisStr(&verticalEllipsis, 1);
    return TextRun { verticalEllipsisStr->string() };
}


}
}
