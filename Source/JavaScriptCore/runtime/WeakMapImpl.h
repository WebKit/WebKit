/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "ExceptionHelpers.h"
#include "HashMapImpl.h"
#include "JSObject.h"
#include <wtf/JSValueMalloc.h>
#include <wtf/MallocPtr.h>

namespace JSC {

struct WeakMapBucketDataKey {
    static const HashTableType Type = HashTableType::Key;
    WriteBarrier<JSObject> key;
};
static_assert(sizeof(WeakMapBucketDataKey) == sizeof(void*), "");

struct WeakMapBucketDataKeyValue {
    static const HashTableType Type = HashTableType::KeyValue;
    WriteBarrier<JSObject> key;
#if USE(JSVALUE32_64)
    uint32_t padding;
#endif
    WriteBarrier<Unknown> value;
};
static_assert(sizeof(WeakMapBucketDataKeyValue) == 16, "");

ALWAYS_INLINE uint32_t jsWeakMapHash(JSObject* key)
{
    return wangsInt64Hash(JSValue::encode(key));
}

ALWAYS_INLINE uint32_t nextCapacityAfterBatchRemoval(uint32_t capacity, uint32_t keyCount)
{
    while (shouldShrink(capacity, keyCount))
        capacity = nextCapacity(capacity, keyCount);
    return capacity;
}

template <typename Data>
class WeakMapBucket {
public:
    ALWAYS_INLINE void setKey(VM& vm, JSCell* owner, JSObject* key)
    {
        m_data.key.set(vm, owner, key);
    }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value>::type setValue(VM& vm, JSCell* owner, JSValue value)
    {
        m_data.value.set(vm, owner, value);
    }
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKey>::value>::type setValue(VM&, JSCell*, JSValue) { }

    ALWAYS_INLINE JSObject* key() const { return m_data.key.get(); }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value, JSValue>::type value() const
    {
        return m_data.value.get();
    }
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKey>::value, JSValue>::type value() const { return JSValue(); }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value>::type copyFrom(const WeakMapBucket& from)
    {
        m_data.key.copyFrom(from.m_data.key);
        m_data.value.setWithoutWriteBarrier(from.m_data.value.get());
    }
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKey>::value>::type copyFrom(const WeakMapBucket& from)
    {
        m_data.key.copyFrom(from.m_data.key);
    }

    static ptrdiff_t offsetOfKey()
    {
        return OBJECT_OFFSETOF(WeakMapBucket, m_data) + OBJECT_OFFSETOF(Data, key);
    }

    template <typename T = Data>
    static typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value, ptrdiff_t>::type offsetOfValue()
    {
        return OBJECT_OFFSETOF(WeakMapBucket, m_data) + OBJECT_OFFSETOF(Data, value);
    }

    template <typename T = Data>
    ALWAYS_INLINE static typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value, JSValue>::type extractValue(const WeakMapBucket& bucket)
    {
        return bucket.value();
    }

    template <typename T = Data>
    ALWAYS_INLINE static typename std::enable_if<std::is_same<T, WeakMapBucketDataKey>::value, JSValue>::type extractValue(const WeakMapBucket&)
    {
        return JSValue();
    }

    bool isEmpty()
    {
        return !m_data.key.unvalidatedGet();
    }

    static JSObject* deletedKey()
    {
        return bitwise_cast<JSObject*>(static_cast<uintptr_t>(-3));
    }

    bool isDeleted()
    {
        return m_data.key.unvalidatedGet() == deletedKey();
    }

    void makeDeleted()
    {
        m_data.key.setWithoutWriteBarrier(deletedKey());
        clearValue();
    }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value>::type visitAggregate(SlotVisitor& visitor)
    {
        visitor.append(m_data.value);
    }

private:
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKeyValue>::value>::type clearValue()
    {
        m_data.value.clear();
    }
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucketDataKey>::value>::type clearValue() { }

    Data m_data;
};

template <typename BucketType>
class WeakMapBuffer {
public:
    WeakMapBuffer() = delete;

    static size_t allocationSize(Checked<size_t> capacity)
    {
        return (capacity * sizeof(BucketType)).unsafeGet();
    }

    ALWAYS_INLINE BucketType* buffer() const
    {
        return bitwise_cast<BucketType*>(this);
    }

    static MallocPtr<WeakMapBuffer, JSValueMalloc> create(uint32_t capacity)
    {
        size_t allocationSize = WeakMapBuffer::allocationSize(capacity);
        auto buffer = MallocPtr<WeakMapBuffer, JSValueMalloc>::malloc(allocationSize);
        buffer->reset(capacity);
        return buffer;
    }

