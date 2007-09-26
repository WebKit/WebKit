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

#include "config.h"
#include "TextBreakIterator.h"

#include "TextBreakIteratorInternalICU.h"

#include <unicode/ubrk.h>

namespace WebCore {

static TextBreakIterator* setUpIterator(bool& createdIterator, TextBreakIterator*& iterator,
    UBreakIteratorType type, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        UErrorCode openStatus = U_ZERO_ERROR;
        iterator = static_cast<TextBreakIterator*>(ubrk_open(type, currentTextBreakLocaleID(), 0, 0, &openStatus));
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    UErrorCode setTextStatus = U_ZERO_ERROR;
    ubrk_setText(iterator, string, length, &setTextStatus);
    if (U_FAILURE(setTextStatus))
        return 0;

    return iterator;
}

TextBreakIterator* characterBreakIterator(const UChar* string, int length)
{
    static bool createdCharacterBreakIterator = false;
    static TextBreakIterator* staticCharacterBreakIterator;
    return setUpIterator(createdCharacterBreakIterator,
        staticCharacterBreakIterator, UBRK_CHARACTER, string, length);
}

TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    static bool createdWordBreakIterator = false;
    static TextBreakIterator* staticWordBreakIterator;
    return setUpIterator(createdWordBreakIterator,
        staticWordBreakIterator, UBRK_WORD, string, length);
}

TextBreakIterator* lineBreakIterator(const UChar* string, int length)
{
    static bool createdLineBreakIterator = false;
    static TextBreakIterator* staticLineBreakIterator;
    return setUpIterator(createdLineBreakIterator,
        staticLineBreakIterator, UBRK_LINE, string, length);
}

TextBreakIterator* sentenceBreakIterator(const UChar* string, int length)
{
    static bool createdSentenceBreakIterator = false;
    static TextBreakIterator* staticSentenceBreakIterator;
    return setUpIterator(createdSentenceBreakIterator,
        staticSentenceBreakIterator, UBRK_SENTENCE, string, length);
}

int textBreakFirst(TextBreakIterator* bi)
{
    return ubrk_first(bi);
}

int textBreakNext(TextBreakIterator* bi)
{
    return ubrk_next(bi);
}

int textBreakPreceding(TextBreakIterator* bi, int pos)
{
    return ubrk_preceding(bi, pos);
}

int textBreakFollowing(TextBreakIterator* bi, int pos)
{
    return ubrk_following(bi, pos);
}

int textBreakCurrent(TextBreakIterator* bi)
{
    return ubrk_current(bi);
}

bool isTextBreak(TextBreakIterator* bi, int pos)
{
    return ubrk_isBoundary(bi, pos);
}

}
