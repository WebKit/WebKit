/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

/* ==== COMPILER() - the compiler being used to build the project ==== */

/* COMPILER(CLANG) - Clang  */
#if defined(__clang__)
#define WTF_COMPILER_CLANG 1

/* Specific compiler features */
#define WTF_COMPILER_SUPPORTS_CXX_VARIADIC_TEMPLATES __has_feature(cxx_variadic_templates)
#define WTF_COMPILER_SUPPORTS_CXX_RVALUE_REFERENCES __has_feature(cxx_rvalue_references)
#define WTF_COMPILER_SUPPORTS_CXX_DELETED_FUNCTIONS __has_feature(cxx_deleted_functions)
#define WTF_COMPILER_SUPPORTS_CXX_NULLPTR __has_feature(cxx_nullptr)
#define WTF_COMPILER_SUPPORTS_CXX_EXPLICIT_CONVERSIONS __has_feature(cxx_explicit_conversions)
#define WTF_COMPILER_SUPPORTS_BLOCKS __has_feature(blocks)
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT __has_feature(c_static_assert)
#define WTF_COMPILER_SUPPORTS_CXX_STATIC_ASSERT __has_feature(cxx_static_assert)
#define WTF_COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL __has_feature(cxx_override_control)
#define WTF_COMPILER_SUPPORTS_CXX_STRONG_ENUMS __has_feature(cxx_strong_enums)
#define WTF_COMPILER_SUPPORTS_CXX_REFERENCE_QUALIFIED_FUNCTIONS __has_feature(cxx_reference_qualified_functions)
#define WTF_COMPILER_SUPPORTS_CXX_AUTO_TYPE __has_feature(cxx_auto_type)
#define WTF_COMPILER_SUPPORTS_CXX_GENERALIZED_INITIALIZERS __has_feature(cxx_generalized_initializers)

#endif

/* COMPILER(MSVC) - Microsoft Visual C++ */
#if defined(_MSC_VER)
#if _MSC_VER < 1800
#error "Please use a newer version of Visual Studio. WebKit requires VS2013 or newer to compile."
#endif
#define WTF_COMPILER_MSVC 1

/* Specific compiler features */
#if !COMPILER(CLANG)
#define WTF_COMPILER_SUPPORTS_CXX_NULLPTR 1
#endif

#if !COMPILER(CLANG)
#define WTF_COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL 1
#endif

#define WTF_COMPILER_SUPPORTS_CXX_RVALUE_REFERENCES 1
#define WTF_COMPILER_SUPPORTS_CXX_STATIC_ASSERT 1
#define WTF_COMPILER_SUPPORTS_CXX_AUTO_TYPE 1
#define WTF_COMPILER_SUPPORTS_CXX_STRONG_ENUMS 1
#define WTF_COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL 1
#define WTF_COMPILER_SUPPORTS_CXX_DELETED_FUNCTIONS 1
#define WTF_COMPILER_SUPPORTS_CXX_EXPLICIT_CONVERSIONS 1
#define WTF_COMPILER_SUPPORTS_CXX_GENERALIZED_INITIALIZERS 1
#define WTF_COMPILER_SUPPORTS_CXX_VARIADIC_TEMPLATES 1

#endif /* defined(_MSC_VER) */

/* COMPILER(GCCE) - GNU Compiler Collection for Embedded */
#if defined(__GCCE__)
#define WTF_COMPILER_GCCE 1
#define GCCE_VERSION (__GCCE__ * 10000 + __GCCE_MINOR__ * 100 + __GCCE_PATCHLEVEL__)
#define GCCE_VERSION_AT_LEAST(major, minor, patch) (GCCE_VERSION >= (major * 10000 + minor * 100 + patch))
#endif

/* COMPILER(GCC) - GNU Compiler Collection */
#if defined(__GNUC__)
#define WTF_COMPILER_GCC 1
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#define GCC_VERSION_AT_LEAST(major, minor, patch) (GCC_VERSION >= (major * 10000 + minor * 100 + patch))
#else
/* Define this for !GCC compilers, just so we can write things like GCC_VERSION_AT_LEAST(4, 1, 0). */
#define GCC_VERSION_AT_LEAST(major, minor, patch) 0
#endif

/* Specific compiler features */
#if COMPILER(GCC) && !COMPILER(CLANG)
#if !GCC_VERSION_AT_LEAST(4, 7, 0)
#error "Please use a newer version of GCC. WebKit requires GCC 4.7.0 or newer to compile."
#endif
#if GCC_VERSION_AT_LEAST(4, 8, 0)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
/* C11 support */
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT 1
#endif
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (defined(__cplusplus) && __cplusplus >= 201103L)
/* C++11 support */
#define WTF_COMPILER_SUPPORTS_CXX_RVALUE_REFERENCES 1
#define WTF_COMPILER_SUPPORTS_CXX_STATIC_ASSERT 1
#define WTF_COMPILER_SUPPORTS_CXX_VARIADIC_TEMPLATES 1
#define WTF_COMPILER_SUPPORTS_CXX_AUTO_TYPE 1
#define WTF_COMPILER_SUPPORTS_CXX_DELETED_FUNCTIONS 1
#define WTF_COMPILER_SUPPORTS_CXX_EXPLICIT_CONVERSIONS 1
#define WTF_COMPILER_SUPPORTS_CXX_NULLPTR 1
/* Strong enums should work from gcc 4.4, but doesn't seem to support some operators */
#define WTF_COMPILER_SUPPORTS_CXX_STRONG_ENUMS 1
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#define WTF_COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL 1
#endif /* defined(__GXX_EXPERIMENTAL_CXX0X__) || (defined(__cplusplus) && __cplusplus >= 201103L) */
#endif /* COMPILER(GCC) */

