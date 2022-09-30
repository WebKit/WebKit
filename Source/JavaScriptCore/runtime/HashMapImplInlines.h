/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ExceptionExpectation.h"
#include "HashMapImpl.h"
#include "JSCJSValueInlines.h"
#include "VMTrapsInlines.h"

namespace JSC {

ALWAYS_INLINE bool areKeysEqual(JSGlobalObject* globalObject, JSValue a, JSValue b)
{
    // We want +0 and -0 to be compared to true here. sameValue() itself doesn't
    // guarantee that, however, we normalize all keys before comparing and storing
    // them in the map. The normalization will convert -0.0 and 0.0 to the integer
    // representation for 0.
    return sameValue(globalObject, a, b);
}

// Note that normalization is inlined in DFG's NormalizeMapKey.
// Keep in sync with the implementation of DFG and FTL normalization.
ALWAYS_INLINE JSValue normalizeMapKey(JSValue key)
{
    if (!key.isNumber()) {
        if (key.isHeapBigInt())
            return tryConvertToBigInt32(key.asHeapBigInt());
        return key;
    }

    if (key.isInt32())
        return key;

    double d = key.asDouble();
    if (std::isnan(d))
        return jsNaN();

    int i = static_cast<int>(d);
    if (i == d) {
        // When a key is -0, we convert it to positive zero.
        // When a key is the double representation for an integer, we convert it to an integer.
        return jsNumber(i);
    }
    // This means key is definitely not negative zero, and it's definitely not a double representation of an integer.
    return key;
}

ALWAYS_INLINE uint32_t wangsInt64Hash(uint64_t key)
{
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return static_cast<unsigned>(key);
}

ALWAYS_INLINE uint32_t jsMapHash(JSBigInt* bigInt)
{
    return bigInt->hash();
}

template<ExceptionExpectation expection>
ALWAYS_INLINE uint32_t jsMapHashImpl(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    ASSERT_WITH_MESSAGE(normalizeMapKey(value) == value, "We expect normalized values flowing into this function.");

    if (value.isString()) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        String wtfString = asString(value)->value(globalObject);
        if constexpr (expection == ExceptionExpectation::CanThrow)
            RETURN_IF_EXCEPTION(scope, UINT_MAX);
        else
            EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
        return wtfString.impl()->hash();
    }

    if (value.isHeapBigInt())
        return jsMapHash(value.asHeapBigInt());

    return wangsInt64Hash(JSValue::encode(value));
}

ALWAYS_INLINE uint32_t jsMapHash(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    return jsMapHashImpl<ExceptionExpectation::CanThrow>(globalObject, vm, value);
}

ALWAYS_INLINE uint32_t jsMapHashForAlreadyHashedValue(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    return jsMapHashImpl<ExceptionExpectation::ShouldNotThrow>(globalObject, vm, value);
}

ALWAYS_INLINE std::optional<uint32_t> concurrentJSMapHash(JSValue key)
{
    key = normalizeMapKey(key);
    if (key.isString()) {
        JSString* string = asString(key);
        if (string->length() > 10 * 1024)
            return std::nullopt;
        const StringImpl* impl = string->tryGetValueImpl();
        if (!impl)
            return std::nullopt;
        return impl->concurrentHash();
    }

    if (key.isHeapBigInt())
        return key.asHeapBigInt()->concurrentHash();

    uint64_t rawValue = JSValue::encode(key);
    return wangsInt64Hash(rawValue);
}

static constexpr uint32_t hashMapInitialCapacity = 4;

ALWAYS_INLINE uint32_t shouldShrink(uint32_t capacity, uint32_t keyCount)
{
    return 8 * keyCount <= capacity && capacity > hashMapInitialCapacity;
}

ALWAYS_INLINE uint32_t shouldRehash(uint32_t capacity, uint32_t keyCount, uint32_t deleteCount)
{
    return 2 * (keyCount + deleteCount) >= capacity;
}

