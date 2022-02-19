/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_CONFIG_PREFIX_H
#define PAS_CONFIG_PREFIX_H

#include "pas_platform.h"

#if defined(ENABLE_PAS_TESTING)
#define __PAS_ENABLE_TESTING 1
#else
#define __PAS_ENABLE_TESTING 0
#endif

#if (defined(__arm64__) && defined(__APPLE__)) || defined(__aarch64__) || defined(__arm64e__)
#define __PAS_ARM64 1
#if defined(__arm64e__)
#define __PAS_ARM64E 1
#else
#define __PAS_ARM64E 0
#endif
#else
#define __PAS_ARM64 0
#define __PAS_ARM64E 0
#endif

#if (defined(arm) || defined(__arm__) || defined(ARM) || defined(_ARM_)) && !__PAS_ARM64
#define __PAS_ARM32 1
#else
#define __PAS_ARM32 0
#endif

#define __PAS_ARM (!!__PAS_ARM64 || !!__PAS_ARM32)

#if defined(__i386__) || defined(i386) || defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL)
#define __PAS_X86 1
#else
#define __PAS_X86 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define __PAS_X86_64 1
#else
#define __PAS_X86_64 0
#endif

#if defined(__riscv)
#define __PAS_RISCV 1
#else
#define __PAS_RISCV 0
#endif

#endif /* PAS_CONFIG_PREFIX_H */
