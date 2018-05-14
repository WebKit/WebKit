/*
 * Copyright (C) 2011, 2012, 2014 Apple Inc. All rights reserved.
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

#ifndef WTF_Compiler_h
#define WTF_Compiler_h

/* COMPILER() - the compiler being used to build the project */
#define COMPILER(WTF_FEATURE) (defined WTF_COMPILER_##WTF_FEATURE  && WTF_COMPILER_##WTF_FEATURE)

/* COMPILER_SUPPORTS() - whether the compiler being used to build the project supports the given feature. */
#define COMPILER_SUPPORTS(WTF_COMPILER_FEATURE) (defined WTF_COMPILER_SUPPORTS_##WTF_COMPILER_FEATURE  && WTF_COMPILER_SUPPORTS_##WTF_COMPILER_FEATURE)

/* COMPILER_QUIRK() - whether the compiler being used to build the project requires a given quirk. */
#define COMPILER_QUIRK(WTF_COMPILER_QUIRK) (defined WTF_COMPILER_QUIRK_##WTF_COMPILER_QUIRK  && WTF_COMPILER_QUIRK_##WTF_COMPILER_QUIRK)

/* COMPILER_HAS_CLANG_BUILTIN() - whether the compiler supports a particular clang builtin. */
#ifdef __has_builtin
#define COMPILER_HAS_CLANG_BUILTIN(x) __has_builtin(x)
#else
#define COMPILER_HAS_CLANG_BUILTIN(x) 0
#endif

/* COMPILER_HAS_CLANG_FEATURE() - whether the compiler supports a particular language or library feature. */
/* http://clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension */
#ifdef __has_feature
#define COMPILER_HAS_CLANG_FEATURE(x) __has_feature(x)
#else
#define COMPILER_HAS_CLANG_FEATURE(x) 0
#endif

/* COMPILER_HAS_CLANG_DECLSPEC() - whether the compiler supports a Microsoft style __declspec attribute. */
/* https://clang.llvm.org/docs/LanguageExtensions.html#has-declspec-attribute */
#ifdef __has_declspec_attribute
#define COMPILER_HAS_CLANG_DECLSPEC(x) __has_declspec_attribute(x)
#else
#define COMPILER_HAS_CLANG_DECLSPEC(x) 0
#endif

/* ==== COMPILER() - primary detection of the compiler being used to build the project, in alphabetical order ==== */

/* COMPILER(CLANG) - Clang  */

#if defined(__clang__)
#define WTF_COMPILER_CLANG 1
#define WTF_COMPILER_SUPPORTS_BLOCKS COMPILER_HAS_CLANG_FEATURE(blocks)
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT COMPILER_HAS_CLANG_FEATURE(c_static_assert)
#define WTF_COMPILER_SUPPORTS_CXX_EXCEPTIONS COMPILER_HAS_CLANG_FEATURE(cxx_exceptions)
#define WTF_COMPILER_SUPPORTS_BUILTIN_IS_TRIVIALLY_COPYABLE COMPILER_HAS_CLANG_FEATURE(is_trivially_copyable)

#ifdef __cplusplus
#if __cplusplus <= 201103L
#define WTF_CPP_STD_VER 11
#elif __cplusplus <= 201402L
#define WTF_CPP_STD_VER 14
#elif __cplusplus <= 201703L
#define WTF_CPP_STD_VER 17
#endif
#endif

#endif // defined(__clang__)

/* COMPILER(GCC_OR_CLANG) - GNU Compiler Collection or Clang */
#if defined(__GNUC__)
#define WTF_COMPILER_GCC_OR_CLANG 1
#endif

/* COMPILER(GCC) - GNU Compiler Collection */
/* Note: This section must come after the Clang section since we check !COMPILER(CLANG) here. */
#if COMPILER(GCC_OR_CLANG) && !COMPILER(CLANG)
#define WTF_COMPILER_GCC 1

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#define GCC_VERSION_AT_LEAST(major, minor, patch) (GCC_VERSION >= (major * 10000 + minor * 100 + patch))

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT 1
#endif

#endif /* COMPILER(GCC) */

