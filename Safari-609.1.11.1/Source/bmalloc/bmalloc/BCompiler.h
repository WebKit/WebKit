/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

/* BCOMPILER() - the compiler being used to build the project */
#define BCOMPILER(BFEATURE) (defined BCOMPILER_##BFEATURE && BCOMPILER_##BFEATURE)

/* BCOMPILER_HAS_CLANG_FEATURE() - whether the compiler supports a particular language or library feature. */
/* http://clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension */
#ifdef __has_feature
#define BCOMPILER_HAS_CLANG_FEATURE(x) __has_feature(x)
#else
#define BCOMPILER_HAS_CLANG_FEATURE(x) 0
#endif

#define BASAN_ENABLED BCOMPILER_HAS_CLANG_FEATURE(address_sanitizer)

/* BCOMPILER(GCC_COMPATIBLE) - GNU Compiler Collection or compatibles */

#if defined(__GNUC__)
#define BCOMPILER_GCC_COMPATIBLE 1
#endif

/* BNO_RETURN */

#if !defined(BNO_RETURN) && BCOMPILER(GCC_COMPATIBLE)
#define BNO_RETURN __attribute((__noreturn__))
#endif

#if !defined(BNO_RETURN)
#define BNO_RETURN
#endif

/* BFALLTHROUGH */

#if !defined(BFALLTHROUGH) && defined(__cplusplus) && defined(__has_cpp_attribute)

#if __has_cpp_attribute(fallthrough)
#define BFALLTHROUGH [[fallthrough]]
#elif __has_cpp_attribute(clang::fallthrough)
#define BFALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#define BFALLTHROUGH [[gnu::fallthrough]]
#endif

#endif // !defined(BFALLTHROUGH) && defined(__cplusplus) && defined(__has_cpp_attribute)

#if !defined(BFALLTHROUGH)
#define BFALLTHROUGH
#endif
