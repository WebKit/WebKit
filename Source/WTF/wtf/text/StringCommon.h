/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StringCommon_h
#define StringCommon_h

#include <unicode/uchar.h>
#include <wtf/ASCIICType.h>

namespace WTF {

template<typename CharacterTypeA, typename CharacterTypeB>
inline bool equalIgnoringASCIICase(const CharacterTypeA* a, const CharacterTypeB* b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (toASCIILower(a[i]) != toASCIILower(b[i]))
            return false;
    }
    return true;
}

template<typename StringClass>
bool equalIgnoringASCIICaseCommon(const StringClass& a, const StringClass& b)
{
    unsigned length = a.length();
    if (length != b.length())
        return false;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringASCIICase(a.characters8(), b.characters8(), length);

        return equalIgnoringASCIICase(a.characters8(), b.characters16(), length);
    }

    if (b.is8Bit())
        return equalIgnoringASCIICase(a.characters16(), b.characters8(), length);

    return equalIgnoringASCIICase(a.characters16(), b.characters16(), length);
}

}

#endif // StringCommon_h
