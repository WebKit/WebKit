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

#include "IsoHeapImpl.h"
#include "IsoTLS.h"

namespace bmalloc {

template<typename Type>
void* IsoTLS::allocate(api::IsoHeap<Type>& handle, bool abortOnFailure)
{
    return allocator<typename api::IsoHeap<Type>::Config>(handle).allocate(abortOnFailure);
}

template<typename Type>
void IsoTLS::deallocate(api::IsoHeap<Type>& handle, void* p)
{
    if (!p)
        return;
    deallocator<typename api::IsoHeap<Type>::Config>(handle).deallocate(p);
}

template<typename Type>
void IsoTLS::scavenge(api::IsoHeap<Type>& handle)
{
    IsoTLS* tls = get();
    if (!tls)
        return;
    if (!handle.isInitialized())
        return;
    unsigned offset = handle.allocatorOffset();
    if (offset < tls->m_extent)
        reinterpret_cast<IsoAllocator<typename api::IsoHeap<Type>::Config>*>(tls->m_data + offset)->scavenge();
    offset = handle.deallocatorOffset();
    if (offset < tls->m_extent)
        reinterpret_cast<IsoDeallocator<typename api::IsoHeap<Type>::Config>*>(tls->m_data + offset)->scavenge();
    handle.impl().scavengeNow();
}

template<typename Config, typename Type>
IsoAllocator<Config>& IsoTLS::allocator(api::IsoHeap<Type>& handle)
{
    unsigned offset = handle.allocatorOffset();
    IsoTLS* tls = get();
    if (!tls || offset >= tls->m_extent) {
        tls = ensureHeapAndEntries(handle);
        offset = handle.allocatorOffset();
    }
    return *reinterpret_cast<IsoAllocator<Config>*>(tls->m_data + offset);
}

template<typename Config, typename Type>
IsoDeallocator<Config>& IsoTLS::deallocator(api::IsoHeap<Type>& handle)
{
    unsigned offset = handle.deallocatorOffset();
    IsoTLS* tls = get();
    RELEASE_BASSERT(offset < tls->m_extent);
    return *reinterpret_cast<IsoDeallocator<Config>*>(tls->m_data + offset);
}

inline IsoTLS* IsoTLS::get()
{
#if HAVE_PTHREAD_MACHDEP_H
    return static_cast<IsoTLS*>(_pthread_getspecific_direct(tlsKey));
#else
    if (!s_didInitialize)
        return nullptr;
    return static_cast<IsoTLS*>(pthread_getspecific(s_tlsKey));
#endif
}

inline void IsoTLS::set(IsoTLS* tls)
{
#if HAVE_PTHREAD_MACHDEP_H
    _pthread_setspecific_direct(tlsKey, tls);
#else
    pthread_setspecific(s_tlsKey, tls);
#endif
}

template<typename Type>
void IsoTLS::ensureHeap(api::IsoHeap<Type>& handle)
{
    if (!handle.isInitialized()) {
        std::lock_guard<StaticMutex> locker(handle.m_initializationLock);
        if (!handle.isInitialized()) {
            auto* heap = new IsoHeapImpl<typename api::IsoHeap<Type>::Config>();
            std::atomic_thread_fence(std::memory_order_seq_cst);
            handle.setAllocatorOffset(heap->allocatorOffset());
            handle.setDeallocatorOffset(heap->deallocatorOffset());
            handle.m_impl = heap;
        }
    }
}

template<typename Type>
BNO_INLINE IsoTLS* IsoTLS::ensureHeapAndEntries(api::IsoHeap<Type>& handle)
{
    RELEASE_BASSERT(
        !get()
        || handle.allocatorOffset() >= get()->m_extent
        || handle.deallocatorOffset() >= get()->m_extent);
    ensureHeap(handle);
    return ensureEntries(std::max(handle.allocatorOffset(), handle.deallocatorOffset()));
}

} // namespace bmalloc

