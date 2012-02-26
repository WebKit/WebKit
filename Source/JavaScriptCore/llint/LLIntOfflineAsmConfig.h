/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef LLIntOfflineAsmConfig_h
#define LLIntOfflineAsmConfig_h

#include "LLIntCommon.h"
#include <wtf/Assertions.h>
#include <wtf/InlineASM.h>
#include <wtf/Platform.h>

#if CPU(X86)
#define OFFLINE_ASM_X86 1
#else
#define OFFLINE_ASM_X86 0
#endif

#if CPU(ARM_THUMB2)
#define OFFLINE_ASM_ARMv7 1
#else
#define OFFLINE_ASM_ARMv7 0
#endif

#if CPU(X86_64)
#define OFFLINE_ASM_X86_64 1
#else
#define OFFLINE_ASM_X86_64 0
#endif

#if USE(JSVALUE64)
#define OFFLINE_ASM_JSVALUE64 1
#else
#define OFFLINE_ASM_JSVALUE64 0
#endif

#if !ASSERT_DISABLED
#define OFFLINE_ASM_ASSERT_ENABLED 1
#else
#define OFFLINE_ASM_ASSERT_ENABLED 0
#endif

#if CPU(BIG_ENDIAN)
#define OFFLINE_ASM_BIG_ENDIAN 1
#else
#define OFFLINE_ASM_BIG_ENDIAN 0
#endif

#if LLINT_OSR_TO_JIT
#define OFFLINE_ASM_JIT_ENABLED 1
#else
#define OFFLINE_ASM_JIT_ENABLED 0
#endif

#if LLINT_EXECUTION_TRACING
#define OFFLINE_ASM_EXECUTION_TRACING 1
#else
#define OFFLINE_ASM_EXECUTION_TRACING 0
#endif

#if LLINT_ALWAYS_ALLOCATE_SLOW
#define OFFLINE_ASM_ALWAYS_ALLOCATE_SLOW 1
#else
#define OFFLINE_ASM_ALWAYS_ALLOCATE_SLOW 0
#endif

#if ENABLE(VALUE_PROFILER)
#define OFFLINE_ASM_VALUE_PROFILER 1
#else
#define OFFLINE_ASM_VALUE_PROFILER 0
#endif

#if CPU(ARM_THUMB2)
#define OFFLINE_ASM_GLOBAL_LABEL(label)          \
    ".globl " SYMBOL_STRING(label) "\n"          \
    HIDE_SYMBOL(name) "\n"                       \
    ".thumb\n"                                   \
    ".thumb_func " THUMB_FUNC_PARAM(label) "\n"  \
    SYMBOL_STRING(label) ":\n"
#else
#define OFFLINE_ASM_GLOBAL_LABEL(label)         \
    ".globl " SYMBOL_STRING(label) "\n"         \
    HIDE_SYMBOL(name) "\n"                      \
    SYMBOL_STRING(label) ":\n"
#endif

#endif // LLIntOfflineAsmConfig_h
