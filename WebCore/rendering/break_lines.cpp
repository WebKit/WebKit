/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "break_lines.h"

#include "CharacterNames.h"
#include "TextBreakIterator.h"

#if PLATFORM(MAC)
#include <CoreServices/CoreServices.h>
#endif

namespace WebCore {

static inline bool isBreakableSpace(UChar ch, bool treatNoBreakSpaceAsBreak)
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

static inline bool shouldBreakAfter(UChar ch)
{
    // Match WinIE's breaking strategy, which is to always allow breaks after hyphens and question marks.
    switch (ch) {
        case '-':
        case '?':
        case softHyphen:
            return true;
        default:
            return false;
    }
}

static inline bool needsLineBreakIterator(UChar ch)
{
    return ch > 0x7F && ch != noBreakSpace;
}

#ifdef BUILDING_ON_TIGER
static inline TextBreakLocatorRef lineBreakLocator()
{
    TextBreakLocatorRef locator = 0;
    UCCreateTextBreakLocator(0, 0, kUCTextBreakLineMask, &locator);
    return locator;
}
#endif

int nextBreakablePosition(const UChar* str, int pos, int len, bool treatNoBreakSpaceAsBreak)
{
#ifndef BUILDING_ON_TIGER
    TextBreakIterator* breakIterator = 0;
#endif
    int nextBreak = -1;

    UChar lastCh = pos > 0 ? str[pos - 1] : 0;
    for (int i = pos; i < len; i++) {
        UChar ch = str[i];

        if (isBreakableSpace(ch, treatNoBreakSpaceAsBreak) || shouldBreakAfter(lastCh))
            return i;

        if (needsLineBreakIterator(ch) || needsLineBreakIterator(lastCh)) {
            if (nextBreak < i && i) {
#ifndef BUILDING_ON_TIGER
                if (!breakIterator)
                    breakIterator = lineBreakIterator(str, len);
                if (breakIterator)
                    nextBreak = textBreakFollowing(breakIterator, i - 1);
#else
                static TextBreakLocatorRef breakLocator = lineBreakLocator();
                if (breakLocator) {
                    UniCharArrayOffset nextUCBreak;
                    if (UCFindTextBreak(breakLocator, kUCTextBreakLineMask, 0, str, len, i, &nextUCBreak) == 0)
                        nextBreak = nextUCBreak;
                }
#endif
            }
            if (i == nextBreak && !isBreakableSpace(lastCh, treatNoBreakSpaceAsBreak))
                return i;
        }

        lastCh = ch;
    }

    return len;
}

} // namespace WebCore
