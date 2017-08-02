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

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC
#define GIGACAGE_MASK 0
#define GIGACAGE_ENABLED 0

extern "C" {
extern WTF_EXPORTDATA const void* g_gigacageBasePtr;
}

namespace Gigacage {

inline void ensureGigacage() { }
inline void disableGigacage() { }

inline void addDisableCallback(void (*)(void*), void*) { }
inline void removeDisableCallback(void (*)(void*), void*) { }

template<typename T>
inline T* caged(T* ptr) { return ptr; }

inline bool isCaged(const void*) { return false; }

inline void* tryAlignedMalloc(size_t alignment, size_t size) { return tryFastAlignedMalloc(alignment, size); }
inline void alignedFree(void* p) { fastAlignedFree(p); }
WTF_EXPORT_PRIVATE void* tryMalloc(size_t size);
inline void free(void* p) { fastFree(p); }

WTF_EXPORT_PRIVATE void* tryAllocateVirtualPages(size_t size);
WTF_EXPORT_PRIVATE void freeVirtualPages(void* basePtr, size_t size);

} // namespace Gigacage
#else
#include <bmalloc/Gigacage.h>

namespace Gigacage {

WTF_EXPORT_PRIVATE void* tryAlignedMalloc(size_t alignment, size_t size);
WTF_EXPORT_PRIVATE void alignedFree(void*);
WTF_EXPORT_PRIVATE void* tryMalloc(size_t);
WTF_EXPORT_PRIVATE void free(void*);

WTF_EXPORT_PRIVATE void* tryAllocateVirtualPages(size_t size);
WTF_EXPORT_PRIVATE void freeVirtualPages(void* basePtr, size_t size);

} // namespace Gigacage
#endif