ALWAYS_INLINE uint32_t nextCapacity(uint32_t capacity, uint32_t keyCount)
{
    if (!capacity)
        return hashMapInitialCapacity;

    if (shouldShrink(capacity, keyCount)) {
        ASSERT((capacity / 2) >= hashMapInitialCapacity);
        return capacity / 2;
    }

    if (3 * keyCount <= capacity && capacity > 64) {
        // We stay at the same size if rehashing would cause us to be no more than
        // 1/3rd full. This comes up for programs like this:
        // Say the hash table grew to a key count of 64, causing it to grow to a capacity of 256.
        // Then, the table added 63 items. The load is now 127. Then, 63 items are deleted.
        // The load is still 127. Then, another item is added. The load is now 128, and we
        // decide that we need to rehash. The key count is 65, almost exactly what it was
        // when we grew to a capacity of 256. We don't really need to grow to a capacity
        // of 512 in this situation. Instead, we choose to rehash at the same size. This
        // will bring the load down to 65. We rehash into the same size when we determine
        // that the new load ratio will be under 1/3rd. (We also pick a minumum capacity
        // at which this rule kicks in because otherwise we will be too sensitive to rehashing
        // at the same capacity).
        return capacity;
    }
    return Checked<uint32_t>(capacity) * 2;
}

template <typename HashMapBucketType>
void HashMapImpl<HashMapBucketType>::finishCreation(VM& vm)
{
    ASSERT_WITH_MESSAGE(HashMapBucket<HashMapBucketDataKey>::offsetOfKey() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey(), "We assume this to be true in the DFG and FTL JIT.");
    Base::finishCreation(vm);
    setUpHeadAndTail(vm);
}

template <typename HashMapBucketType>
void HashMapImpl<HashMapBucketType>::finishCreation(JSGlobalObject* globalObject, VM& vm, HashMapImpl* base)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    Base::finishCreation(vm);

    // This size should be the same to the case when you clone the map by calling add() repeatedly.
    uint32_t capacity = (Checked<uint32_t>(base->m_keyCount) * 2) + 1;
    RELEASE_ASSERT(capacity <= (1U << 31));
    capacity = std::max<uint32_t>(WTF::roundUpToPowerOfTwo(capacity), hashMapInitialCapacity);
    makeAndSetNewBuffer(globalObject, capacity, vm);
    RETURN_IF_EXCEPTION(scope, void());

    setUpHeadAndTail(vm);

    HashMapBucketType* bucket = base->m_head.get()->next();
    while (bucket) {
        if (!bucket->deleted()) {
            addNormalizedNonExistingForCloning(globalObject, bucket->key(), HashMapBucketType::extractValue(*bucket));
            RETURN_IF_EXCEPTION(scope, void());
        }
        bucket = bucket->next();
    }
    checkConsistency();
}

template <typename HashMapBucketType>
ALWAYS_INLINE HashMapBucketType** HashMapImpl<HashMapBucketType>::findBucket(JSGlobalObject* globalObject, JSValue key)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    key = normalizeMapKey(key);
    uint32_t hash = jsMapHash(globalObject, vm, key);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return findBucket(globalObject, key, hash);
}

