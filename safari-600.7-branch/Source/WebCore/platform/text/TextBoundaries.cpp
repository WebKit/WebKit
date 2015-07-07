/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Dominik Röttsches <dominik.roettsches@access-company.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextBoundaries.h"

#include "TextBreakIterator.h"
#include <wtf/text/StringImpl.h>

namespace WebCore {

unsigned endOfFirstWordBoundaryContext(StringView text)
{
    unsigned length = text.length();
    for (unsigned i = 0; i < length; ) {
        unsigned first = i;
        UChar32 ch;
        U16_NEXT(text, i, length, ch);
        if (!requiresContextForWordBoundary(ch))
            return first;
    }
    return length;
}

unsigned startOfLastWordBoundaryContext(StringView text)
{
    unsigned length = text.length();
    for (unsigned i = length; i > 0; ) {
        unsigned last = i;
        UChar32 ch;
        U16_PREV(text, 0, i, ch);
        if (!requiresContextForWordBoundary(ch))
            return last;
    }
    return 0;
}

#if !PLATFORM(COCOA)

int findNextWordFromIndex(StringView text, int position, bool forward)
{
    TextBreakIterator* it = wordBreakIterator(text);

    if (forward) {
        position = textBreakFollowing(it, position);
        while (position != TextBreakDone) {
            // We stop searching when the character preceeding the break is alphanumeric.
            if (static_cast<unsigned>(position) < text.length() && u_isalnum(text[position - 1]))
                return position;

            position = textBreakFollowing(it, position);
        }

        return text.length();
    } else {
        position = textBreakPreceding(it, position);
        while (position != TextBreakDone) {
            // We stop searching when the character following the break is alphanumeric.
            if (position && u_isalnum(text[position]))
                return position;

            position = textBreakPreceding(it, position);
        }

        return 0;
    }
}

void findWordBoundary(StringView text, int position, int* start, int* end)
{
    TextBreakIterator* it = wordBreakIterator(text);
    *end = textBreakFollowing(it, position);
    if (*end < 0)
        *end = textBreakLast(it);
    *start = textBreakPrevious(it);
}

void findEndWordBoundary(StringView text, int position, int* end)
{
    TextBreakIterator* it = wordBreakIterator(text);
    *end = textBreakFollowing(it, position);
    if (*end < 0)
        *end = textBreakLast(it);
}

#endif // !PLATFORM(COCOA)

} // namespace WebCore
