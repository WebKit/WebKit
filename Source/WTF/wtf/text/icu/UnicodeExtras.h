/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include <array>
#include <span>
#include <unicode/ucsdet.h>

namespace WTF {

WTF_EXPORT_PRIVATE std::span<const UCharsetMatch*> ucsdet_detectAll_span(UCharsetDetector*, UErrorCode* status);

static constexpr std::array<char, 16> U8_LEAD3_T1_BITS_SAFE { '\x20', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x30', '\x10', '\x30', '\x30' };
static constexpr std::array<char, 16> U8_LEAD4_T1_BITS_SAFE { '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x1E', '\x0F' , '\x0F', '\x0F', '\x00', '\x00', '\x00', '\x00' };

#define U8_INTERNAL_NEXT_OR_SUB_SAFE(s, i, length, c, sub) { \
    (c)=(uint8_t)(s)[(i)++]; \
    if(!U8_IS_SINGLE(c)) { \
        uint8_t __t = 0; \
        if((i)!=(length) && \
            /* fetch/validate/assemble all but last trail byte */ \
            ((c)>=0xe0 ? \
                ((c)<0xf0 ?  /* U+0800..U+FFFF except surrogates */ \
                    U8_LEAD3_T1_BITS_SAFE[(c)&=0xf]&(1<<((__t=(s)[i])>>5)) && \
                    (__t&=0x3f, 1) \
                :  /* U+10000..U+10FFFF */ \
                    ((c)-=0xf0)<=4 && \
                    U8_LEAD4_T1_BITS_SAFE[(__t=(s)[i])>>4]&(1<<(c)) && \
                    ((c)=((c)<<6)|(__t&0x3f), ++(i)!=(length)) && \
                    (__t=(s)[i]-0x80)<=0x3f) && \
                /* valid second-to-last trail byte */ \
                ((c)=((c)<<6)|__t, ++(i)!=(length)) \
            :  /* U+0080..U+07FF */ \
                (c)>=0xc2 && ((c)&=0x1f, 1)) && \
            /* last trail byte */ \
            (__t=(s)[i]-0x80)<=0x3f && \
            ((c)=((c)<<6)|__t, ++(i), 1)) { \
        } else { \
            (c)=(sub);  /* ill-formed*/ \
        } \
    } \
}

#define U8_NEXT_SPAN(s, i, c) U8_INTERNAL_NEXT_OR_SUB_SAFE(s, i, std::size(s), c, U_SENTINEL)
#define U8_NEXT_OR_FFFD_SPAN(s, i, c) U8_INTERNAL_NEXT_OR_SUB_SAFE(s, i, std::size(s), c, 0xfffd)

} // namespace WTF

using WTF::ucsdet_detectAll_span;