    ALWAYS_INLINE void reset(uint32_t capacity)
    {
        memset(this, 0, allocationSize(capacity));
    }
};

template <typename WeakMapBucketType>
class WeakMapImpl : public JSDestructibleObject {
    using Base = JSDestructibleObject;
    using WeakMapBufferType = WeakMapBuffer<WeakMapBucketType>;

public:
    using BucketType = WeakMapBucketType;

    static void destroy(JSCell*);

    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t estimatedSize(JSCell*, VM&);

    WeakMapImpl(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    static constexpr uint32_t initialCapacity = 4;

    void finishCreation(VM& vm)
    {
        ASSERT_WITH_MESSAGE(WeakMapBucket<WeakMapBucketDataKey>::offsetOfKey() == WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfKey(), "We assume this to be true in the DFG and FTL JIT.");

        Base::finishCreation(vm);

        auto locker = holdLock(cellLock());
        makeAndSetNewBuffer(locker, initialCapacity);
    }

    // WeakMap operations must not cause GC. We model operations in DFG based on this guarantee.
    // This guarantee is ensured by DisallowGC.

    template <typename T = WeakMapBucketType>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, WeakMapBucket<WeakMapBucketDataKeyValue>>::value, JSValue>::type get(JSObject* key)
    {
        DisallowGC disallowGC;
        if (WeakMapBucketType* bucket = findBucket(key))
            return bucket->value();
        return jsUndefined();
    }

    ALWAYS_INLINE bool has(JSObject* key)
    {
        DisallowGC disallowGC;
        return !!findBucket(key);
    }

    ALWAYS_INLINE void add(VM& vm, JSObject* key, JSValue value = JSValue())
    {
        DisallowGC disallowGC;
        add(vm, key, value, jsWeakMapHash(key));
    }

    ALWAYS_INLINE void add(VM& vm, JSObject* key, JSValue value, uint32_t hash)
    {
        DisallowGC disallowGC;
        ASSERT_WITH_MESSAGE(jsWeakMapHash(key) == hash, "We expect hash value is what we expect.");

        addInternal(vm, key, value, hash);
        if (shouldRehashAfterAdd())
            rehash();
    }

    ALWAYS_INLINE bool remove(JSObject* key)
    {
        DisallowGC disallowGC;
        WeakMapBucketType* bucket = findBucket(key);
        if (!bucket)
            return false;

        bucket->makeDeleted();

        ++m_deleteCount;
        RELEASE_ASSERT(m_keyCount > 0);
        --m_keyCount;

        if (shouldShrink())
            rehash();

        return true;
    }

    ALWAYS_INLINE uint32_t size() const
    {
        return m_keyCount;
    }

    void takeSnapshot(MarkedArgumentBuffer&, unsigned limit = 0);

    static ptrdiff_t offsetOfBuffer()
    {
        return OBJECT_OFFSETOF(WeakMapImpl<WeakMapBucketType>, m_buffer);
    }

    static ptrdiff_t offsetOfCapacity()
    {
        return OBJECT_OFFSETOF(WeakMapImpl<WeakMapBucketType>, m_capacity);
    }

    static constexpr bool isWeakMap()
    {
        return std::is_same<WeakMapBucketType, JSC::WeakMapBucket<WeakMapBucketDataKeyValue>>::value;
    }

    static constexpr bool isWeakSet()
    {
        return std::is_same<WeakMapBucketType, JSC::WeakMapBucket<WeakMapBucketDataKey>>::value;
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        if (isWeakMap())
            return vm.weakMapSpace<mode>();
        return vm.weakSetSpace<mode>();
    }

    static void visitOutputConstraints(JSCell*, SlotVisitor&);
    void finalizeUnconditionally(VM&);

private:
    ALWAYS_INLINE WeakMapBucketType* findBucket(JSObject* key)
    {
        return findBucket(key, jsWeakMapHash(key));
    }

    ALWAYS_INLINE WeakMapBucketType* findBucket(JSObject* key, uint32_t hash)
    {
        return findBucketAlreadyHashed(key, hash);
    }

    ALWAYS_INLINE WeakMapBucketType* buffer() const
    {
        return m_buffer->buffer();
    }

    enum class IterationState { Continue, Stop };
    template<typename Functor>
    void forEach(Functor functor)
    {
        auto* buffer = this->buffer();
        for (uint32_t index = 0; index < m_capacity; ++index) {
            auto* bucket = buffer + index;
            if (bucket->isEmpty() || bucket->isDeleted())
                continue;
            if (functor(bucket->key(), bucket->value()) == IterationState::Stop)
                return;
        }
    }

    ALWAYS_INLINE uint32_t shouldRehashAfterAdd() const
    {
        return JSC::shouldRehashAfterAdd(m_capacity, m_keyCount, m_deleteCount);
    }

    ALWAYS_INLINE uint32_t shouldShrink() const
    {
        return JSC::shouldShrink(m_capacity, m_keyCount);
    }

    ALWAYS_INLINE static bool canUseBucket(WeakMapBucketType* bucket, JSObject* key)
    {
        return !bucket->isDeleted() && key == bucket->key();
    }

    ALWAYS_INLINE void addInternal(VM& vm, JSObject* key, JSValue value, uint32_t hash)
    {
        const uint32_t mask = m_capacity - 1;
        uint32_t index = hash & mask;
        WeakMapBucketType* buffer = this->buffer();
        WeakMapBucketType* bucket = buffer + index;
        while (!bucket->isEmpty()) {
            if (canUseBucket(bucket, key)) {
                ASSERT(!bucket->isDeleted());
                bucket->setValue(vm, this, value);
                return;
            }
            index = (index + 1) & mask;
            bucket = buffer + index;
        }

        auto* newEntry = buffer + index;
        newEntry->setKey(vm, this, key);
        newEntry->setValue(vm, this, value);
        ++m_keyCount;
    }

    ALWAYS_INLINE WeakMapBucketType* findBucketAlreadyHashed(JSObject* key, uint32_t hash)
    {
        const uint32_t mask = m_capacity - 1;
        uint32_t index = hash & mask;
        WeakMapBucketType* buffer = this->buffer();
        WeakMapBucketType* bucket = buffer + index;

        while (!bucket->isEmpty()) {
            if (canUseBucket(bucket, key)) {
                ASSERT(!bucket->isDeleted());
                return buffer + index;
            }
            index = (index + 1) & mask;
            bucket = buffer + index;
        }
        return nullptr;
    }

    enum class RehashMode { Normal, RemoveBatching };
    void rehash(RehashMode mode = RehashMode::Normal)
    {
        // Since shrinking is done just after GC runs (finalizeUnconditionally), WeakMapImpl::rehash()
        // function must not touch any GC related features. This is why we do not allocate WeakMapBuffer
        // in auxiliary buffer.

        // This rehash modifies m_buffer which is not GC-managed buffer. But m_buffer can be touched in
        // visitOutputConstraints. Thus, we should guard it with cellLock.
        auto locker = holdLock(cellLock());

        uint32_t oldCapacity = m_capacity;
        MallocPtr<WeakMapBufferType, JSValueMalloc> oldBuffer = WTFMove(m_buffer);

        uint32_t capacity = m_capacity;
        if (mode == RehashMode::RemoveBatching) {
            ASSERT(shouldShrink());
            capacity = nextCapacityAfterBatchRemoval(capacity, m_keyCount);
        } else
            capacity = nextCapacity(capacity, m_keyCount);
        makeAndSetNewBuffer(locker, capacity);

        auto* buffer = this->buffer();
        const uint32_t mask = m_capacity - 1;
        for (uint32_t oldIndex = 0; oldIndex < oldCapacity; ++oldIndex) {
            auto* entry = oldBuffer->buffer() + oldIndex;
            if (entry->isEmpty() || entry->isDeleted())
                continue;

            uint32_t index = jsWeakMapHash(entry->key()) & mask;
            WeakMapBucketType* bucket = buffer + index;
            while (!bucket->isEmpty()) {
                index = (index + 1) & mask;
                bucket = buffer + index;
            }
            bucket->copyFrom(*entry);
        }

        m_deleteCount = 0;

        checkConsistency();
    }

    ALWAYS_INLINE void checkConsistency() const
    {
        if (!ASSERT_DISABLED) {
            uint32_t size = 0;
            auto* buffer = this->buffer();
            for (uint32_t index = 0; index < m_capacity; ++index) {
                auto* bucket = buffer + index;
                if (bucket->isEmpty() || bucket->isDeleted())
                    continue;
                ++size;
            }
            ASSERT(size == m_keyCount);
        }
    }

    void makeAndSetNewBuffer(const AbstractLocker&, uint32_t capacity)
    {
        ASSERT(!(capacity & (capacity - 1)));

        m_buffer = WeakMapBufferType::create(capacity);
        m_capacity = capacity;
        ASSERT(m_buffer);
        assertBufferIsEmpty();
    }

    ALWAYS_INLINE void assertBufferIsEmpty() const
    {
        if (!ASSERT_DISABLED) {
            for (unsigned i = 0; i < m_capacity; i++)
                ASSERT((buffer() + i)->isEmpty());
        }
    }

    template<typename Appender>
    void takeSnapshotInternal(unsigned limit, Appender);

    MallocPtr<WeakMapBufferType, JSValueMalloc> m_buffer;
    uint32_t m_capacity { 0 };
    uint32_t m_keyCount { 0 };
    uint32_t m_deleteCount { 0 };
};

} // namespace JSC
