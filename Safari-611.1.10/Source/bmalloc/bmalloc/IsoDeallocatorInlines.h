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

#include "BInline.h"
#include "IsoDeallocator.h"
#include "IsoPage.h"
#include "IsoSharedPage.h"
#include "Mutex.h"
#include <mutex>

namespace bmalloc {

template<typename Config>
IsoDeallocator<Config>::IsoDeallocator(Mutex& lock)
    : m_lock(&lock)
{
}

template<typename Config>
IsoDeallocator<Config>::~IsoDeallocator()
{
}

template<typename Config>
template<typename Type>
void IsoDeallocator<Config>::deallocate(api::IsoHeap<Type>& handle, void* ptr)
{
    static constexpr bool verbose = false;
    if (verbose)
        fprintf(stderr, "%p: deallocating %p of size %u\n", &IsoPage<Config>::pageFor(ptr)->heap(), ptr, Config::objectSize);

    // For allocation from shared pages, we deallocate immediately rather than batching deallocations with object log.
    // The batching delays the reclamation of the shared cells, which can make allocator mistakenly think that "we exhaust shared
    // cells because this is allocated a lot". Since the number of shared cells are limited, this immediate deallocation path
    // should be rarely taken. If we see frequent malloc-and-free pattern, we tier up the allocator from shared mode to fast mode.
    IsoPageBase* page = IsoPageBase::pageFor(ptr);
    if (page->isShared()) {
        LockHolder locker(*m_lock);
        static_cast<IsoSharedPage*>(page)->free<Config>(locker, handle, ptr);
        return;
    }

    if (m_objectLog.size() == m_objectLog.capacity())
        scavenge();
    
    m_objectLog.push(ptr);
}

template<typename Config>
BNO_INLINE void IsoDeallocator<Config>::scavenge()
{
    LockHolder locker(*m_lock);
    
    for (void* ptr : m_objectLog)
        IsoPage<Config>::pageFor(ptr)->free(locker, ptr);
    m_objectLog.clear();
}

} // namespace bmalloc

