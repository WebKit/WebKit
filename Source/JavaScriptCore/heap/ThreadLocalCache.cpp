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

#include "config.h"
#include "ThreadLocalCache.h"

#include "ThreadLocalCacheInlines.h"
#include "ThreadLocalCacheLayout.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

RefPtr<ThreadLocalCache> ThreadLocalCache::create(Heap& heap)
{
    return adoptRef(new ThreadLocalCache(heap));
}

ThreadLocalCache::ThreadLocalCache(Heap& heap)
    : m_heap(heap)
{
    m_data = allocateData();
}

ThreadLocalCache::~ThreadLocalCache()
{
    destroyData(m_data);
}

ThreadLocalCache::Data* ThreadLocalCache::allocateData()
{
    size_t oldSize = m_data ? m_data->size : 0;
    ThreadLocalCacheLayout::Snapshot layout = m_heap.threadLocalCacheLayout().snapshot();
    
    Data* result = static_cast<Data*>(fastMalloc(OBJECT_OFFSETOF(Data, allocator) + layout.size));
    result->size = layout.size;
    result->cache = this;
    for (size_t offset = 0; offset < oldSize; offset += sizeof(LocalAllocator))
        new (&allocator(*result, offset)) LocalAllocator(WTFMove(allocator(*m_data, offset)));
    for (size_t offset = oldSize; offset < layout.size; offset += sizeof(LocalAllocator))
        new (&allocator(*result, offset)) LocalAllocator(this, layout.directories[offset / sizeof(LocalAllocator)]);
    return result;
}

void ThreadLocalCache::destroyData(Data* data)
{
    for (size_t offset = 0; offset < data->size; offset += sizeof(LocalAllocator))
        allocator(*data, offset).~LocalAllocator();
    fastFree(data);
}

void ThreadLocalCache::installSlow(VM& vm, RefPtr<ThreadLocalCache>* previous)
{
#if USE(FAST_TLS_FOR_TLC)
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] () {
            pthread_key_init_np(tlsKey, destructor);
        });
#endif
    
    ref();
    
    if (ThreadLocalCache::Data* oldCacheData = getImpl(vm)) {
        ThreadLocalCache* oldCache = oldCacheData->cache;
        if (previous)
            *previous = adoptRef(oldCache);
        else
            oldCache->deref();
    }
    
    installData(vm, m_data);
}

void ThreadLocalCache::installData(VM& vm, Data* data)
{
#if USE(FAST_TLS_FOR_TLC)
    UNUSED_PARAM(vm);
    _pthread_setspecific_direct(tlsKey, data);
#else
    vm.threadLocalCacheData = data;
#endif
}

LocalAllocator& ThreadLocalCache::allocatorSlow(VM& vm, size_t offset)
{
    Data* oldData = m_data;
    m_data = allocateData();
    destroyData(oldData);
    installData(vm, m_data);
    RELEASE_ASSERT(offset < m_data->size);
    return allocator(*m_data, offset);
}

void ThreadLocalCache::destructor(void* arg)
{
    if (!arg)
        return;
    Data* data = static_cast<Data*>(arg);
    data->cache->deref();
}

} // namespace JSC

