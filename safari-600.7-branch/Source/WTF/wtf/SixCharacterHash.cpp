/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SixCharacterHash.h"

#include <wtf/StdLibExtras.h>

#include <string.h>

namespace WTF {

#define TABLE ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")

unsigned sixCharacterHashStringToInteger(const char* string)
{
    unsigned hash = 0;

    RELEASE_ASSERT(strlen(string) == 6);
    
    for (unsigned i = 0; i < 6; ++i) {
        hash *= 62;
        unsigned c = string[i];
        if (c >= 'A' && c <= 'Z') {
            hash += c - 'A';
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            hash += c - 'a' + 26;
            continue;
        }
        ASSERT(c >= '0' && c <= '9');
        hash += c - '0' + 26 * 2;
    }
    
    return hash;
}

std::array<char, 7> integerToSixCharacterHashString(unsigned hash)
{
    static_assert(WTF_ARRAY_LENGTH(TABLE) - 1 == 62, "Six character hash table is not 62 characters long.");

    std::array<char, 7> buffer;
    unsigned accumulator = hash;
    for (unsigned i = 6; i--;) {
        buffer[i] = TABLE[accumulator % 62];
        accumulator /= 62;
    }
    buffer[6] = 0;
    return buffer;
}

} // namespace WTF

