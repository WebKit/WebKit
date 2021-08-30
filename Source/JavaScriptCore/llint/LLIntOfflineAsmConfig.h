/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "LLIntCommon.h"
#include <wtf/Assertions.h>
#include <wtf/Gigacage.h>

#if ENABLE(C_LOOP)
#if !OS(WINDOWS)
#define OFFLINE_ASM_C_LOOP 1
#define OFFLINE_ASM_C_LOOP_WIN 0
#else
#define OFFLINE_ASM_C_LOOP 0
#define OFFLINE_ASM_C_LOOP_WIN 1
#endif
#define OFFLINE_ASM_X86 0
#define OFFLINE_ASM_X86_WIN 0
#define OFFLINE_ASM_ARMv7 0
#define OFFLINE_ASM_ARM64 0
#define OFFLINE_ASM_ARM64E 0
#define OFFLINE_ASM_X86_64 0
#define OFFLINE_ASM_X86_64_WIN 0
#define OFFLINE_ASM_ARMv7k 0
#define OFFLINE_ASM_ARMv7s 0
#define OFFLINE_ASM_MIPS 0
#define OFFLINE_ASM_RISCV64 0

#else // ENABLE(C_LOOP)

#define OFFLINE_ASM_C_LOOP 0
#define OFFLINE_ASM_C_LOOP_WIN 0

#if CPU(X86) && !COMPILER(MSVC)
#define OFFLINE_ASM_X86 1
#else
#define OFFLINE_ASM_X86 0
#endif

#if CPU(X86) && COMPILER(MSVC)
#define OFFLINE_ASM_X86_WIN 1
#else
#define OFFLINE_ASM_X86_WIN 0
#endif

#ifdef __ARM_ARCH_7K__
#define OFFLINE_ASM_ARMv7k 1
#else
#define OFFLINE_ASM_ARMv7k 0
#endif

#ifdef __ARM_ARCH_7S__
#define OFFLINE_ASM_ARMv7s 1
#else
#define OFFLINE_ASM_ARMv7s 0
#endif

#if CPU(ARM_THUMB2)
#define OFFLINE_ASM_ARMv7 1
#else
#define OFFLINE_ASM_ARMv7 0
#endif

#if CPU(X86_64) && !COMPILER(MSVC)
#define OFFLINE_ASM_X86_64 1
#else
#define OFFLINE_ASM_X86_64 0
#endif

#if CPU(X86_64) && COMPILER(MSVC)
#define OFFLINE_ASM_X86_64_WIN 1
#else
#define OFFLINE_ASM_X86_64_WIN 0
#endif

#if CPU(MIPS)
#define OFFLINE_ASM_MIPS 1
#else
#define OFFLINE_ASM_MIPS 0
#endif

#if CPU(ARM64)
#define OFFLINE_ASM_ARM64 1
#else
#define OFFLINE_ASM_ARM64 0
#endif

#if CPU(ARM64E)
#define OFFLINE_ASM_ARM64E 1
#undef OFFLINE_ASM_ARM64
#define OFFLINE_ASM_ARM64 0 // Pretend that ARM64 and ARM64E are mutually exclusive to please the offlineasm.
#else
#define OFFLINE_ASM_ARM64E 0
#endif

#if CPU(RISCV64)
#define OFFLINE_ASM_RISCV64 1
#else
#define OFFLINE_ASM_RISCV64 0
#endif

#if CPU(MIPS)
#ifdef WTF_MIPS_PIC
#define S(x) #x
#define SX(x) S(x)
#define OFFLINE_ASM_CPLOAD(reg) \
    ".set noreorder\n" \
    ".cpload " SX(reg) "\n" \
    ".set reorder\n"
#else
#define OFFLINE_ASM_CPLOAD(reg)
#endif
#endif

#endif // ENABLE(C_LOOP)

#if USE(JSVALUE64)
#define OFFLINE_ASM_JSVALUE64 1
#else
#define OFFLINE_ASM_JSVALUE64 0
#endif

#if USE(BIGINT32)
#define OFFLINE_ASM_BIGINT32 1
#else
#define OFFLINE_ASM_BIGINT32 0
#endif

#if CPU(ADDRESS64)
#define OFFLINE_ASM_ADDRESS64 1
#else
#define OFFLINE_ASM_ADDRESS64 0
#endif

#if ASSERT_ENABLED
#define OFFLINE_ASM_ASSERT_ENABLED 1
#else
#define OFFLINE_ASM_ASSERT_ENABLED 0
#endif

#if LLINT_TRACING
#define OFFLINE_ASM_TRACING 1
#else
#define OFFLINE_ASM_TRACING 0
#endif

#define OFFLINE_ASM_GIGACAGE_ENABLED GIGACAGE_ENABLED

#if ENABLE(WEBASSEMBLY)
#define OFFLINE_ASM_WEBASSEMBLY 1
#else
#define OFFLINE_ASM_WEBASSEMBLY 0
#endif

#if ENABLE(WEBASSEMBLY_B3JIT)
#define OFFLINE_ASM_WEBASSEMBLY_B3JIT 1
#else
#define OFFLINE_ASM_WEBASSEMBLY_B3JIT 0
#endif

#if HAVE(FAST_TLS)
#define OFFLINE_ASM_HAVE_FAST_TLS 1
#else
#define OFFLINE_ASM_HAVE_FAST_TLS 0
#endif
