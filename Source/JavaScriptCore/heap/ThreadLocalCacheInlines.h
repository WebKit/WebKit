/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ThreadLocalCache.h"
#include "VM.h"

namespace JSC {

inline ThreadLocalCache::Data* ThreadLocalCache::getImpl(VM& vm)
{
#if USE(FAST_TLS_FOR_TLC)
    UNUSED_PARAM(vm);
    return static_cast<Data*>(_pthread_getspecific_direct(tlsKey));
#else
    return vm.threadLocalCacheData;
#endif
}

inline RefPtr<ThreadLocalCache> ThreadLocalCache::get(VM& vm)
{
    ThreadLocalCache::Data* data = getImpl(vm);
    if (LIKELY(data))
        return data->cache;
    return nullptr;
}

inline void ThreadLocalCache::install(VM& vm, RefPtr<ThreadLocalCache>* previous)
{
    if (getImpl(vm) == m_data) {
        if (previous)
            *previous = nullptr;
        return;
    }
    installSlow(vm, previous);
}

inline LocalAllocator& ThreadLocalCache::allocator(VM& vm, size_t offset)
{
    ThreadLocalCache::Data* data = getImpl(vm);
    if (LIKELY(offset < data->size))
        return allocator(*data, offset);
    return data->cache->allocatorSlow(vm, offset);
}

template<typename SuccessFunc, typename FailureFunc>
void ThreadLocalCache::tryGetAllocator(VM& vm, size_t offset, const SuccessFunc& successFunc, const FailureFunc& failureFunc)
{
    ThreadLocalCache::Data* data = getImpl(vm);
    if (LIKELY(offset < data->size))
        successFunc(allocator(*data, offset));
    else
        failureFunc();
}

inline LocalAllocator& ThreadLocalCache::allocator(Data& data, size_t offset)
{
    return *bitwise_cast<LocalAllocator*>(bitwise_cast<char*>(&data.allocator[0]) + offset);
}

} // namespace JSC

