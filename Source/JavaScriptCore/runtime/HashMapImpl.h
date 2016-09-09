/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "JSObject.h"

namespace JSC {

struct HashMapBucketDataKey {
    WriteBarrier<Unknown> key;
};

struct HashMapBucketDataKeyValue {
    WriteBarrier<Unknown> key;
    WriteBarrier<Unknown> value;
};

template <typename Data>
class HashMapBucket : public JSCell {
    typedef JSCell Base;

    template <typename T = Data>
    static typename std::enable_if<std::is_same<T, HashMapBucketDataKey>::value, Structure*>::type selectStructure(VM& vm)
    {
        return vm.hashMapBucketSetStructure.get();
    }

    template <typename T = Data>
    static typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value, Structure*>::type selectStructure(VM& vm)
    {
        return vm.hashMapBucketMapStructure.get();
    }

public:
    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    static HashMapBucket* create(VM& vm)
    {
        HashMapBucket* bucket = new (NotNull, allocateCell<HashMapBucket<Data>>(vm.heap)) HashMapBucket(vm, selectStructure(vm));
        bucket->finishCreation(vm);
        ASSERT(!bucket->next());
        ASSERT(!bucket->prev());
        return bucket;
    }

    HashMapBucket(VM& vm, Structure* structure)
        : Base(vm, structure)
    { }

    ALWAYS_INLINE void setNext(VM& vm, HashMapBucket* bucket)
    {
        m_next.set(vm, this, bucket);
    }
    ALWAYS_INLINE void setPrev(VM& vm, HashMapBucket* bucket)
    {
        m_prev.set(vm, this, bucket);
    }

    ALWAYS_INLINE void setKey(VM& vm, JSValue key)
    {
        m_data.key.set(vm, this, key);
    }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value>::type setValue(VM& vm, JSValue value)
    {
        m_data.value.set(vm, this, value);
    }
    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucketDataKey>::value>::type setValue(VM&, JSValue) { }

    ALWAYS_INLINE JSValue key() const { return m_data.key.get(); }

    template <typename T = Data>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value, JSValue>::type value() const
    {
        return m_data.value.get();
    }

    static void visitChildren(JSCell*, SlotVisitor&);

    ALWAYS_INLINE HashMapBucket* next() const { return m_next.get(); }
    ALWAYS_INLINE HashMapBucket* prev() const { return m_prev.get(); }

    ALWAYS_INLINE bool deleted() const { return m_deleted; }
    ALWAYS_INLINE void setDeleted(bool deleted) { m_deleted = deleted; }

    static ptrdiff_t offsetOfKey()
    {
        return OBJECT_OFFSETOF(HashMapBucket, m_data) + OBJECT_OFFSETOF(Data, key);
    }

    template <typename T = Data>
    static typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value, ptrdiff_t>::type offsetOfValue()
    {
        return OBJECT_OFFSETOF(HashMapBucket, m_data) + OBJECT_OFFSETOF(Data, value);
    }

private:
    Data m_data;
    WriteBarrier<HashMapBucket> m_next;
    WriteBarrier<HashMapBucket> m_prev;
    bool m_deleted { false };
};

template <typename BucketType>
class HashMapBuffer {
public:
    HashMapBuffer() = delete;

    static size_t allocationSize(uint32_t capacity)
    {
        return capacity * sizeof(BucketType*);
    }

    ALWAYS_INLINE BucketType** buffer() const
    {
        return bitwise_cast<BucketType**>(this);
    }

    static HashMapBuffer* create(ExecState* exec, VM& vm, JSCell* owner, uint32_t capacity)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        size_t allocationSize = HashMapBuffer::allocationSize(capacity);
        void* data = nullptr;
        DeferGCForAWhile defer(vm.heap);
        if (!vm.heap.tryAllocateStorage(owner, allocationSize, &data)) {
            throwOutOfMemoryError(exec, scope);
            return nullptr;
        }

        HashMapBuffer* buffer = static_cast<HashMapBuffer*>(data);
        memset(buffer, -1, allocationSize);
        return buffer;
    }
};

ALWAYS_INLINE static bool areKeysEqual(ExecState* exec, JSValue a, JSValue b)
{
    // We want +0 and -0 to be compared to true here. sameValue() itself doesn't
    // guarantee that, however, we normalize all keys before comparing and storing
    // them in the map. The normalization will convert -0.0 and 0.0 to the integer
    // representation for 0.
    return sameValue(exec, a, b);
}

ALWAYS_INLINE JSValue normalizeMapKey(JSValue key)
{
    if (!key.isNumber())
        return key;

    if (key.isInt32())
        return key;

    double d = key.asDouble();
    if (std::isnan(d))
        return key;

    int i = static_cast<int>(d);
    if (i == d) {
        // When a key is -0, we convert it to positive zero.
        // When a key is the double representation for an integer, we convert it to an integer.
        return jsNumber(i);
    }
    // This means key is definitely not negative zero, and it's definitely not a double representation of an integer. 
    return key;
}

