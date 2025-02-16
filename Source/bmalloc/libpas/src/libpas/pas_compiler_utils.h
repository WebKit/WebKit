/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#if defined(__clang__)
#define PAS_COMPILER_CLANG 1
#endif

/* PAS_ALLOW_UNSAFE_BUFFER_USAGE */
#if PAS_COMPILER_CLANG
#define PAS_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wunsafe-buffer-usage\"")

#define PAS_ALLOW_UNSAFE_BUFFER_USAGE_END \
    _Pragma("clang diagnostic pop")
#else
#define PAS_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#define PAS_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

/* PAS_UNSAFE_BUFFER_USAGE */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif

#if PAS_COMPILER_CLANG
#if __has_cpp_attribute(clang::unsafe_buffer_usage)
#define PAS_UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]
#elif __has_attribute(unsafe_buffer_usage)
#define PAS_UNSAFE_BUFFER_USAGE __attribute__((__unsafe_buffer_usage__))
#else
#define PAS_UNSAFE_BUFFER_USAGE
#endif
#else
#define PAS_UNSAFE_BUFFER_USAGE
#endif
