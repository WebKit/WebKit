/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "PerThread.h"
#include <cstddef>

namespace bmalloc {

class IsoTLSEntry;
template<typename Config> class IsoAllocator;
template<typename Config> class IsoDeallocator;

namespace api {
template<typename Type> struct IsoHeap;
}

class IsoTLS {
public:
    template<typename Type>
    static void* allocate(api::IsoHeap<Type>&, bool abortOnFailure);
    
    template<typename Type>
    static void deallocate(api::IsoHeap<Type>&, void* p);
    
    template<typename Type>
    static void ensureHeap(api::IsoHeap<Type>&);
    
    static void scavenge();
    
    template<typename Type>
    static void scavenge(api::IsoHeap<Type>&);

private:
    IsoTLS();
    
    template<typename Config, typename Type>
    static void* allocateImpl(api::IsoHeap<Type>&, bool abortOnFailure);
    
    template<typename Config, typename Type>
    void* allocateFast(api::IsoHeap<Type>&, unsigned offset, bool abortOnFailure);
    
    template<typename Config, typename Type>
    static void* allocateSlow(api::IsoHeap<Type>&, bool abortOnFailure);
    
    template<typename Config, typename Type>
    static void deallocateImpl(api::IsoHeap<Type>&, void* p);
    
    template<typename Config, typename Type>
    void deallocateFast(api::IsoHeap<Type>&, unsigned offset, void* p);
    
    template<typename Config, typename Type>
    static void deallocateSlow(api::IsoHeap<Type>&, void* p);
    
    static IsoTLS* get();
    static void set(IsoTLS*);
    
    template<typename Type>
    static IsoTLS* ensureHeapAndEntries(api::IsoHeap<Type>&);
    
    BEXPORT static IsoTLS* ensureEntries(unsigned offset);

    static void destructor(void* arg); // FIXME implement this
    
    static size_t sizeForCapacity(unsigned capacity);
    static unsigned capacityForSize(size_t size);
    
    size_t size();
    
    template<typename Func>
    void forEachEntry(const Func&);
    
    enum class MallocFallbackState : uint8_t {
        Undecided,
        FallBackToMalloc,
        DoNotFallBack
    };
    
    BEXPORT static MallocFallbackState s_mallocFallbackState;
    
    BEXPORT static void determineMallocFallbackState();
    
    IsoTLSEntry* m_lastEntry { nullptr };
    unsigned m_extent { 0 };
    unsigned m_capacity { 0 };
    char m_data[1];

#if HAVE_PTHREAD_MACHDEP_H
    static constexpr pthread_key_t tlsKey = __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY1;
#else
    BEXPORT static bool s_didInitialize;
    BEXPORT static pthread_key_t s_tlsKey;
#endif
};

} // namespace bmalloc

