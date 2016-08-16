/*
 * Copyright (C) 2005, 2007, 2010, 2013, 2016 Apple Inc. All rights reserved.
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

static const UChar lineBreakTableFirstCharacter = '!';
static const UChar lineBreakTableLastCharacter = 127;
static const unsigned lineBreakTableColumnCount = (lineBreakTableLastCharacter - lineBreakTableFirstCharacter) / 8 + 1;

WEBCORE_EXPORT extern const unsigned char lineBreakTable[][lineBreakTableColumnCount];

enum class NonBreakingSpaceBehavior {
    IgnoreNonBreakingSpace,
    TreatNonBreakingSpaceAsBreak,
};

template<NonBreakingSpaceBehavior nonBreakingSpaceBehavior>
static inline bool isBreakableSpace(UChar character)
{
    switch (character) {
    case ' ':
    case '\n':
    case '\t':
        return true;
    case noBreakSpace:
        return nonBreakingSpaceBehavior == NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak;
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

template<NonBreakingSpaceBehavior nonBreakingSpaceBehavior>
inline bool needsLineBreakIterator(UChar character)
{
    if (nonBreakingSpaceBehavior == NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak)
        return character > lineBreakTableLastCharacter;
    return character > lineBreakTableLastCharacter && character != noBreakSpace;
}

// When in non-loose mode, we can use the ASCII shortcut table.
template<typename CharacterType, NonBreakingSpaceBehavior nonBreakingSpaceBehavior>
inline unsigned nextBreakablePositionNonLoosely(LazyLineBreakIterator& lazyBreakIterator, const CharacterType* string, unsigned length, unsigned startPosition)
{
    Optional<unsigned> nextBreak;

    CharacterType lastLastCharacter = startPosition > 1 ? string[startPosition - 2] : static_cast<CharacterType>(lazyBreakIterator.secondToLastCharacter());
    CharacterType lastCharacter = startPosition > 0 ? string[startPosition - 1] : static_cast<CharacterType>(lazyBreakIterator.lastCharacter());
    unsigned priorContextLength = lazyBreakIterator.priorContextLength();
    for (unsigned i = startPosition; i < length; i++) {
        CharacterType character = string[i];

        // Non-loose mode, so use ASCII shortcut (shouldBreakAfter) if not breakable space.
        if (isBreakableSpace<nonBreakingSpaceBehavior>(character) || shouldBreakAfter(lastLastCharacter, lastCharacter, character))
            return i;

        // Non-loose mode, so conditionally use break iterator.
        if (needsLineBreakIterator<nonBreakingSpaceBehavior>(character) || needsLineBreakIterator<nonBreakingSpaceBehavior>(lastCharacter)) {
            if (!nextBreak || nextBreak.value() < i) {
                // Don't break if positioned at start of primary context and there is no prior context.
                if (i || priorContextLength) {
                    TextBreakIterator* breakIterator = lazyBreakIterator.get(priorContextLength);
                    if (breakIterator) {
                        int candidate = textBreakFollowing(breakIterator, i - 1 + priorContextLength);
                        if (candidate == TextBreakDone)
                            nextBreak = Nullopt;
                        else {
                            unsigned result = candidate;
                            ASSERT(result >= priorContextLength);
                            nextBreak = result - priorContextLength;
                        }
                    }
                }
            }
            if (i == nextBreak && !isBreakableSpace<nonBreakingSpaceBehavior>(lastCharacter))
                return i;
        }

        lastLastCharacter = lastCharacter;
        lastCharacter = character;
    }

    return length;
}

// When in loose mode, we can't use the ASCII shortcut table since loose mode allows "$100" to break after '$' in content marked as CJK.
// N.B. It should be possible to combine the following with the non-loose version above by adding a LooseBehavior template parameter;
// however, when doing this, a 10% performance regression appeared on chromium-win (https://bugs.webkit.org/show_bug.cgi?id=89235#c112).
template<typename CharacterType, NonBreakingSpaceBehavior nonBreakingSpaceBehavior>
static inline unsigned nextBreakablePositionLoosely(LazyLineBreakIterator& lazyBreakIterator, const CharacterType* string, unsigned length, unsigned startPosition)
{
    Optional<unsigned> nextBreak;

    CharacterType lastCharacter = startPosition > 0 ? string[startPosition - 1] : static_cast<CharacterType>(lazyBreakIterator.lastCharacter());
    unsigned priorContextLength = lazyBreakIterator.priorContextLength();
    for (unsigned i = startPosition; i < length; i++) {
        CharacterType character = string[i];

        // Always loose mode, so don't use ASCII shortcut (shouldBreakAfter).
        if (isBreakableSpace<nonBreakingSpaceBehavior>(character))
            return i;

        // Always use line break iterator in loose mode.
        if (!nextBreak || nextBreak.value() < i) {
            // Don't break if positioned at start of primary context and there is no prior context.
            if (i || priorContextLength) {
                TextBreakIterator* breakIterator = lazyBreakIterator.get(priorContextLength);
                if (breakIterator) {
                    ASSERT(i + priorContextLength >= 1);
                    int candidate = textBreakFollowing(breakIterator, i + priorContextLength - 1);
                    if (candidate == TextBreakDone)
                        nextBreak = Nullopt;
                    else {
                        unsigned result = candidate;
                        ASSERT(result > priorContextLength);
                        nextBreak = result - priorContextLength;
                    }
                }
            }
        }
        if (i == nextBreak && !isBreakableSpace<nonBreakingSpaceBehavior>(lastCharacter))
            return i;

        lastCharacter = character;
    }

    return length;
}

template<typename CharacterType, NonBreakingSpaceBehavior nonBreakingSpaceBehavior>
inline unsigned nextBreakablePositionKeepingAllWords(const CharacterType* string, unsigned length, unsigned startPosition)
{
    for (unsigned i = startPosition; i < length; i++) {
        if (isBreakableSpace<nonBreakingSpaceBehavior>(string[i]))
            return i;
    }
    return length;
}

inline unsigned nextBreakablePositionKeepingAllWords(LazyLineBreakIterator& lazyBreakIterator, unsigned startPosition)
{
    auto stringView = lazyBreakIterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionKeepingAllWords<LChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionKeepingAllWords<UChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(stringView.characters16(), stringView.length(), startPosition);
}

inline unsigned nextBreakablePositionKeepingAllWordsIgnoringNBSP(LazyLineBreakIterator& iterator, unsigned startPosition)
{
    auto stringView = iterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionKeepingAllWords<LChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionKeepingAllWords<UChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(stringView.characters16(), stringView.length(), startPosition);
}

inline unsigned nextBreakablePosition(LazyLineBreakIterator& iterator, unsigned startPosition)
{
    auto stringView = iterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionNonLoosely<LChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(iterator, stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionNonLoosely<UChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(iterator, stringView.characters16(), stringView.length(), startPosition);
}

inline unsigned nextBreakablePositionIgnoringNBSP(LazyLineBreakIterator& lazyBreakIterator, unsigned startPosition)
{
    auto stringView = lazyBreakIterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionNonLoosely<LChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(lazyBreakIterator, stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionNonLoosely<UChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(lazyBreakIterator, stringView.characters16(), stringView.length(), startPosition);
}

inline unsigned nextBreakablePositionLoose(LazyLineBreakIterator& lazyBreakIterator, unsigned startPosition)
{
    auto stringView = lazyBreakIterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionLoosely<LChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(lazyBreakIterator, stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionLoosely<UChar, NonBreakingSpaceBehavior::TreatNonBreakingSpaceAsBreak>(lazyBreakIterator, stringView.characters16(), stringView.length(), startPosition);
}

inline unsigned nextBreakablePositionIgnoringNBSPLoose(LazyLineBreakIterator& lazyBreakIterator, unsigned startPosition)
{
    auto stringView = lazyBreakIterator.stringView();
    if (stringView.is8Bit())
        return nextBreakablePositionLoosely<LChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(lazyBreakIterator, stringView.characters8(), stringView.length(), startPosition);
    return nextBreakablePositionLoosely<UChar, NonBreakingSpaceBehavior::IgnoreNonBreakingSpace>(lazyBreakIterator, stringView.characters16(), stringView.length(), startPosition);
}

inline bool isBreakable(LazyLineBreakIterator& lazyBreakIterator, unsigned startPosition, Optional<unsigned>& nextBreakable, bool breakNBSP, bool isLooseMode, bool keepAllWords)
{
    if (nextBreakable && nextBreakable.value() >= startPosition)
        return startPosition == nextBreakable;

    if (keepAllWords) {
        if (breakNBSP)
            nextBreakable = nextBreakablePositionKeepingAllWords(lazyBreakIterator, startPosition);
        else
            nextBreakable = nextBreakablePositionKeepingAllWordsIgnoringNBSP(lazyBreakIterator, startPosition);
    } else if (isLooseMode) {
        if (breakNBSP)
            nextBreakable = nextBreakablePositionLoose(lazyBreakIterator, startPosition);
        else
            nextBreakable = nextBreakablePositionIgnoringNBSPLoose(lazyBreakIterator, startPosition);
    } else {
        if (breakNBSP)
            nextBreakable = nextBreakablePosition(lazyBreakIterator, startPosition);
        else
            nextBreakable = nextBreakablePositionIgnoringNBSP(lazyBreakIterator, startPosition);
    }
    return startPosition == nextBreakable;
}

} // namespace WebCore
