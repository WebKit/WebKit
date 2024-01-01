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

#include "config.h"
#include "TextUtil.h"

#include "BreakLines.h"
#include "FontCascade.h"
#include "InlineLineTypes.h"
#include "InlineTextItem.h"
#include "Latin1TextIterator.h"
#include "LayoutInlineTextBox.h"
#include "RenderBox.h"
#include "RenderStyleInlines.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include "WidthIterator.h"
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
    if (useSimplifiedContentMeasuring) {
        auto view = StringView(text).substring(from, to - from);
        if (fontCascade.canTakeFixedPitchFastContentMeasuring())
            width = fontCascade.widthForSimpleTextWithFixedPitch(view, inlineTextBox.style().collapseWhiteSpace());
        else
            width = fontCascade.widthForSimpleText(view);
    } else {
        auto& style = inlineTextBox.style();
        auto directionalOverride = style.unicodeBidi() == UnicodeBidi::Override;
        auto run = WebCore::TextRun { StringView(text).substring(from, to - from), contentLogicalLeft, { }, ExpansionBehavior::defaultBehavior(), directionalOverride ? style.direction() : TextDirection::LTR, directionalOverride };
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

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, const FontCascade& fontCascade, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft, UseTrailingWhitespaceMeasuringOptimization useTrailingWhitespaceMeasuringOptimization)
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
    return width(inlineTextItem.inlineTextBox(), fontCascade, from, to, contentLogicalLeft, useTrailingWhitespaceMeasuringOptimization);
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
static void fallbackFontsForRunWithIterator(SingleThreadWeakHashSet<const Font>& fallbackFonts, const FontCascade& fontCascade, const TextRun& run, TextIterator& textIterator)
{
    auto isRTL = run.rtl();
    auto isSmallCaps = fontCascade.isSmallCaps();
    auto& primaryFont = fontCascade.primaryFont();

    char32_t currentCharacter = 0;
    unsigned clusterLength = 0;
    while (textIterator.consume(currentCharacter, clusterLength)) {

        auto addFallbackFontForCharacterIfApplicable = [&](auto character) {
            if (isSmallCaps)
                character = u_toupper(character);

            auto glyphData = fontCascade.glyphDataForCharacter(character, isRTL);
            if (glyphData.glyph && glyphData.font && glyphData.font != &primaryFont) {
                auto isNonSpacingMark = U_MASK(u_charType(character)) & U_GC_MN_MASK;

                // https://drafts.csswg.org/css-text-3/#white-space-processing
                // "Unsupported Default_ignorable characters must be ignored for text rendering."
                auto isIgnored = isDefaultIgnorableCodePoint(character);

                // If we include the synthetic bold expansion, then even zero-width glyphs will have their fonts added.
                if (isNonSpacingMark || glyphData.font->widthForGlyph(glyphData.glyph, Font::SyntheticBoldInclusion::Exclude))
                    if (!isIgnored)
                        fallbackFonts.add(*glyphData.font);
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

    char32_t currentCharacter = 0;
    unsigned clusterLength = 0;
    while (textIterator.consume(currentCharacter, clusterLength)) {

        auto computeTopAndBottomForCharacter = [&](auto character) {
            if (isSmallCaps)
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
                U16_FWD_1(text, index, startPosition + length);
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

bool TextUtil::mayBreakInBetween(const InlineTextItem& previousInlineItem, const InlineTextItem& nextInlineItem)
{
    // Check if these 2 adjacent non-whitespace inline items are connected at a breakable position.
    ASSERT(!previousInlineItem.isWhitespace() && !nextInlineItem.isWhitespace());

    auto previousContent = previousInlineItem.inlineTextBox().content();
    auto nextContent = nextInlineItem.inlineTextBox().content();
    // Now we need to collect at least 3 adjacent characters to be able to make a decision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    if (!previousContent.is8Bit()) {
        // FIXME: Remove this workaround when we move over to a better way of handling prior-context with Unicode.
        // See the templated CharacterType in nextBreakablePosition for last and lastlast characters.
        nextContent.convertTo16Bit();
    }
    auto& previousContentStyle = previousInlineItem.style();
    auto& nextContentStyle = nextInlineItem.style();
    auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { nextContent, nextContentStyle.computedLocale(), TextUtil::lineBreakIteratorMode(nextContentStyle.lineBreak()), TextUtil::contentAnalysis(nextContentStyle.wordBreak()) };
    auto previousContentLength = previousContent.length();
    // FIXME: We should look into the entire uncommitted content for more text context.
    UChar lastCharacter = previousContentLength ? previousContent[previousContentLength - 1] : 0;
    if (lastCharacter == softHyphen && previousContentStyle.hyphens() == Hyphens::None)
        return false;
    UChar secondToLastCharacter = previousContentLength > 1 ? previousContent[previousContentLength - 2] : 0;
    lineBreakIteratorFactory.priorContext().set({ secondToLastCharacter, lastCharacter });
    // Now check if we can break right at the inline item boundary.
    // With the [ex-ample], findNextBreakablePosition should return the startPosition (0).
    // FIXME: Check if there's a more correct way of finding breaking opportunities.
    return !findNextBreakablePosition(lineBreakIteratorFactory, 0, nextContentStyle);
}

unsigned TextUtil::findNextBreakablePosition(CachedLineBreakIteratorFactory& lineBreakIteratorFactory, unsigned startPosition, const RenderStyle& style)
{
    auto keepAllWordsForCJK = style.wordBreak() == WordBreak::KeepAll;
    auto breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;

    if (keepAllWordsForCJK) {
        if (breakNBSP)
            return nextBreakablePositionKeepingAllWords(lineBreakIteratorFactory, startPosition);
        return nextBreakablePositionKeepingAllWordsIgnoringNBSP(lineBreakIteratorFactory, startPosition);
    }

    if (lineBreakIteratorFactory.mode() == TextBreakIterator::LineMode::Behavior::Default) {
        if (breakNBSP)
            return WebCore::nextBreakablePosition(lineBreakIteratorFactory, startPosition);
        return nextBreakablePositionIgnoringNBSP(lineBreakIteratorFactory, startPosition);
    }

    if (breakNBSP)
        return nextBreakablePositionWithoutShortcut(lineBreakIteratorFactory, startPosition);
    return nextBreakablePositionIgnoringNBSPWithoutShortcut(lineBreakIteratorFactory, startPosition);
}

bool TextUtil::shouldPreserveSpacesAndTabs(const Box& layoutBox)
{
    // https://www.w3.org/TR/css-text-4/#white-space-collapsing
    auto whitespaceCollapse = layoutBox.style().whiteSpaceCollapse();
    return whitespaceCollapse == WhiteSpaceCollapse::Preserve || whitespaceCollapse == WhiteSpaceCollapse::BreakSpaces;
}

bool TextUtil::shouldPreserveNewline(const Box& layoutBox)
{
    // https://www.w3.org/TR/css-text-4/#white-space-collapsing
    auto whitespaceCollapse = layoutBox.style().whiteSpaceCollapse();
    return whitespaceCollapse == WhiteSpaceCollapse::Preserve || whitespaceCollapse == WhiteSpaceCollapse::PreserveBreaks || whitespaceCollapse == WhiteSpaceCollapse::BreakSpaces;
}

bool TextUtil::isWrappingAllowed(const RenderStyle& style)
{
    // https://www.w3.org/TR/css-text-4/#text-wrap
    return style.textWrapMode() != TextWrapMode::NoWrap;
}

bool TextUtil::shouldTrailingWhitespaceHang(const RenderStyle& style)
{
    // https://www.w3.org/TR/css-text-4/#white-space-phase-2
    return style.whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve && style.textWrapMode() != TextWrapMode::NoWrap;
}

TextBreakIterator::LineMode::Behavior TextUtil::lineBreakIteratorMode(LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto:
    case LineBreak::AfterWhiteSpace:
    case LineBreak::Anywhere:
        return TextBreakIterator::LineMode::Behavior::Default;
    case LineBreak::Loose:
        return TextBreakIterator::LineMode::Behavior::Loose;
    case LineBreak::Normal:
        return TextBreakIterator::LineMode::Behavior::Normal;
    case LineBreak::Strict:
        return TextBreakIterator::LineMode::Behavior::Strict;
    }
    ASSERT_NOT_REACHED();
    return TextBreakIterator::LineMode::Behavior::Default;
}

TextBreakIterator::ContentAnalysis TextUtil::contentAnalysis(WordBreak wordBreak)
{
    switch (wordBreak) {
    case WordBreak::Normal:
    case WordBreak::BreakAll:
    case WordBreak::KeepAll:
    case WordBreak::BreakWord:
        return TextBreakIterator::ContentAnalysis::Mechanical;
    case WordBreak::AutoPhrase:
        return TextBreakIterator::ContentAnalysis::Linguistic;
    }
    return TextBreakIterator::ContentAnalysis::Mechanical;
}

bool TextUtil::isStrongDirectionalityCharacter(char32_t character)
{
    auto bidiCategory = u_charDirection(character);
    return bidiCategory == U_RIGHT_TO_LEFT
        || bidiCategory == U_RIGHT_TO_LEFT_ARABIC
        || bidiCategory == U_RIGHT_TO_LEFT_EMBEDDING
        || bidiCategory == U_RIGHT_TO_LEFT_OVERRIDE
        || bidiCategory == U_LEFT_TO_RIGHT_EMBEDDING
        || bidiCategory == U_LEFT_TO_RIGHT_OVERRIDE
        || bidiCategory == U_POP_DIRECTIONAL_FORMAT;
}

bool TextUtil::containsStrongDirectionalityText(StringView text)
{
    if (text.is8Bit())
        return false;

    auto length = text.length();
    for (size_t position = 0; position < length;) {
        char32_t character;
        U16_NEXT(text.characters16(), position, length, character);
        if (isStrongDirectionalityCharacter(character))
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
        char32_t character;
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
    auto length = firstUserPerceivedCharacterLength(inlineTextItem.inlineTextBox(), inlineTextItem.start(), inlineTextItem.length());
    return std::min<size_t>(inlineTextItem.length(), length);
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

bool TextUtil::hasHangablePunctuationStart(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!inlineTextItem.length() || !style.hangingPunctuation().contains(HangingPunctuation::First))
        return false;
    auto leadingCharacter = inlineTextItem.inlineTextBox().content()[inlineTextItem.start()];
    return U_GET_GC_MASK(leadingCharacter) & (U_GC_PS_MASK | U_GC_PI_MASK | U_GC_PF_MASK);
}

float TextUtil::hangablePunctuationStartWidth(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!hasHangablePunctuationStart(inlineTextItem, style))
        return { };
    ASSERT(inlineTextItem.length());
    auto leadingPosition = inlineTextItem.start();
    return width(inlineTextItem, style.fontCascade(), leadingPosition, leadingPosition + 1, { });
}

bool TextUtil::hasHangablePunctuationEnd(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!inlineTextItem.length() || !style.hangingPunctuation().contains(HangingPunctuation::Last))
        return false;
    auto trailingCharacter = inlineTextItem.inlineTextBox().content()[inlineTextItem.end() - 1];
    return U_GET_GC_MASK(trailingCharacter) & (U_GC_PE_MASK | U_GC_PI_MASK | U_GC_PF_MASK);
}

float TextUtil::hangablePunctuationEndWidth(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!hasHangablePunctuationEnd(inlineTextItem, style))
        return { };
    ASSERT(inlineTextItem.length());
    auto trailingPosition = inlineTextItem.end() - 1;
    return width(inlineTextItem, style.fontCascade(), trailingPosition, trailingPosition + 1, { });
}

bool TextUtil::hasHangableStopOrCommaEnd(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!inlineTextItem.length() || !style.hangingPunctuation().containsAny({ HangingPunctuation::AllowEnd, HangingPunctuation::ForceEnd }))
        return false;
    auto trailingPosition = inlineTextItem.end() - 1;
    auto trailingCharacter = inlineTextItem.inlineTextBox().content()[trailingPosition];
    auto isHangableStopOrComma = trailingCharacter == 0x002C
        || trailingCharacter == 0x002E || trailingCharacter == 0x060C
        || trailingCharacter == 0x06D4 || trailingCharacter == 0x3001
        || trailingCharacter == 0x3002 || trailingCharacter == 0xFF0C
        || trailingCharacter == 0xFF0E || trailingCharacter == 0xFE50
        || trailingCharacter == 0xFE51 || trailingCharacter == 0xFE52
        || trailingCharacter == 0xFF61 || trailingCharacter == 0xFF64;
    return isHangableStopOrComma;
}

float TextUtil::hangableStopOrCommaEndWidth(const InlineTextItem& inlineTextItem, const RenderStyle& style)
{
    if (!hasHangableStopOrCommaEnd(inlineTextItem, style))
        return { };
    ASSERT(inlineTextItem.length());
    auto trailingPosition = inlineTextItem.end() - 1;
    return width(inlineTextItem, style.fontCascade(), trailingPosition, trailingPosition + 1, { });
}

template<typename CharacterType>
static bool canUseSimplifiedTextMeasuringForCharacters(std::span<const CharacterType> characters, const FontCascade& fontCascade, const Font& primaryFont, bool whitespaceIsCollapsed)
{
    auto* rawCharacters = characters.data();
    for (unsigned i = 0; i < characters.size(); ++i) {
        auto character = rawCharacters[i]; // Not using characters[i] to bypass the bounds check.
        if (!WidthIterator::characterCanUseSimplifiedTextMeasuring(character, whitespaceIsCollapsed))
            return false;
        auto glyphData = fontCascade.glyphDataForCharacter(character, false);
        if (!glyphData.isValid() || glyphData.font != &primaryFont)
            return false;
    }
    return true;
}

bool TextUtil::canUseSimplifiedTextMeasuring(StringView textContent, const RenderStyle& style, const RenderStyle* firstLineStyle)
{
    ASSERT(textContent.is8Bit() || FontCascade::characterRangeCodePath(textContent.characters16(), textContent.length()) == FontCascade::CodePath::Simple);
    // FIXME: All these checks should be more fine-grained at the inline item level.
    auto& fontCascade = style.fontCascade();
    if (fontCascade.wordSpacing() || fontCascade.letterSpacing())
        return false;

    // Additional check on the font codepath.
    auto run = TextRun { textContent };
    run.setCharacterScanForCodePath(false);
    if (fontCascade.codePath(run) != FontCascade::CodePath::Simple)
        return false;

    if (firstLineStyle && fontCascade != firstLineStyle->fontCascade())
        return false;

    auto& primaryFont = fontCascade.primaryFont();
    if (primaryFont.syntheticBoldOffset())
        return false;

    auto whitespaceIsCollapsed = style.collapseWhiteSpace();
    if (textContent.is8Bit())
        return canUseSimplifiedTextMeasuringForCharacters(textContent.span8(), fontCascade, primaryFont, whitespaceIsCollapsed);
    return canUseSimplifiedTextMeasuringForCharacters(textContent.span16(), fontCascade, primaryFont, whitespaceIsCollapsed);
}

bool TextUtil::hasPositionDependentContentWidth(StringView textContent)
{
    for (char32_t character : StringView(textContent).codePoints()) {
        if (character == tabCharacter)
            return true;
    }
    return false;
}

}
}
