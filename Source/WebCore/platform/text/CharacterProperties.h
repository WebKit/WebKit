/*
 * Copyright (C) 2015 Apple, Inc.  All rights reserved.
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

namespace WebCore {

static inline bool isEmojiGroupCandidate(UChar32 character)
{
    return character == 0x2640
        || character == 0x2642
        || character == 0x26F9
        || character == 0x2764
        || character == 0x1F308
        || (character >= 0x1F3C3 && character <= 0x1F3C4)
        || (character >= 0x1F3CA && character <= 0x1F3CC)
        || character == 0x1F3F3
        || character == 0x1F441
        || (character >= 0x1F466 && character <= 0x1F469)
        || (character >= 0x1F46E && character <= 0x1F46F)
        || character == 0x1F471
        || character == 0x1F473
        || character == 0x1F477
        || (character >= 0x1F481 && character <= 0x1F482)
        || (character >= 0x1F486 && character <= 0x1F487)
        || character == 0x1F48B
        || character == 0x1F575
        || character == 0x1F5E8
        || (character >= 0x1F645 && character <= 0x1F647)
        || character == 0x1F64B
        || (character >= 0x1F64D && character <= 0x1F64E)
        || character == 0x1F6A3
        || (character >= 0x1F6B4 && character <= 0x1F6B6);
}

static inline bool isEmojiFitzpatrickModifier(UChar32 character)
{
    // U+1F3FB - EMOJI MODIFIER FITZPATRICK TYPE-1-2
    // U+1F3FC - EMOJI MODIFIER FITZPATRICK TYPE-3
    // U+1F3FD - EMOJI MODIFIER FITZPATRICK TYPE-4
    // U+1F3FE - EMOJI MODIFIER FITZPATRICK TYPE-5
    // U+1F3FF - EMOJI MODIFIER FITZPATRICK TYPE-6

    return character >= 0x1F3FB && character <= 0x1F3FF;
}

inline bool isVariationSelector(UChar32 character)
{
    return character >= 0xFE00 && character <= 0xFE0F;
}

}
