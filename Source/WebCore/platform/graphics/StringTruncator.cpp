/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StringTruncator.h"

#include "Font.h"
#include "TextBreakIterator.h"
#include "TextRun.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

#define STRING_BUFFER_SIZE 2048

typedef unsigned TruncationFunction(const String&, unsigned length, unsigned keepCount, UChar* buffer, bool shouldInsertEllipsis);

static inline int textBreakAtOrPreceding(TextBreakIterator* it, int offset)
{
    if (isTextBreak(it, offset))
        return offset;

    int result = textBreakPreceding(it, offset);
    return result == TextBreakDone ? 0 : result;
}

static inline int boundedTextBreakFollowing(TextBreakIterator* it, int offset, int length)
{
    int result = textBreakFollowing(it, offset);
    return result == TextBreakDone ? length : result;
}

static unsigned centerTruncateToBuffer(const String& string, unsigned length, unsigned keepCount, UChar* buffer, bool shouldInsertEllipsis)
{
    ASSERT_WITH_SECURITY_IMPLICATION(keepCount < length);
    ASSERT_WITH_SECURITY_IMPLICATION(keepCount < STRING_BUFFER_SIZE);
    
    unsigned omitStart = (keepCount + 1) / 2;
    NonSharedCharacterBreakIterator it(StringView(string).substring(0, length));
    unsigned omitEnd = boundedTextBreakFollowing(it, omitStart + (length - keepCount) - 1, length);
    omitStart = textBreakAtOrPreceding(it, omitStart);

#if PLATFORM(IOS)
    // FIXME: We should guard this code behind an editing behavior. Then we can remove the PLATFORM(IOS)-guard.

    // Strip single character before ellipsis character, when that character is preceded by a space
    if (omitStart > 1 && string[omitStart - 1] != space && omitStart > 2 && string[omitStart - 2] == space)
        --omitStart;

    // Strip whitespace before and after the ellipsis character
    while (omitStart > 1 && string[omitStart - 1] == space)
        --omitStart;

    // Strip single character after ellipsis character, when that character is followed by a space
    if ((length - omitEnd) > 1 && string[omitEnd] != space && (length - omitEnd) > 2 && string[omitEnd + 1] == space)
        ++omitEnd;

    while ((length - omitEnd) > 1 && string[omitEnd] == space)
        ++omitEnd;
#endif

    unsigned truncatedLength = shouldInsertEllipsis ? omitStart + 1 + (length - omitEnd) : length - (omitEnd - omitStart);
    ASSERT(truncatedLength <= length);

    memcpy(buffer, string.deprecatedCharacters(), sizeof(UChar) * omitStart);
    if (shouldInsertEllipsis) {
        buffer[omitStart] = horizontalEllipsis;
        memcpy(&buffer[omitStart + 1], &string.deprecatedCharacters()[omitEnd], sizeof(UChar) * (length - omitEnd));
    } else
        memcpy(&buffer[omitStart], &string.deprecatedCharacters()[omitEnd], sizeof(UChar) * (length - omitEnd));
    return truncatedLength;
}

static unsigned rightTruncateToBuffer(const String& string, unsigned length, unsigned keepCount, UChar* buffer, bool shouldInsertEllipsis)
{
    ASSERT_WITH_SECURITY_IMPLICATION(keepCount < length);
    ASSERT_WITH_SECURITY_IMPLICATION(keepCount < STRING_BUFFER_SIZE);

#if PLATFORM(IOS)
    // FIXME: We should guard this code behind an editing behavior. Then we can remove the PLATFORM(IOS)-guard.

    // Strip single character before ellipsis character, when that character is preceded by a space
    if (keepCount > 1 && string[keepCount - 1] != space && keepCount > 2 && string[keepCount - 2] == space)
        --keepCount;

    // Strip whitespace before the ellipsis character
    while (keepCount > 1 && string[keepCount - 1] == space)
        --keepCount;
#endif

    NonSharedCharacterBreakIterator it(StringView(string).substring(0, length));
    unsigned keepLength = textBreakAtOrPreceding(it, keepCount);
    unsigned truncatedLength = shouldInsertEllipsis ? keepLength + 1 : keepLength;

    memcpy(buffer, string.deprecatedCharacters(), sizeof(UChar) * keepLength);
    if (shouldInsertEllipsis)
        buffer[keepLength] = horizontalEllipsis;

    return truncatedLength;
}