ALWAYS_INLINE uint32_t jsMapHash(ExecState* exec, VM& vm, JSValue value)
{
    ASSERT_WITH_MESSAGE(normalizeMapKey(value) == value, "We expect normalized values flowing into this function.");

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isString()) {
        JSString* string = asString(value);
        const String& wtfString = string->value(exec);
        if (UNLIKELY(scope.exception()))
            return UINT_MAX;
        return wtfString.impl()->hash();
    }

    auto wangsInt64Hash = [] (uint64_t key) -> uint32_t {
        key += ~(key << 32);
        key ^= (key >> 22);
        key += ~(key << 13);
        key ^= (key >> 8);
        key += (key << 3);
        key ^= (key >> 15);
        key += ~(key << 27);
        key ^= (key >> 31);
        return static_cast<unsigned>(key);
    };
    uint64_t rawValue = JSValue::encode(value);
    return wangsInt64Hash(rawValue);
}

template <typename HashMapBucketType>
class HashMapImpl : public JSCell {
    typedef JSCell Base;
    typedef HashMapBuffer<HashMapBucketType> HashMapBufferType;

    template <typename T = HashMapBucketType>
    static typename std::enable_if<std::is_same<T, HashMapBucket<HashMapBucketDataKey>>::value, Structure*>::type selectStructure(VM& vm)
    {
        return vm.hashMapImplSetStructure.get();
    }

    template <typename T = HashMapBucketType>
    static typename std::enable_if<std::is_same<T, HashMapBucket<HashMapBucketDataKeyValue>>::value, Structure*>::type selectStructure(VM& vm)
    {
        return vm.hashMapImplMapStructure.get();
    }

public:
    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    static HashMapImpl* create(ExecState* exec, VM& vm)
    {
        ASSERT_WITH_MESSAGE(HashMapBucket<HashMapBucketDataKey>::offsetOfKey() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey(), "We assume this to be true in the DFG and FTL JIT.");

        HashMapImpl* impl = new (NotNull, allocateCell<HashMapImpl>(vm.heap)) HashMapImpl(vm, selectStructure(vm));
        impl->finishCreation(exec, vm);
        return impl;
    }

    static void visitChildren(JSCell*, SlotVisitor&);
    static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    HashMapImpl(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_size(0)
        , m_deleteCount(0)
        , m_capacity(4)
    {
    }

    ALWAYS_INLINE HashMapBucketType** buffer() const
    {
        return m_buffer.get()->buffer();
    }

    void finishCreation(ExecState* exec, VM& vm)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        Base::finishCreation(vm);

        makeAndSetNewBuffer(exec, vm);
        if (UNLIKELY(scope.exception()))
            return;

        m_head.set(vm, this, HashMapBucketType::create(vm));
        m_tail.set(vm, this, HashMapBucketType::create(vm));

        m_head->setNext(vm, m_tail.get());
        m_tail->setPrev(vm, m_head.get());
        m_head->setDeleted(true);
        m_tail->setDeleted(true);

    }

    static HashMapBucketType* emptyValue()
    {
        return bitwise_cast<HashMapBucketType*>(static_cast<uintptr_t>(-1));
    }

    static ALWAYS_INLINE bool isEmpty(HashMapBucketType* bucket)
    {
        return bucket == emptyValue();
    }

    static HashMapBucketType* deletedValue()
    {
        return bitwise_cast<HashMapBucketType*>(static_cast<uintptr_t>(-3));
    }

    static ALWAYS_INLINE bool isDeleted(HashMapBucketType* bucket)
    {
        return bucket == deletedValue();
    }