#if COMPILER(GCC_OR_CLANG) && defined(NDEBUG) && !defined(__OPTIMIZE__) && !defined(RELEASE_WITHOUT_OPTIMIZATIONS)
#error "Building release without compiler optimizations: WebKit will be slow. Set -DRELEASE_WITHOUT_OPTIMIZATIONS if this is intended."
#endif

/* COMPILER(MINGW) - MinGW GCC */

#if defined(__MINGW32__)
#define WTF_COMPILER_MINGW 1
#include <_mingw.h>
#endif

/* COMPILER(MINGW64) - mingw-w64 GCC - used as additional check to exclude mingw.org specific functions */

/* Note: This section must come after the MinGW section since we check COMPILER(MINGW) here. */

#if COMPILER(MINGW) && defined(__MINGW64_VERSION_MAJOR) /* best way to check for mingw-w64 vs mingw.org */
#define WTF_COMPILER_MINGW64 1
#endif

/* COMPILER(MSVC) - Microsoft Visual C++ */

#if defined(_MSC_VER)

#define WTF_COMPILER_MSVC 1

#if _MSC_VER < 1910
#error "Please use a newer version of Visual Studio. WebKit requires VS2017 or newer to compile."
#endif

#endif

#if !COMPILER(CLANG) && !COMPILER(MSVC)
#define WTF_COMPILER_QUIRK_CONSIDERS_UNREACHABLE_CODE 1
#endif

/* ==== COMPILER_SUPPORTS - additional compiler feature detection, in alphabetical order ==== */

/* COMPILER_SUPPORTS(EABI) */

#if defined(__ARM_EABI__) || defined(__EABI__)
#define WTF_COMPILER_SUPPORTS_EABI 1
#endif

/* ASAN_ENABLED and SUPPRESS_ASAN */

#define ASAN_ENABLED COMPILER_HAS_CLANG_FEATURE(address_sanitizer)

#if ASAN_ENABLED
#define SUPPRESS_ASAN __attribute__((no_sanitize_address))
#else
#define SUPPRESS_ASAN
#endif

/* ==== Compiler-independent macros for various compiler features, in alphabetical order ==== */

/* ALWAYS_INLINE */

#if !defined(ALWAYS_INLINE) && COMPILER(GCC_OR_CLANG) && defined(NDEBUG) && !COMPILER(MINGW)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#endif

#if !defined(ALWAYS_INLINE) && COMPILER(MSVC) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#endif

#if !defined(ALWAYS_INLINE)
#define ALWAYS_INLINE inline
#endif

#if COMPILER(MSVC)
#define ALWAYS_INLINE_EXCEPT_MSVC inline
#else
#define ALWAYS_INLINE_EXCEPT_MSVC ALWAYS_INLINE
#endif

/* WTF_EXTERN_C_{BEGIN, END} */

#ifdef __cplusplus
#define WTF_EXTERN_C_BEGIN extern "C" {
#define WTF_EXTERN_C_END }
#else
#define WTF_EXTERN_C_BEGIN
#define WTF_EXTERN_C_END
#endif

/* FALLTHROUGH */

#if !defined(FALLTHROUGH) && defined(__cplusplus) && defined(__has_cpp_attribute)

#if __has_cpp_attribute(fallthrough)
#define FALLTHROUGH [[fallthrough]]
#elif __has_cpp_attribute(clang::fallthrough)
#define FALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#define FALLTHROUGH [[gnu::fallthrough]]
#endif

#elif !defined(FALLTHROUGH) && !defined(__cplusplus)

#if COMPILER(GCC)
#if GCC_VERSION_AT_LEAST(7, 0, 0)
#define FALLTHROUGH __attribute__ ((fallthrough))
#endif
#endif

#endif // !defined(FALLTHROUGH) && defined(__cplusplus) && defined(__has_cpp_attribute)

#if !defined(FALLTHROUGH)
#define FALLTHROUGH
#endif

/* LIKELY */

#if !defined(LIKELY) && COMPILER(GCC_OR_CLANG)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#endif

#if !defined(LIKELY)
#define LIKELY(x) (x)
#endif

/* NEVER_INLINE */

#if !defined(NEVER_INLINE) && COMPILER(GCC_OR_CLANG)
#define NEVER_INLINE __attribute__((__noinline__))
#endif

