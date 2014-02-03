/*
 * Copyright (C) 2005, 2007, 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef break_lines_h
#define break_lines_h

#include "TextBreakIterator.h"
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

static const UChar asciiLineBreakTableFirstChar = '!';
static const UChar asciiLineBreakTableLastChar = 127;
static const unsigned asciiLineBreakTableColumnCount = (asciiLineBreakTableLastChar - asciiLineBreakTableFirstChar) / 8 + 1;

extern const unsigned char asciiLineBreakTable[][asciiLineBreakTableColumnCount];

int nextBreakablePositionIgnoringNBSP(LazyLineBreakIterator&, int pos);
int nextBreakablePosition(LazyLineBreakIterator&, int pos);

template<bool treatNoBreakSpaceAsBreak>
static inline bool isBreakableSpace(UChar ch)
{
    switch (ch) {
    case ' ':
    case '\n':
    case '\t':
        return true;
    case noBreakSpace:
        return treatNoBreakSpaceAsBreak;
    default:
        return false;
    }
}

inline bool shouldBreakAfter(UChar lastCh, UChar ch, UChar nextCh)
{
    // Don't allow line breaking between '-' and a digit if the '-' may mean a minus sign in the context,
    // while allow breaking in 'ABCD-1234' and '1234-5678' which may be in long URLs.
    if (ch == '-' && isASCIIDigit(nextCh))
        return isASCIIAlphanumeric(lastCh);

    // If both ch and nextCh are ASCII characters, use a lookup table for enhanced speed and for compatibility
    // with other browsers (see comments for asciiLineBreakTable for details).
    if (ch >= asciiLineBreakTableFirstChar && ch <= asciiLineBreakTableLastChar && nextCh >= asciiLineBreakTableFirstChar && nextCh <= asciiLineBreakTableLastChar) {
        const unsigned char* tableRow = asciiLineBreakTable[ch - asciiLineBreakTableFirstChar];
        int nextChIndex = nextCh - asciiLineBreakTableFirstChar;
        return tableRow[nextChIndex / 8] & (1 << (nextChIndex % 8));
    }
    // Otherwise defer to the Unicode algorithm by returning false.
    return false;
}

template<bool treatNoBreakSpaceAsBreak>
inline bool needsLineBreakIterator(UChar ch)
{
    if (treatNoBreakSpaceAsBreak)
        return ch > asciiLineBreakTableLastChar;
    return ch > asciiLineBreakTableLastChar && ch != noBreakSpace;
}

template<typename CharacterType, bool treatNoBreakSpaceAsBreak>
inline int nextBreakablePosition(LazyLineBreakIterator& lazyBreakIterator, const CharacterType* str, unsigned length, int pos)
{
    int len = static_cast<int>(length);
    int nextBreak = -1;

    CharacterType lastLastCh = pos > 1 ? str[pos - 2] : static_cast<CharacterType>(lazyBreakIterator.secondToLastCharacter());
    CharacterType lastCh = pos > 0 ? str[pos - 1] : static_cast<CharacterType>(lazyBreakIterator.lastCharacter());
    unsigned priorContextLength = lazyBreakIterator.priorContextLength();
    for (int i = pos; i < len; i++) {
        CharacterType ch = str[i];

        if (isBreakableSpace<treatNoBreakSpaceAsBreak>(ch) || shouldBreakAfter(lastLastCh, lastCh, ch))
            return i;

        if (needsLineBreakIterator<treatNoBreakSpaceAsBreak>(ch) || needsLineBreakIterator<treatNoBreakSpaceAsBreak>(lastCh)) {
            if (nextBreak < i) {
                // Don't break if positioned at start of primary context and there is no prior context.
                if (i || priorContextLength) {
                    TextBreakIterator* breakIterator = lazyBreakIterator.get(priorContextLength);
                    if (breakIterator) {
                        nextBreak = textBreakFollowing(breakIterator, i - 1 + priorContextLength);
                        if (nextBreak >= 0)
                            nextBreak -= priorContextLength;
                    }
                }
            }
            if (i == nextBreak && !isBreakableSpace<treatNoBreakSpaceAsBreak>(lastCh))
                return i;
        }

        lastLastCh = lastCh;
        lastCh = ch;
    }

    return len;
}

inline int nextBreakablePositionIgnoringNBSP(LazyLineBreakIterator& lazyBreakIterator, int pos)
{
    String string = lazyBreakIterator.string();
    if (string.is8Bit())
        return nextBreakablePosition<LChar, false>(lazyBreakIterator, string.characters8(), string.length(), pos);
    return nextBreakablePosition<UChar, false>(lazyBreakIterator, string.characters16(), string.length(), pos);
}

inline int nextBreakablePosition(LazyLineBreakIterator& lazyBreakIterator, int pos)
{
    String string = lazyBreakIterator.string();
    if (string.is8Bit())
        return nextBreakablePosition<LChar, true>(lazyBreakIterator, string.characters8(), string.length(), pos);
    return nextBreakablePosition<UChar, true>(lazyBreakIterator, string.characters16(), string.length(), pos);
}

inline bool isBreakable(LazyLineBreakIterator& lazyBreakIterator, int pos, int& nextBreakable, bool breakNBSP)
{
    if (pos > nextBreakable) {
        if (breakNBSP)
            nextBreakable = nextBreakablePosition(lazyBreakIterator, pos);
        else
            nextBreakable = nextBreakablePositionIgnoringNBSP(lazyBreakIterator, pos);
    }
    return pos == nextBreakable;
}

} // namespace WebCore

#endif // break_lines_h