static unsigned rightClipToCharacterBuffer(const String& string, unsigned length, unsigned keepCount, UChar* buffer, bool)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);

    NonSharedCharacterBreakIterator it(StringView(string).substring(0, length));
    unsigned keepLength = textBreakAtOrPreceding(it, keepCount);
    memcpy(buffer, string.deprecatedCharacters(), sizeof(UChar) * keepLength);

    return keepLength;
}

static unsigned rightClipToWordBuffer(const String& string, unsigned length, unsigned keepCount, UChar* buffer, bool)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);

    TextBreakIterator* it = wordBreakIterator(StringView(string).substring(0, length));
    unsigned keepLength = textBreakAtOrPreceding(it, keepCount);
    memcpy(buffer, string.deprecatedCharacters(), sizeof(UChar) * keepLength);

#if PLATFORM(IOS)
    // FIXME: We should guard this code behind an editing behavior. Then we can remove the PLATFORM(IOS)-guard.
    // Motivated by <rdar://problem/7439327> truncation should not include a trailing space
    while ((keepLength > 0) && (string[keepLength - 1] == space))
        --keepLength;
#endif
    return keepLength;
}

static unsigned leftTruncateToBuffer(const String& string, unsigned length, unsigned keepCount, UChar* buffer, bool shouldInsertEllipsis)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);

    unsigned startIndex = length - keepCount;

    NonSharedCharacterBreakIterator it(string);
    unsigned adjustedStartIndex = startIndex;
    startIndex = boundedTextBreakFollowing(it, startIndex, length - startIndex);

    // Strip single character after ellipsis character, when that character is preceded by a space
    if (adjustedStartIndex < length && string[adjustedStartIndex] != space
        && adjustedStartIndex < length - 1 && string[adjustedStartIndex + 1] == space)
        ++adjustedStartIndex;

    // Strip whitespace after the ellipsis character
    while (adjustedStartIndex < length && string[adjustedStartIndex] == space)
        ++adjustedStartIndex;

    if (shouldInsertEllipsis) {
        buffer[0] = horizontalEllipsis;
        memcpy(&buffer[1], &string.deprecatedCharacters()[adjustedStartIndex], sizeof(UChar) * (length - adjustedStartIndex + 1));
        return length - adjustedStartIndex + 1;
    }
    memcpy(&buffer[0], &string.deprecatedCharacters()[adjustedStartIndex], sizeof(UChar) * (length - adjustedStartIndex + 1));
    return length - adjustedStartIndex;
}

static float stringWidth(const Font& renderer, const UChar* characters, unsigned length, bool disableRoundingHacks)
{
    TextRun run(characters, length);
    if (disableRoundingHacks)
        run.disableRoundingHacks();
    return renderer.width(run);
}

