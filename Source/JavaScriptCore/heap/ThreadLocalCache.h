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

#include "AllocationFailureMode.h"
#include "LocalAllocator.h"
#include "SecurityOriginToken.h"
#include <wtf/FastMalloc.h>
#include <wtf/FastTLS.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class Heap;
class VM;

class ThreadLocalCache : public ThreadSafeRefCounted<ThreadLocalCache> {
    WTF_MAKE_NONCOPYABLE(ThreadLocalCache);
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    JS_EXPORT_PRIVATE static RefPtr<ThreadLocalCache> create(Heap&, SecurityOriginToken = uniqueSecurityOriginToken());
    
    JS_EXPORT_PRIVATE virtual ~ThreadLocalCache();

    static RefPtr<ThreadLocalCache> get(VM&);
    
    // This is designed to be fast enough that you could even call it before every allocation, by
    // optimizing for the case that you're just installing the cache that is already installed. This
    // assumes a relatively small number of caches or low chance of actual context switch combined
    // with possibly high rate of "I may have context switched" sites that call this out of paranoia.
    void install(VM&, RefPtr<ThreadLocalCache>* = nullptr);
    
    static LocalAllocator& allocator(VM&, size_t offset);
    
    template<typename SuccessFunc, typename FailureFunc>
    static void tryGetAllocator(VM&, size_t offset, const SuccessFunc&, const FailureFunc&);
    
    static ptrdiff_t offsetOfSizeInData() { return OBJECT_OFFSETOF(Data, size); }
    static ptrdiff_t offsetOfFirstAllocatorInData() { return OBJECT_OFFSETOF(Data, allocator); }
    
    SecurityOriginToken securityOriginToken() const { return m_securityOriginToken; }

protected:    
    JS_EXPORT_PRIVATE ThreadLocalCache(Heap&, SecurityOriginToken);
    
private:
    friend class VM;
    
    struct Data {
        size_t size;
        ThreadLocalCache* cache;
        LocalAllocator allocator[1];
    };

    static Data* getImpl(VM&);
    
    Data* allocateData();
    void destroyData(Data*);
    static LocalAllocator& allocator(Data& data, size_t offset);

    void installSlow(VM&, RefPtr<ThreadLocalCache>*);
    static void installData(VM&, Data*);
    
    LocalAllocator& allocatorSlow(VM&, size_t offset);
    
    static void destructor(void*);
    
    // When installed, we ref() the cache. Uninstalling deref()s the cache. TLS destruction deref()s
    // the cache. When the cache is destructed, it needs to return all of its state to the GC. I
    // think that just means stopAllocating(), but with some seriously heavy caveats having to do
    // with when it's valid to make that call. Alternatively, we could RELEASE_ASSERT that the cache
    // is empty when we destruct it. This is correct for caches that are kept live by GC objects and
    // it's correct for immortal caches.
    Heap& m_heap;
    Data* m_data { nullptr };
    
    SecurityOriginToken m_securityOriginToken;

#if USE(FAST_TLS_FOR_TLC)
    static const pthread_key_t tlsKey = WTF_GC_TLC_KEY;
#endif
};

} // namespace JSC

