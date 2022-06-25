/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if USE(FOUNDATION)
typedef struct _NSRange NSRange;
#endif

namespace WebCore {

struct CharacterRange {
    uint64_t location { 0 };
    uint64_t length { 0 };

    CharacterRange() = default;
    constexpr CharacterRange(uint64_t location, uint64_t length);

#if USE(CF)
    constexpr CharacterRange(CFRange);
    constexpr operator CFRange() const;
#endif

#if USE(FOUNDATION)
    constexpr CharacterRange(NSRange);
    constexpr operator NSRange() const;
#endif
};

constexpr CharacterRange::CharacterRange(uint64_t location, uint64_t length)
    : location(location)
    , length(length)
{
}

#if USE(CF)

constexpr CharacterRange::CharacterRange(CFRange range)
    : CharacterRange(range.location, range.length)
{
    ASSERT(range.location != kCFNotFound);
}

constexpr CharacterRange::operator CFRange() const
{
    CFIndex locationCF = location;
    CFIndex lengthCF = length;
    return { locationCF, lengthCF };
}

#endif

#if USE(FOUNDATION) && defined(__OBJC__)

constexpr CharacterRange::CharacterRange(NSRange range)
    : CharacterRange { range.location, range.length }
{
    ASSERT(range.location != NSNotFound);
}

constexpr CharacterRange::operator NSRange() const
{
    NSUInteger locationNS = location;
    NSUInteger lengthNS = length;
    return { locationNS, lengthNS };
}

#endif

} // namespace WebCore
