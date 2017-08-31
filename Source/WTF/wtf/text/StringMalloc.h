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

#include <wtf/FastMalloc.h>

namespace WTF {

WTF_EXPORT_PRIVATE void* tryStringMalloc(size_t);
WTF_EXPORT_PRIVATE void* stringMalloc(size_t);
WTF_EXPORT_PRIVATE void* stringRealloc(void*, size_t);
WTF_EXPORT_PRIVATE void stringFree(void*);

#define WTF_MAKE_STRING_ALLOCATED \
public: \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::stringMalloc(size); \
    } \
    \
    void operator delete(void* p) \
    { \
        ::WTF::stringFree(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::stringMalloc(size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        ::WTF::stringFree(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
private: \
typedef int __thisIsHereToForceASemicolonAfterThisMacro


struct StringMalloc {
    static void* malloc(size_t size) { return stringMalloc(size); }
    static void* tryMalloc(size_t size) { return tryStringMalloc(size); }
    static void* realloc(void* p, size_t size) { return stringRealloc(p, size); }
    static void free(void* p) { stringFree(p); }
};

} // namespace WTF

using WTF::StringMalloc;
using WTF::stringMalloc;
using WTF::stringRealloc;
using WTF::stringFree;
using WTF::tryStringMalloc;
