/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "DebugHeap.h"

#include "Algorithm.h"
#include "BAssert.h"
#include "BPlatform.h"
#include "VMAllocate.h"
#include <cstdlib>
#include <thread>

namespace bmalloc {

DebugHeap* debugHeapCache { nullptr };
    
DEFINE_STATIC_PER_PROCESS_STORAGE(DebugHeap);

#if BOS(DARWIN)

DebugHeap::DebugHeap(const LockHolder&)
    : m_zone(malloc_create_zone(0, 0))
    , m_pageSize(vmPageSize())
{
    malloc_set_zone_name(m_zone, "WebKit Using System Malloc");
}

void* DebugHeap::malloc(size_t size, FailureAction action)
{
    void* result = malloc_zone_malloc(m_zone, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* DebugHeap::memalign(size_t alignment, size_t size, FailureAction action)
{
    void* result = malloc_zone_memalign(m_zone, alignment, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* DebugHeap::realloc(void* object, size_t size, FailureAction action)
{
    void* result = malloc_zone_realloc(m_zone, object, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void DebugHeap::free(void* object)
{
    malloc_zone_free(m_zone, object);
}

void DebugHeap::scavenge()
{
    // Currently |goal| does not affect on the behavior of malloc_zone_pressure_relief if (1) we only scavenge one zone and (2) it is not nanomalloc.
    constexpr size_t goal = 0;
    malloc_zone_pressure_relief(m_zone, goal);
}

void DebugHeap::dump()
{
    constexpr bool verbose = true;
    malloc_zone_print(m_zone, verbose);
}

#else

DebugHeap::DebugHeap(const LockHolder&)
    : m_pageSize(vmPageSize())
{
}

void* DebugHeap::malloc(size_t size, FailureAction action)
{
    void* result = ::malloc(size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* DebugHeap::memalign(size_t alignment, size_t size, FailureAction action)
{
    void* result;
    if (posix_memalign(&result, alignment, size))
        RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* DebugHeap::realloc(void* object, size_t size, FailureAction action)
{
    void* result = ::realloc(object, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void DebugHeap::free(void* object)
{
    ::free(object);
}

void DebugHeap::scavenge()
{
}

void DebugHeap::dump()
{
}

#endif

// FIXME: This looks an awful lot like the code in wtf/Gigacage.cpp for large allocation.
// https://bugs.webkit.org/show_bug.cgi?id=175086

void* DebugHeap::memalignLarge(size_t alignment, size_t size)
{
    alignment = roundUpToMultipleOf(m_pageSize, alignment);
    size = roundUpToMultipleOf(m_pageSize, size);
    void* result = tryVMAllocate(alignment, size);
    if (!result)
        return nullptr;
    {
        LockHolder locker(mutex());
        m_sizeMap[result] = size;
    }
    return result;
}

void DebugHeap::freeLarge(void* base)
{
    if (!base)
        return;
    
    size_t size;
    {
        LockHolder locker(mutex());
        size = m_sizeMap[base];
        size_t numErased = m_sizeMap.erase(base);
        RELEASE_BASSERT(numErased == 1);
    }
    
    vmDeallocate(base, size);
}

} // namespace bmalloc
