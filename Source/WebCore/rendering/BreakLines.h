/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <unicode/ubrk.h>
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

enum class NoBreakSpaceBehavior {
    Normal,
    Break,
};

enum class WordBreakBehavior {
    Normal,
    BreakAll,
    KeepAll,
};

enum class LineBreakRules {
    Normal, // Fast path available when using default line-breaking rules within ASCII.
    Special, // Uses ICU to handle special line-breaking rules.
};

template<LineBreakRules rules, WordBreakBehavior words, NoBreakSpaceBehavior spaces>
inline unsigned nextBreakablePosition(CachedLineBreakIteratorFactory&, size_t startPosition);
inline unsigned nextBreakablePosition(CachedLineBreakIteratorFactory& iterator, size_t startPosition)
{
    return nextBreakablePosition<LineBreakRules::Normal, WordBreakBehavior::Normal, NoBreakSpaceBehavior::Normal>(iterator, startPosition);
}
inline bool isBreakable(CachedLineBreakIteratorFactory&, unsigned startPosition, std::optional<unsigned>& nextBreakable, bool breakNBSP, bool canUseShortcut, bool keepAllWords, bool breakAnywhere);

// Private Use Below

static const UChar lineBreakTableFirstCharacter = '!';
static const UChar lineBreakTableLastCharacter = 127;
static const unsigned lineBreakTableColumnCount = (lineBreakTableLastCharacter - lineBreakTableFirstCharacter) / 8 + 1;
WEBCORE_EXPORT extern const unsigned char lineBreakTable[][lineBreakTableColumnCount];

template<NoBreakSpaceBehavior nonBreakingSpaceBehavior>
static inline bool isBreakableSpace(UChar character)
{
    switch (character) {
    case ' ':
    case '\n':
    case '\t':
        return true;
    case noBreakSpace:
        return nonBreakingSpaceBehavior == NoBreakSpaceBehavior::Break;
    default:
        return false;
    }
}

inline bool shouldBreakAfter(UChar lastCharacter, UChar character, UChar nextCharacter)
{
    // Don't allow line breaking between '-' and a digit if the '-' may mean a minus sign in the context,
    // while allow breaking in 'ABCD-1234' and '1234-5678' which may be in long URLs.
    if (character == '-' && isASCIIDigit(nextCharacter))
        return isASCIIAlphanumeric(lastCharacter);

    // If both ch and nextCh are ASCII characters, use a lookup table for enhanced speed and for compatibility
    // with other browsers (see comments for asciiLineBreakTable for details).
    if (character >= lineBreakTableFirstCharacter && character <= lineBreakTableLastCharacter && nextCharacter >= lineBreakTableFirstCharacter && nextCharacter <= lineBreakTableLastCharacter) {
        const unsigned char* tableRow = lineBreakTable[character - lineBreakTableFirstCharacter];
        unsigned nextCharacterIndex = nextCharacter - lineBreakTableFirstCharacter;
        return tableRow[nextCharacterIndex / 8] & (1 << (nextCharacterIndex % 8));
    }
    // Otherwise defer to the Unicode algorithm by returning false.
    return false;
}

template<NoBreakSpaceBehavior nonBreakingSpaceBehavior>
inline bool needsLineBreakIterator(UChar character)
{
    if (nonBreakingSpaceBehavior == NoBreakSpaceBehavior::Break)
        return character > lineBreakTableLastCharacter;
    return character > lineBreakTableLastCharacter && character != noBreakSpace;
}

// When in non-loose mode, we can use the ASCII shortcut table.
template<typename CharacterType, LineBreakRules shortcutRules, NoBreakSpaceBehavior nonBreakingSpaceBehavior>
inline size_t nextBreakablePosition(CachedLineBreakIteratorFactory& lineBreakIteratorFactory, std::span<const CharacterType> string, size_t startPosition)
{
    std::optional<unsigned> nextBreak;

    CharacterType lastLastCharacter = startPosition > 1 ? string[startPosition - 2] : static_cast<CharacterType>(lineBreakIteratorFactory.priorContext().secondToLastCharacter());
    CharacterType lastCharacter = startPosition > 0 ? string[startPosition - 1] : static_cast<CharacterType>(lineBreakIteratorFactory.priorContext().lastCharacter());
    auto priorContextLength = lineBreakIteratorFactory.priorContext().length();
    for (size_t i = startPosition; i < string.size(); ++i) {
        CharacterType character = string[i];

        if (isBreakableSpace<nonBreakingSpaceBehavior>(character) || (shortcutRules == LineBreakRules::Normal && shouldBreakAfter(lastLastCharacter, lastCharacter, character)))
            return i;

        if (shortcutRules == LineBreakRules::Special || needsLineBreakIterator<nonBreakingSpaceBehavior>(character) || needsLineBreakIterator<nonBreakingSpaceBehavior>(lastCharacter)) {
            if (!nextBreak || nextBreak.value() < i) {
                // Don't break if positioned at start of primary context and there is no prior context.
                if (i || priorContextLength) {
                    auto& breakIterator = lineBreakIteratorFactory.get();
                    nextBreak = breakIterator.following(i - 1);
                }
            }
            if (i == nextBreak && !isBreakableSpace<nonBreakingSpaceBehavior>(lastCharacter))
                return i;
        }

        lastLastCharacter = lastCharacter;
        lastCharacter = character;
    }

    return string.size();
}