    ALWAYS_INLINE HashMapBucketType** findBucket(ExecState* exec, JSValue key)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        key = normalizeMapKey(key);
        uint32_t hash = jsMapHash(exec, vm, key);
        if (UNLIKELY(scope.exception()))
            return nullptr;
        return findBucket(exec, key, hash);
    }

    ALWAYS_INLINE HashMapBucketType** findBucket(ExecState* exec, JSValue key, uint32_t hash)
    {
        ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");
        return findBucketAlreadyHashedAndNormalized(exec, key, hash);
    }

    ALWAYS_INLINE JSValue get(ExecState* exec, JSValue key)
    {
        if (HashMapBucketType** bucket = findBucket(exec, key))
            return (*bucket)->value();
        return jsUndefined();
    }

    ALWAYS_INLINE bool has(ExecState* exec, JSValue key)
    {
        return !!findBucket(exec, key);
    }

    ALWAYS_INLINE void add(ExecState* exec, JSValue key, JSValue value = JSValue())
    {
        key = normalizeMapKey(key);

        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        const uint32_t mask = m_capacity - 1;
        uint32_t index = jsMapHash(exec, vm, key) & mask;
        if (UNLIKELY(scope.exception()))
            return;
        HashMapBucketType** buffer = this->buffer();
        HashMapBucketType* bucket = buffer[index];
        while (!isEmpty(bucket)) {
            if (!isDeleted(bucket) && areKeysEqual(exec, key, bucket->key())) {
                bucket->setValue(vm, value);
                return;
            }
            index = (index + 1) & mask;
            bucket = buffer[index];
        }

        HashMapBucketType* newEntry = m_tail.get();
        buffer[index] = newEntry;
        newEntry->setKey(vm, key);
        newEntry->setValue(vm, value);
        newEntry->setDeleted(false);
        HashMapBucketType* newTail = HashMapBucketType::create(vm);
        m_tail.set(vm, this, newTail);
        newTail->setPrev(vm, newEntry);
        newTail->setDeleted(true);
        newEntry->setNext(vm, newTail);
        uint32_t newSize = ++m_size;
        if (2*newSize > m_capacity)
            rehash(exec);
    }

    ALWAYS_INLINE bool remove(ExecState* exec, JSValue key)
    {
        HashMapBucketType** bucket = findBucket(exec, key);
        if (!bucket)
            return false;

        VM& vm = exec->vm();
        HashMapBucketType* impl = *bucket;
        impl->next()->setPrev(vm, impl->prev());
        impl->prev()->setNext(vm, impl->next());
        impl->setDeleted(true);

        *bucket = deletedValue();

        m_deleteCount++;

        return true;
    }

    ALWAYS_INLINE uint32_t size() const
    {
        RELEASE_ASSERT(m_size >= m_deleteCount);
        return m_size - m_deleteCount;
    }

    ALWAYS_INLINE void clear(ExecState* exec)
    {
        VM& vm = exec->vm();
        m_size = 0;
        m_deleteCount = 0;
        HashMapBucketType* head = m_head.get();
        HashMapBucketType* bucket = m_head->next();
        HashMapBucketType* tail = m_tail.get();
        while (bucket != tail) {
            HashMapBucketType* next = bucket->next();
            // We restart each iterator by pointing it to the head of the list.
            bucket->setNext(vm, head);
            bucket = next;
        }
        m_head->setNext(vm, m_tail.get());
        m_tail->setPrev(vm, m_head.get());
        m_capacity = 4;
        makeAndSetNewBuffer(exec, vm);
    }

    ALWAYS_INLINE size_t bufferSizeInBytes() const
    {
        return m_capacity * sizeof(HashMapBucketType*);
    }

    static ptrdiff_t offsetOfBuffer()
    {
        return OBJECT_OFFSETOF(HashMapImpl<HashMapBucketType>, m_buffer);
    }

    static ptrdiff_t offsetOfCapacity()
    {
        return OBJECT_OFFSETOF(HashMapImpl<HashMapBucketType>, m_capacity);
    }

    HashMapBucketType* head() { return m_head.get(); }
    HashMapBucketType* tail() { return m_tail.get(); }

    size_t approximateSize() const
    {
        size_t size = sizeof(HashMapImpl);
        size += bufferSizeInBytes();
        size += 2 * sizeof(HashMapBucketType); // Head and tail members.
        size += (m_deleteCount + m_size) * sizeof(HashMapBucketType); // Number of members on the list.
        return size;
    }

private:
    ALWAYS_INLINE HashMapBucketType** findBucketAlreadyHashedAndNormalized(ExecState* exec, JSValue key, uint32_t hash)
    {
        const uint32_t mask = m_capacity - 1;
        uint32_t index = hash & mask;
        HashMapBucketType** buffer = this->buffer();
        HashMapBucketType* bucket = buffer[index];

        while (!isEmpty(bucket)) {
            if (!isDeleted(bucket) && areKeysEqual(exec, key, bucket->key()))
                return buffer + index;
            index = (index + 1) & mask;
            bucket = buffer[index];
        }
        return nullptr;
    }

    void rehash(ExecState* exec)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        m_capacity = m_capacity * 2;
        makeAndSetNewBuffer(exec, vm);
        if (UNLIKELY(scope.exception()))
            return;

        HashMapBucketType* iter = m_head->next();
        HashMapBucketType* end = m_tail.get();
        const uint32_t mask = m_capacity - 1;
        RELEASE_ASSERT(!(m_capacity & (m_capacity - 1)));
        HashMapBucketType** buffer = this->buffer();
        while (iter != end) {
            uint32_t index = jsMapHash(exec, vm, iter->key()) & mask;
            ASSERT_WITH_MESSAGE(!scope.exception(), "All keys should already be hashed before, so this should not throw because it won't resolve ropes.");
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
    }

    void makeAndSetNewBuffer(ExecState* exec, VM& vm)
    {
        ASSERT(!(m_capacity & (m_capacity - 1)));

        HashMapBufferType* buffer = HashMapBufferType::create(exec, vm, this, m_capacity);
        if (UNLIKELY(!buffer))
            return;

        m_buffer.set(vm, this, buffer);
        if (!ASSERT_DISABLED) {
            for (unsigned i = 0; i < m_capacity; i++)
                ASSERT(isEmpty(this->buffer()[i]));
        }
    }

    WriteBarrier<HashMapBucketType> m_head;
    WriteBarrier<HashMapBucketType> m_tail;
    CopyBarrier<HashMapBufferType> m_buffer;
    uint32_t m_size;
    uint32_t m_deleteCount;
    uint32_t m_capacity;
};

} // namespace JSC