/* COMPILER(MINGW) - MinGW GCC */
/* COMPILER(MINGW64) - mingw-w64 GCC - only used as additional check to exclude mingw.org specific functions */
#if defined(__MINGW32__)
#define WTF_COMPILER_MINGW 1
#include <_mingw.h> /* private MinGW header */
    #if defined(__MINGW64_VERSION_MAJOR) /* best way to check for mingw-w64 vs mingw.org */
        #define WTF_COMPILER_MINGW64 1
    #endif /* __MINGW64_VERSION_MAJOR */
#endif /* __MINGW32__ */

/* COMPILER(SUNCC) */
#if defined(__SUNPRO_CC) || defined(__SUNPRO_C)
#define WTF_COMPILER_SUNCC 1
#endif

/* ABI */
#if defined(__ARM_EABI__) || defined(__EABI__)
#define WTF_COMPILER_SUPPORTS_EABI 1
#endif

/* ==== Compiler features ==== */

/* Required C++11 features. We can remove these once they've been required for some time */

#ifdef __cplusplus
#if !COMPILER_SUPPORTS(CXX_RVALUE_REFERENCES)
#error "Please use a compiler that supports C++11 rvalue references."
#endif
#if !COMPILER_SUPPORTS(CXX_STATIC_ASSERT)
#error "Please use a compiler that supports C++11 static_assert."
#endif
#if !COMPILER_SUPPORTS(CXX_AUTO_TYPE)
#error "Please use a compiler that supports C++11 auto."
#endif
#if !COMPILER_SUPPORTS(CXX_VARIADIC_TEMPLATES)
#error "Please use a compiler that supports C++11 variadic templates."
#endif
#endif

/* PURE_FUNCTION */

#if COMPILER(GCC)
#define PURE_FUNCTION __attribute__ ((__pure__))
#else
#define PURE_FUNCTION
#endif

/* ALWAYS_INLINE */

#ifndef ALWAYS_INLINE
#if COMPILER(GCC) && defined(NDEBUG) && !COMPILER(MINGW)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif COMPILER(MSVC) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif
#endif


/* NEVER_INLINE */

#ifndef NEVER_INLINE
#if COMPILER(GCC)
#define NEVER_INLINE __attribute__((__noinline__))
#elif COMPILER(MSVC) || COMPILER(RVCT)
#define NEVER_INLINE __declspec(noinline)
#else
#define NEVER_INLINE
#endif
#endif


/* UNLIKELY */

#ifndef UNLIKELY
#if COMPILER(GCC)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif
#endif


/* LIKELY */

#ifndef LIKELY
#if COMPILER(GCC)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define LIKELY(x) (x)
#endif
#endif


/* NO_RETURN */


#ifndef NO_RETURN
#if COMPILER(GCC)
#define NO_RETURN __attribute((__noreturn__))
#elif COMPILER(MSVC)
#define NO_RETURN __declspec(noreturn)
#else
#define NO_RETURN
#endif
#endif


/* NO_RETURN_WITH_VALUE */

#ifndef NO_RETURN_WITH_VALUE
#if !COMPILER(MSVC)
#define NO_RETURN_WITH_VALUE NO_RETURN
#else
#define NO_RETURN_WITH_VALUE
#endif
#endif


/* WARN_UNUSED_RETURN */

#if COMPILER(GCC)
#define WARN_UNUSED_RETURN __attribute__ ((warn_unused_result))
#else
#define WARN_UNUSED_RETURN
#endif

/* OVERRIDE and FINAL */

#if COMPILER_SUPPORTS(CXX_OVERRIDE_CONTROL)
#define OVERRIDE override
#else
#define OVERRIDE
#endif

#if COMPILER_SUPPORTS(CXX_OVERRIDE_CONTROL) && !COMPILER_QUIRK(FINAL_IS_BUGGY)
#if COMPILER_QUIRK(FINAL_IS_CALLED_SEALED)
#define FINAL sealed
#define final sealed
#else
#define FINAL final
#endif
#else
#define FINAL
#endif

#if COMPILER_SUPPORTS(CXX_DELETED_FUNCTIONS)
#define WTF_DELETED_FUNCTION = delete
#else
#define WTF_DELETED_FUNCTION
#endif

/* REFERENCED_FROM_ASM */

#ifndef REFERENCED_FROM_ASM
#if COMPILER(GCC)
#define REFERENCED_FROM_ASM __attribute__((used))
#else
#define REFERENCED_FROM_ASM
#endif
#endif

/* OBJC_CLASS */

#ifndef OBJC_CLASS
#ifdef __OBJC__
#define OBJC_CLASS @class
#else
#define OBJC_CLASS class
#endif
#endif

/* UNUSED_PARAM */

#if COMPILER(MSVC)
#define UNUSED_PARAM(variable) (void)&variable
#else
#define UNUSED_PARAM(variable) (void)variable
#endif

/* UNUSED_LABEL */

/* This is to keep the compiler from complaining when for local labels are
 declared but not referenced. For example, this can happen with code that
 works with auto-generated code.
 */
#if COMPILER(MSVC)
#define UNUSED_LABEL(label) if (false) goto label
#else
#define UNUSED_LABEL(label) UNUSED_PARAM(&& label)
#endif

#endif /* WTF_Compiler_h */
