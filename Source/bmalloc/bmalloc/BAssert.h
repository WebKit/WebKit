/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef BAssert_h
#define BAssert_h

#include "BPlatform.h"

#if defined(NDEBUG) && BOS(DARWIN)

#if BCPU(X86_64) || BCPU(X86)
#define BBreakpointTrap()  asm volatile ("int3")
#elif BCPU(ARM_THUMB2)
#define BBreakpointTrap()  asm volatile ("bkpt #0")
#elif BCPU(ARM64)
#define BBreakpointTrap()  asm volatile ("brk #0")
#else
#error "Unsupported CPU".
#endif

// Crash with a SIGTRAP i.e EXC_BREAKPOINT.
// We are not using __builtin_trap because it is only guaranteed to abort, but not necessarily
// trigger a SIGTRAP. Instead, we use inline asm to ensure that we trigger the SIGTRAP.
#define BCRASH() do { \
        BBreakpointTrap(); \
        __builtin_unreachable(); \
    } while (false)

#else // not defined(NDEBUG) && BOS(DARWIN)

#define BCRASH() do { \
    *(int*)0xbbadbeef = 0; \
} while (0);

#endif // defined(NDEBUG) && BOS(DARWIN)

#define BASSERT_IMPL(x) do { \
    if (!(x)) \
        BCRASH(); \
} while (0);

#define RELEASE_BASSERT(x) BASSERT_IMPL(x)

#define UNUSED(x) (void)x

// ===== Release build =====

#if defined(NDEBUG)

#define BASSERT(x)

#define IF_DEBUG(x)

#endif // defined(NDEBUG)


// ===== Debug build =====

#if !defined(NDEBUG)

#define BASSERT(x) BASSERT_IMPL(x)

#define IF_DEBUG(x) x

#endif // !defined(NDEBUG)

#endif // BAssert_h