#if !defined(NEVER_INLINE) && COMPILER(MSVC)
#define NEVER_INLINE __declspec(noinline)
#endif

#if !defined(NEVER_INLINE)
#define NEVER_INLINE
#endif

/* NO_RETURN */

#if !defined(NO_RETURN) && COMPILER(GCC_OR_CLANG)
#define NO_RETURN __attribute((__noreturn__))
#endif

#if !defined(NO_RETURN) && COMPILER(MSVC)
#define NO_RETURN __declspec(noreturn)
#endif

#if !defined(NO_RETURN)
#define NO_RETURN
#endif

/* NOT_TAIL_CALLED */

#if !defined(NOT_TAIL_CALLED) && defined(__has_attribute)
#if __has_attribute(not_tail_called)
#define NOT_TAIL_CALLED __attribute__((not_tail_called))
#endif
#endif

#if !defined(NOT_TAIL_CALLED)
#define NOT_TAIL_CALLED
#endif

/* RETURNS_NONNULL */
#if !defined(RETURNS_NONNULL) && COMPILER(GCC_OR_CLANG)
#define RETURNS_NONNULL __attribute__((returns_nonnull))
#endif

#if !defined(RETURNS_NONNULL)
#define RETURNS_NONNULL
#endif

/* NO_RETURN_WITH_VALUE */

#if !defined(NO_RETURN_WITH_VALUE) && !COMPILER(MSVC)
#define NO_RETURN_WITH_VALUE NO_RETURN
#endif

#if !defined(NO_RETURN_WITH_VALUE)
#define NO_RETURN_WITH_VALUE
#endif

/* OBJC_CLASS */

#if !defined(OBJC_CLASS) && defined(__OBJC__)
#define OBJC_CLASS @class
#endif

#if !defined(OBJC_CLASS)
#define OBJC_CLASS class
#endif

/* PURE_FUNCTION */

#if !defined(PURE_FUNCTION) && COMPILER(GCC_OR_CLANG)
#define PURE_FUNCTION __attribute__((__pure__))
#endif

#if !defined(PURE_FUNCTION)
#define PURE_FUNCTION
#endif

/* UNUSED_FUNCTION */

#if !defined(UNUSED_FUNCTION) && COMPILER(GCC_OR_CLANG)
#define UNUSED_FUNCTION __attribute__((unused))
#endif

#if !defined(UNUSED_FUNCTION)
#define UNUSED_FUNCTION
#endif

/* REFERENCED_FROM_ASM */

#if !defined(REFERENCED_FROM_ASM) && COMPILER(GCC_OR_CLANG)
#define REFERENCED_FROM_ASM __attribute__((__used__))
#endif

#if !defined(REFERENCED_FROM_ASM)
#define REFERENCED_FROM_ASM
#endif

/* UNLIKELY */

#if !defined(UNLIKELY) && COMPILER(GCC_OR_CLANG)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#if !defined(UNLIKELY)
#define UNLIKELY(x) (x)
#endif

/* UNUSED_LABEL */

/* Keep the compiler from complaining for a local label that is defined but not referenced. */
/* Helpful when mixing hand-written and autogenerated code. */

#if !defined(UNUSED_LABEL) && COMPILER(MSVC)
#define UNUSED_LABEL(label) if (false) goto label
#endif

#if !defined(UNUSED_LABEL)
#define UNUSED_LABEL(label) UNUSED_PARAM(&& label)
#endif

/* UNUSED_PARAM */

#if !defined(UNUSED_PARAM) && COMPILER(MSVC)
#define UNUSED_PARAM(variable) (void)&variable
#endif

#if !defined(UNUSED_PARAM)
#define UNUSED_PARAM(variable) (void)variable
#endif

/* WARN_UNUSED_RETURN */

#if !defined(WARN_UNUSED_RETURN) && COMPILER(GCC_OR_CLANG)
#define WARN_UNUSED_RETURN __attribute__((__warn_unused_result__))
#endif

#if !defined(WARN_UNUSED_RETURN)
#define WARN_UNUSED_RETURN
#endif

#if !defined(__has_include) && COMPILER(MSVC)
#define __has_include(path) 0
#endif

#endif /* WTF_Compiler_h */
