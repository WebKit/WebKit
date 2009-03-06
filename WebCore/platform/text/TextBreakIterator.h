/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef TextBreakIterator_h
#define TextBreakIterator_h

#include <wtf/unicode/Unicode.h>

namespace WebCore {

    class TextBreakIterator;

    // Note: The returned iterator is good only until you get another iterator.

    // Iterates over "extended grapheme clusters", as defined in UAX #29.
    // Note that platform implementations may be less sophisticated - e.g. ICU prior to
    // version 4.0 only supports "legacy grapheme clusters".
    // Use this for general text processing, e.g. string truncation.
    TextBreakIterator* characterBreakIterator(const UChar*, int length);

    // This is similar to character break iterator in most cases, but is subject to
    // platform UI conventions. One notable example where this can be different
    // from character break iterator is Thai prepend characters, see bug 24342.
    // Use this for insertion point and selection manipulations.
    TextBreakIterator* cursorMovementIterator(const UChar*, int length);

    TextBreakIterator* wordBreakIterator(const UChar*, int length);
    TextBreakIterator* lineBreakIterator(const UChar*, int length);
    TextBreakIterator* sentenceBreakIterator(const UChar*, int length);

    int textBreakFirst(TextBreakIterator*);
    int textBreakNext(TextBreakIterator*);
    int textBreakCurrent(TextBreakIterator*);
    int textBreakPreceding(TextBreakIterator*, int);
    int textBreakFollowing(TextBreakIterator*, int);
    bool isTextBreak(TextBreakIterator*, int);

    const int TextBreakDone = -1;

}

#endif
