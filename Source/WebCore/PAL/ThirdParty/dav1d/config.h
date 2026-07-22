/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#define ARCH_AARCH64 0
#if defined(__arm64__) || defined(__aarch64__)
#undef ARCH_AARCH64
#define ARCH_AARCH64 1
#endif

#define ARCH_ARM 0
#if defined(arm) || defined(__arm__) || defined(ARM) || defined(_ARM_)
#undef ARCH_ARM
#define ARCH_ARM 1
#endif

#define ARCH_LOONGARCH 0
#define ARCH_LOONGARCH32 0
#define ARCH_LOONGARCH64 0

#define ARCH_PPC64LE 0

#define ARCH_RISCV 0

#define ARCH_RV32 0

#define ARCH_RV64 0

#define ARCH_X86 0

#define ARCH_X86_32 0
#if defined(__i386__) || defined(i386) || defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL)
#undef ARCH_X86_32
#define ARCH_X86_32 1
#undef ARCH_X86
#define ARCH_X86 1
#endif

#define ARCH_X86_64 0
#if defined(__x86_64__) || defined(_M_X64)
#undef ARCH_X86_64
#define ARCH_X86_64 1
#undef ARCH_X86
#define ARCH_X86 1
#endif

#define CONFIG_16BPC 1

#define CONFIG_8BPC 1

#define CONFIG_LOG 0

#define ENDIANNESS_BIG 0

#if ARCH_AARCH64 && defined(__LP64__)
#define HAVE_ASM 1
#else
#define HAVE_ASM 0
#endif

#if ARCH_AARCH64
#define AS_ARCH_LEVEL armv8.6-a+crc

#define HAVE_AS_ARCH_DIRECTIVE 1
#define HAVE_AS_ARCHEXT_DOTPROD_DIRECTIVE 1
#define HAVE_AS_ARCHEXT_I8MM_DIRECTIVE 1
#define HAVE_AS_ARCHEXT_SVE_DIRECTIVE 1
#define HAVE_AS_ARCHEXT_SVE2_DIRECTIVE 1

#define HAVE_DOTPROD 1
#define HAVE_I8MM 1
#define HAVE_SVE 1
#define HAVE_SVE2 1

#define HAVE_AS_FUNC 0

// Mach-O prepends an underscore to C symbol names, asm also needs to do the same.
#define PREFIX 1

#endif // ARCH_AARCH64

#define HAVE_ELF_AUX_INFO 0

#define HAVE_GETAUXVAL 0

#define HAVE_CLOCK_GETTIME 1

#define HAVE_MEMALIGN 0

#define HAVE_POSIX_MEMALIGN 1

#define HAVE_PTHREAD_GETAFFINITY_NP 1

#define HAVE_PTHREAD_NP_H 0

#define HAVE_PTHREAD_SETNAME_NP 1

#define HAVE_UNISTD_H 1

#define TRIM_DSP_FUNCTIONS 1
