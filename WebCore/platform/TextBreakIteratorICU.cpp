/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "TextBreakIterator.h"

namespace WebCore {

TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    // The locale is currently ignored when determining character cluster breaks.
    // This may change in the future, according to Deborah Goldsmith.
    static bool createdIterator = false;
    static UBreakIterator* iterator;
    UErrorCode status;
    if (!createdIterator) {
        status = U_ZERO_ERROR;
        iterator = ubrk_open(UBRK_WORD, "en_us", 0, 0, &status);
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    status = U_ZERO_ERROR;
    ubrk_setText(iterator, string, length, &status);
    if (U_FAILURE(status))
        return 0;

    return iterator;
}

TextBreakIterator* characterBreakIterator(const UChar* string, int length)
{
    if (!string)
        return 0;

    // The locale is currently ignored when determining character cluster breaks.
    // This may change in the future, according to Deborah Goldsmith.
    static bool createdIterator = false;
    static UBreakIterator* iterator;
    UErrorCode status;
    if (!createdIterator) {
        status = U_ZERO_ERROR;
        iterator = ubrk_open(UBRK_CHARACTER, "en_us", 0, 0, &status);
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    status = U_ZERO_ERROR;
    ubrk_setText(iterator, reinterpret_cast<const UChar*>(string), length, &status);
    if (status != U_ZERO_ERROR)
        return 0;

    return iterator;
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

}
