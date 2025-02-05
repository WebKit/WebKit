/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DenormalDisabler.h"

namespace WebCore {

#if HAVE(DENORMAL)

#if CPU(X86_SSE2)
constexpr int kDenormalBitMask = 0x8040;
#elif CPU(ARM) || CPU(ARM64)
constexpr int kDenormalBitMask = 1 << 24;
#endif


#if CPU(X86_SSE2)
constexpr int kFlushDenormalsValue = kDenormalBitMask;
#elif CPU(ARM) || CPU(ARM64)
constexpr int kFlushDenormalsValue = kDenormalBitMask;
#endif

static intptr_t readStatusWord()
{
    intptr_t result = DenormalDisabler::kUndefinedStatusWord;
#if CPU(X86_SSE2)
    asm volatile("stmxcsr %0" : "=m"(result));
#elif CPU(ARM)
    asm volatile("vmrs %[result], FPSCR" : [result] "=r"(result));
#elif CPU(ARM64)
    asm volatile("mrs %x[result], FPCR" : [result] "=r"(result));
#endif
    return result;
}

static void setStatusWord(intptr_t statusWord)
{
#if CPU(X86_SSE2)
    asm volatile("ldmxcsr %0" : : "m"(statusWord));
#elif CPU(ARM)
    asm volatile("vmsr FPSCR, %[src]" : : [src] "r"(statusWord));
#elif CPU(ARM64)
    asm volatile("msr FPCR, %x[src]" : : [src] "r"(statusWord));
#endif
}

constexpr bool areDenormalsEnabled(intptr_t statusWord)
{
    return (statusWord & kDenormalBitMask) != kFlushDenormalsValue;
}

DenormalDisabler::DenormalDisabler()
    : m_savedCSR { readStatusWord() }
    , m_disablingActivated { areDenormalsEnabled(m_savedCSR) }
{
    if (m_disablingActivated) {
        ASSERT(m_savedCSR != kUndefinedStatusWord);
        setStatusWord((m_savedCSR & ~kDenormalBitMask) | kFlushDenormalsValue);
        ASSERT(!areDenormalsEnabled(readStatusWord()));
    }
}

DenormalDisabler::~DenormalDisabler()
{
    if (m_disablingActivated) {
        ASSERT(m_savedCSR != kUndefinedStatusWord);
        setStatusWord(m_savedCSR);
    }
}
#else
DenormalDisabler::DenormalDisabler() = default
DenormalDisabler::~DenormalDisabler() = default;
#endif

}