template <typename HashMapBucketType>
ALWAYS_INLINE HashMapBucketType** HashMapImpl<HashMapBucketType>::findBucket(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
{
    ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");
    return findBucketAlreadyHashedAndNormalized(globalObject, key, hash);
}

template <typename HashMapBucketType>
template <typename T>
ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucket<HashMapBucketDataKeyValue>>::value, JSValue>::type HashMapImpl<HashMapBucketType>::get(JSGlobalObject* globalObject, JSValue key)
{
    if (HashMapBucketType** bucket = findBucket(globalObject, key))
        return (*bucket)->value();
    return jsUndefined();
}

template <typename HashMapBucketType>
ALWAYS_INLINE bool HashMapImpl<HashMapBucketType>::has(JSGlobalObject* globalObject, JSValue key)
{
    return !!findBucket(globalObject, key);
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::add(JSGlobalObject* globalObject, JSValue key, JSValue value)
{
    key = normalizeMapKey(key);
    addNormalizedInternal(globalObject, key, value, [&] (HashMapBucketType* bucket) {
        return !isDeleted(bucket) && areKeysEqual(globalObject, key, bucket->key());
    });
}

template <typename HashMapBucketType>
ALWAYS_INLINE HashMapBucketType* HashMapImpl<HashMapBucketType>::addNormalized(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash)
{
    VM& vm = getVM(globalObject);
    ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");
    DEFER_TERMINATION_AND_ASSERT_WITH_MESSAGE(vm, jsMapHash(globalObject, getVM(globalObject), key) == hash, "We expect hash value is what we expect.");

    return addNormalizedInternal(globalObject, key, value, hash, [&] (HashMapBucketType* bucket) {
        return !isDeleted(bucket) && areKeysEqual(globalObject, key, bucket->key());
    });
}

template <typename HashMapBucketType>
ALWAYS_INLINE bool HashMapImpl<HashMapBucketType>::remove(JSGlobalObject* globalObject, JSValue key)
{
    HashMapBucketType** bucket = findBucket(globalObject, key);
    if (!bucket)
        return false;

    VM& vm = getVM(globalObject);
    HashMapBucketType* impl = *bucket;
    impl->next()->setPrev(vm, impl->prev());
    impl->prev()->setNext(vm, impl->next());
    impl->makeDeleted(vm);

    *bucket = deletedValue();

    ++m_deleteCount;
    ASSERT(m_keyCount > 0);
    --m_keyCount;

    if (shouldShrink())
        rehash(globalObject, RehashMode::AfterRemoval);

    return true;
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::clear(VM& vm)
{
    m_keyCount = 0;
    m_deleteCount = 0;
    HashMapBucketType* head = m_head.get();
    HashMapBucketType* bucket = m_head->next();
    HashMapBucketType* tail = m_tail.get();
    while (bucket != tail) {
        HashMapBucketType* next = bucket->next();
        // We restart each iterator by pointing it to the head of the list.
        bucket->setNext(vm, head);
        bucket->makeDeleted(vm);
        bucket = next;
    }
    m_head->setNext(vm, m_tail.get());
    m_tail->setPrev(vm, m_head.get());
    m_buffer.clear();
    m_capacity = 0;
    checkConsistency();
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::setUpHeadAndTail(VM& vm)
{
    m_head.set(vm, this, HashMapBucketType::create(vm));
    m_tail.set(vm, this, HashMapBucketType::create(vm));

    m_head->setNext(vm, m_tail.get());
    m_tail->setPrev(vm, m_head.get());
    ASSERT(m_head->deleted());
    ASSERT(m_tail->deleted());
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::addNormalizedNonExistingForCloning(JSGlobalObject* globalObject, JSValue key, JSValue value)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint32_t hash = jsMapHash(globalObject, vm, key);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();

    addNormalizedNonExistingForCloningInternal(globalObject, key, value, hash);
}

template <typename HashMapBucketType>
ALWAYS_INLINE HashMapBucketType* HashMapImpl<HashMapBucketType>::addNormalizedNonExistingForCloningInternal(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash)
{
    return addNormalizedInternal(globalObject, key, value, hash, [&](HashMapBucketType*) {
        return false;
    });
}

template <typename HashMapBucketType>
template<typename CanUseBucket>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::addNormalizedInternal(JSGlobalObject* globalObject, JSValue key, JSValue value, const CanUseBucket& canUseBucket)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint32_t hash = jsMapHash(globalObject, vm, key);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    addNormalizedInternal(globalObject, key, value, hash, canUseBucket);
}

template <typename HashMapBucketType>
template<typename CanUseBucket>
ALWAYS_INLINE HashMapBucketType* HashMapImpl<HashMapBucketType>::addNormalizedInternal(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash, const CanUseBucket& canUseBucket)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");

    if (!m_capacity) {
        makeAndSetNewBuffer(globalObject, hashMapInitialCapacity, vm);
        RETURN_IF_EXCEPTION(scope, { });
    }

    const uint32_t mask = m_capacity - 1;
    uint32_t index = hash & mask;
    HashMapBucketType** buffer = this->buffer();
    {
        HashMapBucketType* bucket = buffer[index];
        while (!isEmpty(bucket)) {
            if (canUseBucket(bucket)) {
                bucket->setValue(vm, value);
                return bucket;
            }
            index = (index + 1) & mask;
            bucket = buffer[index];
        }
    }

    if (JSC::shouldRehash(m_capacity, m_keyCount + 1, m_deleteCount)) {
        rehash(globalObject, RehashMode::BeforeAddition);
        RETURN_IF_EXCEPTION(scope, { });
        // We ensure that (1) this map does not have deleted keys because of rehashing and (2) this map does not have the same key as |key| input.
        // Thus, we can just search for empty bucket.
        ASSERT(m_capacity);
        ASSERT(isPowerOfTwo(m_capacity));
        const uint32_t mask = m_capacity - 1;
        index = hash & mask;
        buffer = this->buffer();
        while (!isEmpty(buffer[index]))
            index = (index + 1) & mask;
    }

    HashMapBucketType* newEntry = m_tail.get();
    buffer[index] = newEntry;
    newEntry->setKey(vm, key);
    newEntry->setValue(vm, value);
    ASSERT(!newEntry->deleted());
    HashMapBucketType* newTail = HashMapBucketType::create(vm);
    m_tail.set(vm, this, newTail);
    newTail->setPrev(vm, newEntry);
    ASSERT(newTail->deleted());
    newEntry->setNext(vm, newTail);

    ++m_keyCount;
    return newEntry;
}

template <typename HashMapBucketType>
ALWAYS_INLINE HashMapBucketType** HashMapImpl<HashMapBucketType>::findBucketAlreadyHashedAndNormalized(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
{
    if (!m_capacity)
        return nullptr;

    const uint32_t mask = m_capacity - 1;
    uint32_t index = hash & mask;
    HashMapBucketType** buffer = this->buffer();
    HashMapBucketType* bucket = buffer[index];

    while (!isEmpty(bucket)) {
        if (!isDeleted(bucket) && areKeysEqual(globalObject, key, bucket->key()))
            return buffer + index;
        index = (index + 1) & mask;
        bucket = buffer[index];
    }
    return nullptr;
}

template <typename HashMapBucketType>
void HashMapImpl<HashMapBucketType>::rehash(JSGlobalObject* globalObject, RehashMode mode)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint32_t oldCapacity = m_capacity;
    uint32_t newCapacity = nextCapacity(m_capacity, m_keyCount + (mode == RehashMode::BeforeAddition ? 1 : 0));
    ASSERT(newCapacity);

    if (newCapacity != oldCapacity) {
        makeAndSetNewBuffer(globalObject, newCapacity, vm);
        RETURN_IF_EXCEPTION(scope, void());
    } else {
        ASSERT(newCapacity);
        ASSERT(oldCapacity);
        ASSERT(m_capacity == newCapacity);
        ASSERT(m_buffer);
        m_buffer->reset(newCapacity);
        assertBufferIsEmpty(buffer(), newCapacity);
    }

    HashMapBucketType* iter = m_head->next();
    HashMapBucketType* end = m_tail.get();
    const uint32_t mask = m_capacity - 1;
    RELEASE_ASSERT(!(m_capacity & (m_capacity - 1)));
    HashMapBucketType** buffer = this->buffer();
    while (iter != end) {
        uint32_t index = jsMapHashForAlreadyHashedValue(globalObject, vm, iter->key()) & mask;
        EXCEPTION_ASSERT_WITH_MESSAGE(!scope.exception(), "All keys should already be hashed before, so this should not throw because it won't resolve ropes.");
        {
            HashMapBucketType* bucket = buffer[index];
            while (!isEmpty(bucket)) {
                index = (index + 1) & mask;
                bucket = buffer[index];
            }
        }
        buffer[index] = iter;
        iter = iter->next();
    }

    m_deleteCount = 0;

    checkConsistency();
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::checkConsistency() const
{
    if (ASSERT_ENABLED) {
        HashMapBucketType* iter = m_head->next();
        HashMapBucketType* end = m_tail.get();
        uint32_t size = 0;
        while (iter != end) {
            ++size;
            iter = iter->next();
        }
        ASSERT_UNUSED(size, size == m_keyCount);
    }
}

template <typename HashMapBucketType>
void HashMapImpl<HashMapBucketType>::makeAndSetNewBuffer(JSGlobalObject* globalObject, uint32_t newCapacity, VM& vm)
{
    ASSERT(!(newCapacity & (newCapacity - 1)));

    HashMapBufferType* buffer = HashMapBufferType::tryCreate(globalObject, vm, newCapacity);
    if (UNLIKELY(!buffer))
        return;

    m_buffer.set(vm, this, buffer);
    m_capacity = newCapacity;
    assertBufferIsEmpty(this->buffer(), newCapacity);
}

template <typename HashMapBucketType>
ALWAYS_INLINE void HashMapImpl<HashMapBucketType>::assertBufferIsEmpty(HashMapBucketType** buffer, uint32_t capacity)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(capacity);
#if ASSERT_ENABLED
    for (unsigned i = 0; i < capacity; i++)
        ASSERT(isEmpty(buffer[i]));
#endif
}

} // namespace JSC