template<typename CharacterType, NoBreakSpaceBehavior nonBreakingSpaceBehavior>
inline size_t nextBreakableSpace(std::span<const CharacterType> string, size_t startPosition)
{
    // FIXME: Use ICU instead.
    for (size_t i = startPosition; i < string.size(); ++i) {
        if (isBreakableSpace<nonBreakingSpaceBehavior>(string[i]))
            return i;
        // FIXME: This should either be in isBreakableSpace (though previous attempts broke the world) or should use ICU instead.
        if (string[i] == zeroWidthSpace)
            return i;
        if (string[i] == ideographicSpace)
            return i + 1;
    }
    return string.size();
}

inline unsigned nextCharacter(CachedLineBreakIteratorFactory& lineBreakIteratorFactory, unsigned startPosition)
{
    auto stringView = lineBreakIteratorFactory.stringView();
    ASSERT(startPosition <= stringView.length());
    // FIXME: Can/Should we implement this using a Shared Iterator (performance issue)
    // https://bugs.webkit.org/show_bug.cgi?id=197876
    NonSharedCharacterBreakIterator iterator(stringView);
    std::optional<unsigned> next = ubrk_following(iterator, startPosition);
    return next.value_or(stringView.length());
}

template<LineBreakRules rules, WordBreakBehavior words, NoBreakSpaceBehavior spaces>
inline unsigned nextBreakablePosition(CachedLineBreakIteratorFactory& lineBreakIteratorFactory, size_t startPosition)
{
    auto stringView = lineBreakIteratorFactory.stringView();
    if (stringView.is8Bit()) {
        return words == WordBreakBehavior::KeepAll
            ? nextBreakableSpace<LChar, spaces>(stringView.span8(), startPosition)
            : nextBreakablePosition<LChar, rules, spaces>(lineBreakIteratorFactory, stringView.span8(), startPosition);
    }
    return words == WordBreakBehavior::KeepAll
        ? nextBreakableSpace<UChar, spaces>(stringView.span16(), startPosition)
        : nextBreakablePosition<UChar, rules, spaces>(lineBreakIteratorFactory, stringView.span16(), startPosition);
}


inline bool isBreakable(CachedLineBreakIteratorFactory& lineBreakIteratorFactory, unsigned startPosition, std::optional<unsigned>& nextBreakable, bool breakNBSP, bool canUseShortcut, bool keepAllWords, bool breakAnywhere)
{
    if (nextBreakable && nextBreakable.value() >= startPosition)
        return startPosition == nextBreakable;

    if (breakAnywhere)
        return startPosition == nextCharacter(lineBreakIteratorFactory, startPosition);

    if (keepAllWords) {
        if (breakNBSP)
            return startPosition == nextBreakablePosition<LineBreakRules::Special, WordBreakBehavior::KeepAll, NoBreakSpaceBehavior::Break>(lineBreakIteratorFactory, startPosition);
        return startPosition == nextBreakablePosition<LineBreakRules::Special, WordBreakBehavior::KeepAll, NoBreakSpaceBehavior::Normal>(lineBreakIteratorFactory, startPosition);
    }

    if (canUseShortcut) {
        if (breakNBSP)
            return startPosition == nextBreakablePosition<LineBreakRules::Normal, WordBreakBehavior::Normal, NoBreakSpaceBehavior::Break>(lineBreakIteratorFactory, startPosition);
        return startPosition == nextBreakablePosition<LineBreakRules::Normal, WordBreakBehavior::Normal, NoBreakSpaceBehavior::Normal>(lineBreakIteratorFactory, startPosition);
    }

    if (breakNBSP)
        return startPosition == nextBreakablePosition<LineBreakRules::Special, WordBreakBehavior::Normal, NoBreakSpaceBehavior::Break>(lineBreakIteratorFactory, startPosition);
    return startPosition == nextBreakablePosition<LineBreakRules::Special, WordBreakBehavior::Normal, NoBreakSpaceBehavior::Normal>(lineBreakIteratorFactory, startPosition);
}

} // namespace WebCore