static String truncateString(const String& string, float maxWidth, const Font& font, TruncationFunction truncateToBuffer, bool disableRoundingHacks, float* resultWidth = nullptr, bool shouldInsertEllipsis = true,  float customTruncationElementWidth = 0, bool alwaysTruncate = false)
{
    if (string.isEmpty())
        return string;

    if (resultWidth)
        *resultWidth = 0;

    ASSERT(maxWidth >= 0);

    float currentEllipsisWidth = shouldInsertEllipsis ? stringWidth(font, &horizontalEllipsis, 1, disableRoundingHacks) : customTruncationElementWidth;

    UChar stringBuffer[STRING_BUFFER_SIZE];
    unsigned truncatedLength;
    unsigned keepCount;
    unsigned length = string.length();

    if (length > STRING_BUFFER_SIZE) {
        if (shouldInsertEllipsis)
            keepCount = STRING_BUFFER_SIZE - 1; // need 1 character for the ellipsis
        else
            keepCount = 0;
        truncatedLength = centerTruncateToBuffer(string, length, keepCount, stringBuffer, shouldInsertEllipsis);
    } else {
        keepCount = length;
        memcpy(stringBuffer, string.deprecatedCharacters(), sizeof(UChar) * length);
        truncatedLength = length;
    }

    float width = stringWidth(font, stringBuffer, truncatedLength, disableRoundingHacks);
    if (!shouldInsertEllipsis && alwaysTruncate)
        width += customTruncationElementWidth;
    if ((width - maxWidth) < 0.0001) { // Ignore rounding errors.
        if (resultWidth)
            *resultWidth = width;
        return string;
    }

    unsigned keepCountForLargestKnownToFit = 0;
    float widthForLargestKnownToFit = currentEllipsisWidth;
    
    unsigned keepCountForSmallestKnownToNotFit = keepCount;
    float widthForSmallestKnownToNotFit = width;
    
    if (currentEllipsisWidth >= maxWidth) {
        keepCountForLargestKnownToFit = 1;
        keepCountForSmallestKnownToNotFit = 2;
    }
    
    while (keepCountForLargestKnownToFit + 1 < keepCountForSmallestKnownToNotFit) {
        ASSERT_WITH_SECURITY_IMPLICATION(widthForLargestKnownToFit <= maxWidth);
        ASSERT_WITH_SECURITY_IMPLICATION(widthForSmallestKnownToNotFit > maxWidth);

        float ratio = (keepCountForSmallestKnownToNotFit - keepCountForLargestKnownToFit)
            / (widthForSmallestKnownToNotFit - widthForLargestKnownToFit);
        keepCount = static_cast<unsigned>(maxWidth * ratio);
        
        if (keepCount <= keepCountForLargestKnownToFit) {
            keepCount = keepCountForLargestKnownToFit + 1;
        } else if (keepCount >= keepCountForSmallestKnownToNotFit) {
            keepCount = keepCountForSmallestKnownToNotFit - 1;
        }
        
        ASSERT_WITH_SECURITY_IMPLICATION(keepCount < length);
        ASSERT(keepCount > 0);
        ASSERT_WITH_SECURITY_IMPLICATION(keepCount < keepCountForSmallestKnownToNotFit);
        ASSERT_WITH_SECURITY_IMPLICATION(keepCount > keepCountForLargestKnownToFit);

        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer, shouldInsertEllipsis);

        width = stringWidth(font, stringBuffer, truncatedLength, disableRoundingHacks);
        if (!shouldInsertEllipsis)
            width += customTruncationElementWidth;
        if (width <= maxWidth) {
            keepCountForLargestKnownToFit = keepCount;
            widthForLargestKnownToFit = width;
            if (resultWidth)
                *resultWidth = width;
        } else {
            keepCountForSmallestKnownToNotFit = keepCount;
            widthForSmallestKnownToNotFit = width;
        }
    }
    
    if (keepCountForLargestKnownToFit == 0) {
        keepCountForLargestKnownToFit = 1;
    }
    
    if (keepCount != keepCountForLargestKnownToFit) {
        keepCount = keepCountForLargestKnownToFit;
        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer, shouldInsertEllipsis);
    }
    
    return String(stringBuffer, truncatedLength);
}

String StringTruncator::centerTruncate(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks)
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer, !enableRoundingHacks);
}

String StringTruncator::rightTruncate(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks)
{
    return truncateString(string, maxWidth, font, rightTruncateToBuffer, !enableRoundingHacks);
}

float StringTruncator::width(const String& string, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks)
{
    return stringWidth(font, string.deprecatedCharacters(), string.length(), !enableRoundingHacks);
}

String StringTruncator::centerTruncate(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks, float& resultWidth, bool shouldInsertEllipsis, float customTruncationElementWidth)
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer, !enableRoundingHacks, &resultWidth, shouldInsertEllipsis, customTruncationElementWidth);
}

String StringTruncator::rightTruncate(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks, float& resultWidth, bool shouldInsertEllipsis, float customTruncationElementWidth)
{
    return truncateString(string, maxWidth, font, rightTruncateToBuffer, !enableRoundingHacks, &resultWidth, shouldInsertEllipsis, customTruncationElementWidth);
}

String StringTruncator::leftTruncate(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks, float& resultWidth, bool shouldInsertEllipsis, float customTruncationElementWidth)
{
    return truncateString(string, maxWidth, font, leftTruncateToBuffer, !enableRoundingHacks, &resultWidth, shouldInsertEllipsis, customTruncationElementWidth);
}

String StringTruncator::rightClipToCharacter(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks, float& resultWidth, bool shouldInsertEllipsis, float customTruncationElementWidth)
{
    return truncateString(string, maxWidth, font, rightClipToCharacterBuffer, !enableRoundingHacks, &resultWidth, shouldInsertEllipsis, customTruncationElementWidth);
}

String StringTruncator::rightClipToWord(const String& string, float maxWidth, const Font& font, EnableRoundingHacksOrNot enableRoundingHacks, float& resultWidth, bool shouldInsertEllipsis,  float customTruncationElementWidth, bool alwaysTruncate)
{
    return truncateString(string, maxWidth, font, rightClipToWordBuffer, !enableRoundingHacks, &resultWidth, shouldInsertEllipsis, customTruncationElementWidth, alwaysTruncate);
}

} // namespace WebCore
