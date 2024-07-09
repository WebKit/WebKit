/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "HashMapHelper.h"
#include "WeakMapImpl.h"

namespace JSC {

ALWAYS_INLINE uint32_t jsWeakMapHash(JSCell* key)
{
    return wangsInt64Hash(JSValue::encode(key));
}

ALWAYS_INLINE uint32_t nextCapacityAfterBatchRemoval(uint32_t capacity, uint32_t keyCount)
{
    while (shouldShrink(capacity, keyCount))
        capacity = nextCapacity(capacity, keyCount);
    return capacity;
}

static ALWAYS_INLINE bool canBeHeldWeakly(JSValue value)
{
    // https://tc39.es/proposal-symbols-as-weakmap-keys/#sec-canbeheldweakly-abstract-operation
    if (value.isObject())
        return true;
    if (!value.isSymbol())
        return false;
    return !asSymbol(value)->uid().isRegistered();
}

template <typename WeakMapBucket>
ALWAYS_INLINE void WeakMapImpl<WeakMapBucket>::add(VM& vm, JSCell* key, JSValue value)
{
    DisallowGC disallowGC;
    add(vm, key, value, jsWeakMapHash(key));
}

template <typename WeakMapBucket>
ALWAYS_INLINE void WeakMapImpl<WeakMapBucket>::add(VM& vm, JSCell* key, JSValue value, uint32_t hash)
{
    DisallowGC disallowGC;
    ASSERT_WITH_MESSAGE(jsWeakMapHash(key) == hash, "We expect hash value is what we expect.");

    addInternal(vm, key, value, hash);
    if (shouldRehashAfterAdd())
        rehash();
}

// Note that this function can be executed in parallel as long as the mutator stops.
template<typename WeakMapBucket>
void WeakMapImpl<WeakMapBucket>::finalizeUnconditionally(VM& vm, CollectionScope)
{
    auto* buffer = this->buffer();
    for (uint32_t index = 0; index < m_capacity; ++index) {
        auto* bucket = buffer + index;
        if (bucket->isEmpty() || bucket->isDeleted())
            continue;

        if (vm.heap.isMarked(bucket->key()))
            continue;

        bucket->makeDeleted();
        ++m_deleteCount;
        RELEASE_ASSERT(m_keyCount > 0);
        --m_keyCount;
    }

    if (shouldShrink())
        rehash(RehashMode::RemoveBatching);
}

template<typename WeakMapBucket>
void WeakMapImpl<WeakMapBucket>::rehash(RehashMode mode)
{
    // Since shrinking is done just after GC runs (finalizeUnconditionally), WeakMapImpl::rehash()
    // function must not touch any GC related features. This is why we do not allocate WeakMapBuffer
    // in auxiliary buffer.

    uint32_t oldCapacity = m_capacity;
    MallocPtr<WeakMapBufferType> oldBuffer = WTFMove(m_buffer);

    uint32_t capacity = m_capacity;
    if (mode == RehashMode::RemoveBatching) {
        ASSERT(shouldShrink());
        capacity = nextCapacityAfterBatchRemoval(capacity, m_keyCount);
    } else
        capacity = nextCapacity(capacity, m_keyCount);
    makeAndSetNewBuffer(capacity);

    auto* buffer = this->buffer();
    const uint32_t mask = m_capacity - 1;
    for (uint32_t oldIndex = 0; oldIndex < oldCapacity; ++oldIndex) {
        auto* entry = oldBuffer->buffer() + oldIndex;
        if (entry->isEmpty() || entry->isDeleted())
            continue;

        uint32_t index = jsWeakMapHash(entry->key()) & mask;
        WeakMapBucket* bucket = buffer + index;
        while (!bucket->isEmpty()) {
            index = (index + 1) & mask;
            bucket = buffer + index;
        }
        bucket->copyFrom(*entry);
    }

    m_deleteCount = 0;

    checkConsistency();
}

template<typename WeakMapBucket>
ALWAYS_INLINE uint32_t WeakMapImpl<WeakMapBucket>::shouldRehashAfterAdd() const
{
    return JSC::shouldRehash(m_capacity, m_keyCount, m_deleteCount);
}

} // namespace JSC
