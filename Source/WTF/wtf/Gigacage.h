/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC
#define GIGACAGE_ENABLED 0

namespace Gigacage {

const size_t primitiveGigacageMask = 0;
const size_t jsValueGigacageMask = 0;

enum Kind {
    Primitive,
    JSValue,
    NumberOfKinds
};

inline void ensureGigacage() { }
inline void disablePrimitiveGigacage() { }
inline bool shouldBeEnabled() { return false; }

inline void addPrimitiveDisableCallback(void (*)(void*), void*) { }
inline void removePrimitiveDisableCallback(void (*)(void*), void*) { }

inline void forbidDisablingPrimitiveGigacage() { }

ALWAYS_INLINE const char* name(Kind kind)
{
    switch (kind) {
    case Primitive:
        return "Primitive";
    case JSValue:
        return "JSValue";
    case NumberOfKinds:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

ALWAYS_INLINE bool contains(const void*) { return false; }
ALWAYS_INLINE bool disablingPrimitiveGigacageIsForbidden() { return false; }
ALWAYS_INLINE bool isEnabled(Kind) { return false; }
ALWAYS_INLINE size_t mask(Kind) { return 0; }
ALWAYS_INLINE size_t footprint(Kind) { return 0; }
ALWAYS_INLINE size_t maxSize(Kind) { return 0; }
ALWAYS_INLINE size_t size(Kind) { return 0; }

template<typename T>
inline T* caged(Kind, T* ptr) { return ptr; }
template<typename T>
inline T* cagedMayBeNull(Kind, T* ptr) { return ptr; }

inline bool isCaged(Kind, const void*) { return false; }

inline void* tryAlignedMalloc(Kind, size_t alignment, size_t size) { return tryFastAlignedMalloc(alignment, size); }
inline void alignedFree(Kind, void* p) { fastAlignedFree(p); }
WTF_EXPORT_PRIVATE void* tryMalloc(Kind, size_t size);
WTF_EXPORT_PRIVATE void* tryRealloc(Kind, void*, size_t);
inline void free(Kind, void* p) { fastFree(p); }

WTF_EXPORT_PRIVATE void* tryAllocateZeroedVirtualPages(Kind, size_t size);
WTF_EXPORT_PRIVATE void freeVirtualPages(Kind, void* basePtr, size_t size);

} // namespace Gigacage
#else
#include <bmalloc/Gigacage.h>

namespace Gigacage {

WTF_EXPORT_PRIVATE void* tryAlignedMalloc(Kind, size_t alignment, size_t size);
WTF_EXPORT_PRIVATE void alignedFree(Kind, void*);
WTF_EXPORT_PRIVATE void* tryMalloc(Kind, size_t);
WTF_EXPORT_PRIVATE void* tryRealloc(Kind, void*, size_t);
WTF_EXPORT_PRIVATE void free(Kind, void*);

WTF_EXPORT_PRIVATE void* tryAllocateZeroedVirtualPages(Kind, size_t size);
WTF_EXPORT_PRIVATE void freeVirtualPages(Kind, void* basePtr, size_t size);

} // namespace Gigacage
#endif

namespace Gigacage {

WTF_EXPORT_PRIVATE void* tryMallocArray(Kind, size_t numElements, size_t elementSize);

WTF_EXPORT_PRIVATE void* malloc(Kind, size_t);
WTF_EXPORT_PRIVATE void* mallocArray(Kind, size_t numElements, size_t elementSize);

} // namespace Gigacage


