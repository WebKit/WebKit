/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCommon_h
#define WebCommon_h

// -----------------------------------------------------------------------------
// Default configuration

#if !defined(WEBKIT_IMPLEMENTATION)
    #define WEBKIT_IMPLEMENTATION 0
#endif

#if !defined(WEBKIT_USING_SKIA)
    #if !defined(__APPLE__)
        #define WEBKIT_USING_SKIA 1
    #else
        #define WEBKIT_USING_SKIA 0
    #endif
#endif

#if !defined(WEBKIT_USING_CG)
    #if defined(__APPLE__)
        #define WEBKIT_USING_CG 1
    #else
        #define WEBKIT_USING_CG 0
    #endif
#endif

#if !defined(WEBKIT_USING_V8)
    #define WEBKIT_USING_V8 1
#endif

#if !defined(WEBKIT_USING_JSC)
    #define WEBKIT_USING_JSC 0
#endif

// -----------------------------------------------------------------------------
// Exported symbols need to be annotated with WEBKIT_API

#if defined(WIN32) && defined(WEBKIT_DLL)
    #if defined(WEBKIT_IMPLEMENTATION)
        #define WEBKIT_API __declspec(dllexport)
    #else
        #define WEBKIT_API __declspec(dllimport)
    #endif
#else
    #define WEBKIT_API
#endif

// -----------------------------------------------------------------------------
// Basic types

#include <stddef.h> // For size_t

namespace WebKit {

    // UTF-16 character type
#if defined(WIN32)
typedef wchar_t WebUChar;
#else
typedef unsigned short WebUChar;
#endif

} // namespace WebKit

#endif
