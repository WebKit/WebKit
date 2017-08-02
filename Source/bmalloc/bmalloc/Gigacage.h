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

#include "BExport.h"
#include "BPlatform.h"
#include <inttypes.h>

// The Gigacage is 64GB.
#define GIGACAGE_MASK 0xfffffffffllu
#define GIGACAGE_SIZE (GIGACAGE_MASK + 1)

// FIXME: Consider making this 32GB, in case unsigned 32-bit indices find their way into indexed accesses.
// https://bugs.webkit.org/show_bug.cgi?id=175062
#define GIGACAGE_RUNWAY (16llu * 1024 * 1024 * 1024)

#if BOS(DARWIN) && BCPU(X86_64)
#define GIGACAGE_ENABLED 1
#else
#define GIGACAGE_ENABLED 0
#endif

extern "C" BEXPORT void* g_gigacageBasePtr;

namespace Gigacage {

BEXPORT void ensureGigacage();

BEXPORT void disableGigacage();

// This will call the disable callback immediately if the Gigacage is currently disabled.
BEXPORT void addDisableCallback(void (*)(void*), void*);
BEXPORT void removeDisableCallback(void (*)(void*), void*);

template<typename T>
T* caged(T* ptr)
{
    void* gigacageBasePtr = g_gigacageBasePtr;
    if (!gigacageBasePtr)
        return ptr;
    return reinterpret_cast<T*>(
        reinterpret_cast<uintptr_t>(gigacageBasePtr) + (
            reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(GIGACAGE_MASK)));
}

inline bool isCaged(const void* ptr)
{
    return caged(ptr) == ptr;
}

BEXPORT bool shouldBeEnabled();

} // namespace Gigacage


