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
void IsoDeallocator<Config>::deallocate(void* ptr)
{
    static constexpr bool verbose = false;
    if (verbose)
        fprintf(stderr, "%p: deallocating %p of size %u\n", &IsoPage<Config>::pageFor(ptr)->heap(), ptr, Config::objectSize);

    if (m_objectLog.size() == m_objectLog.capacity())
        scavenge();
    
    m_objectLog.push(ptr);
}

template<typename Config>
BNO_INLINE void IsoDeallocator<Config>::scavenge()
{
    std::lock_guard<Mutex> locker(*m_lock);
    
    for (void* ptr : m_objectLog)
        IsoPage<Config>::pageFor(ptr)->free(ptr);
    m_objectLog.clear();
}

} // namespace bmalloc

