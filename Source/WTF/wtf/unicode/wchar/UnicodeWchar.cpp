/*
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
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
#include "UnicodeWchar.h"

#include <algorithm>

int unorm_normalize(const UChar*, int32_t, UNormalizationMode, int32_t, UChar*, int32_t, UErrorCode*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

UCharDirection u_charDirection(UChar32)
{
    return U_LEFT_TO_RIGHT; // FIXME: implement!
}

UChar32 u_charMirror(UChar32 character)
{
    // FIXME: Implement this.
    return character;
}

int8_t u_charType(UChar32)
{
    // FIXME: Implement this.
    return U_CHAR_CATEGORY_UNKNOWN;
}

uint8_t u_getCombiningClass(UChar32)
{
    // FIXME: Implement this.
    return 0;
}

int u_getIntPropertyValue(UChar32, UProperty property)
{
    switch (property) {
    case UCHAR_DECOMPOSITION_TYPE:
        // FIXME: Implement this.
        return U_DT_NONE;
    case UCHAR_LINE_BREAK:
        // FIXME: Implement this.
        return U_LB_UNKNOWN;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

int u_memcasecmp(const UChar* a, const UChar* b, int length, unsigned options)
{
    for (int i = 0; i < length; ++i) {
        UChar c1 = u_foldCase(a[i], options);
        UChar c2 = u_foldCase(b[i], options);
        if (c1 != c2)
            return c1 - c2;
    }

    return 0;
}

template<wint_t Function(wint_t)>
static inline int convertWithFunction(UChar* result, int resultLength, const UChar* source, int sourceLength, UErrorCode& status)
{
    UChar* resultIterator = result;
    UChar* resultEnd = result + std::min(resultLength, sourceLength);
    while (resultIterator < resultEnd)
        *resultIterator++ = Function(*source++);

    if (sourceLength < resultLength)
        *resultIterator = '\0';

    status = sourceLength > resultLength ? U_ERROR : U_ZERO_ERROR;
    return sourceLength;
}

int u_strFoldCase(UChar* result, int resultLength, const UChar* source, int sourceLength, unsigned options, UErrorCode* status)
{
    ASSERT_UNUSED(options, options == U_FOLD_CASE_DEFAULT);
    return convertWithFunction<towlower>(result, resultLength, source, sourceLength, *status);
}

int u_strToLower(UChar* result, int resultLength, const UChar* source, int sourceLength, const char*, UErrorCode* status)
{
    return convertWithFunction<towlower>(result, resultLength, source, sourceLength, *status);
}

int u_strToUpper(UChar* result, int resultLength, const UChar* source, int sourceLength, const char*, UErrorCode* status)
{
    return convertWithFunction<towupper>(result, resultLength, source, sourceLength, *status);
}
