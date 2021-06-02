/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "JSCJSValue.h"
#include "JSObject.h"

namespace JSC {

JS_EXPORT_PRIVATE const ClassInfo* getHashMapBucketKeyClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getHashMapBucketKeyValueClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getHashMapImplKeyClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getHashMapImplKeyValueClassInfo();

enum class HashTableType {
    Key,
    KeyValue
};

struct HashMapBucketDataKey {
    static const HashTableType Type = HashTableType::Key;
    WriteBarrier<Unknown> key;
};

struct HashMapBucketDataKeyValue {
    static const HashTableType Type = HashTableType::KeyValue;
    WriteBarrier<Unknown> key;
    WriteBarrier<Unknown> value;
};

template <typename Data>
class HashMapBucket final : public JSCell {
    using Base = JSCell;

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
    static const HashTableType Type = Data::Type;
    static const ClassInfo s_info; // This is never accessed directly, since that would break linkage on some compilers.

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        if constexpr (Type == HashTableType::Key)
            return vm.setBucketSpace<mode>();
        else
            return vm.mapBucketSpace<mode>();
    }

    static const ClassInfo* info()
    {
        if constexpr (Type == HashTableType::Key)
            return getHashMapBucketKeyClassInfo();
        else
            return getHashMapBucketKeyValueClassInfo();
    }

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

    static HashMapBucket* createSentinel(VM& vm)
    {
        auto* bucket = create(vm);
        bucket->setKey(vm, jsUndefined());
        bucket->setValue(vm, jsUndefined());
        ASSERT(!bucket->deleted());
        return bucket;
    }

    HashMapBucket(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        ASSERT(deleted());
    }

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

    DECLARE_VISIT_CHILDREN;

    ALWAYS_INLINE HashMapBucket* next() const { return m_next.get(); }
    ALWAYS_INLINE HashMapBucket* prev() const { return m_prev.get(); }

    ALWAYS_INLINE bool deleted() const { return !key(); }
    ALWAYS_INLINE void makeDeleted(VM& vm)
    {
        setKey(vm, JSValue());
        setValue(vm, JSValue());
    }

    static ptrdiff_t offsetOfKey()
    {
        return OBJECT_OFFSETOF(HashMapBucket, m_data) + OBJECT_OFFSETOF(Data, key);
    }

    template <typename T = Data>
    static typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value, ptrdiff_t>::type offsetOfValue()
    {
        return OBJECT_OFFSETOF(HashMapBucket, m_data) + OBJECT_OFFSETOF(Data, value);
    }

    static ptrdiff_t offsetOfNext()
    {
        return OBJECT_OFFSETOF(HashMapBucket, m_next);
    }

    template <typename T = Data>
    ALWAYS_INLINE static typename std::enable_if<std::is_same<T, HashMapBucketDataKeyValue>::value, JSValue>::type extractValue(const HashMapBucket& bucket)
    {
        return bucket.value();
    }

    template <typename T = Data>
    ALWAYS_INLINE static typename std::enable_if<std::is_same<T, HashMapBucketDataKey>::value, JSValue>::type extractValue(const HashMapBucket&)
    {
        return JSValue();
    }

private:
    WriteBarrier<HashMapBucket> m_next;
    WriteBarrier<HashMapBucket> m_prev;
    Data m_data;
};

template <typename BucketType>
class HashMapBuffer {
public:
    HashMapBuffer() = delete;

    static size_t allocationSize(Checked<size_t> capacity)
    {
        return capacity * sizeof(BucketType*);
    }

    ALWAYS_INLINE BucketType** buffer() const
    {
        return bitwise_cast<BucketType**>(this);
    }

    static HashMapBuffer* create(JSGlobalObject* globalObject, VM& vm, JSCell*, uint32_t capacity)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        size_t allocationSize = HashMapBuffer::allocationSize(capacity);
        void* data = vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(vm, allocationSize, nullptr, AllocationFailureMode::ReturnNull);
        if (!data) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        HashMapBuffer* buffer = static_cast<HashMapBuffer*>(data);
        buffer->reset(capacity);
        return buffer;
    }

    ALWAYS_INLINE void reset(uint32_t capacity)
    {
        memset(this, -1, allocationSize(capacity));
    }
};

ALWAYS_INLINE bool areKeysEqual(JSGlobalObject*, JSValue, JSValue);

// Note that normalization is inlined in DFG's NormalizeMapKey.
// Keep in sync with the implementation of DFG and FTL normalization.
ALWAYS_INLINE JSValue normalizeMapKey(JSValue key);
ALWAYS_INLINE uint32_t wangsInt64Hash(uint64_t key);
ALWAYS_INLINE uint32_t jsMapHash(JSBigInt*);
ALWAYS_INLINE uint32_t jsMapHash(JSGlobalObject*, VM&, JSValue);
ALWAYS_INLINE uint32_t shouldShrink(uint32_t capacity, uint32_t keyCount);
ALWAYS_INLINE uint32_t shouldRehashAfterAdd(uint32_t capacity, uint32_t keyCount, uint32_t deleteCount);
ALWAYS_INLINE uint32_t nextCapacity(uint32_t capacity, uint32_t keyCount);

template <typename HashMapBucketType>
class HashMapImpl : public JSNonFinalObject {
    using Base = JSNonFinalObject;
    using HashMapBufferType = HashMapBuffer<HashMapBucketType>;

public:
    using BucketType = HashMapBucketType;

    DECLARE_VISIT_CHILDREN;

    static size_t estimatedSize(JSCell*, VM&);

    HashMapImpl(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_keyCount(0)
        , m_deleteCount(0)
        , m_capacity(4)
    {
    }

    HashMapImpl(VM& vm, Structure* structure, uint32_t sizeHint)
        : Base(vm, structure)
        , m_keyCount(0)
        , m_deleteCount(0)
    {
        uint32_t capacity = (Checked<uint32_t>(sizeHint) * 2) + 1;
        capacity = std::max<uint32_t>(WTF::roundUpToPowerOfTwo(capacity), 4U);
        m_capacity = capacity;
    }

    ALWAYS_INLINE HashMapBucketType** buffer() const
    {
        return m_buffer->buffer();
    }

    void finishCreation(JSGlobalObject*, VM&);
    void finishCreation(JSGlobalObject*, VM&, HashMapImpl* base);

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

    ALWAYS_INLINE HashMapBucketType** findBucket(JSGlobalObject*, JSValue key);

    ALWAYS_INLINE HashMapBucketType** findBucket(JSGlobalObject*, JSValue key, uint32_t hash);

    template <typename T = HashMapBucketType>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucket<HashMapBucketDataKeyValue>>::value, JSValue>::type get(JSGlobalObject*, JSValue key);

    ALWAYS_INLINE bool has(JSGlobalObject*, JSValue key);

    ALWAYS_INLINE void add(JSGlobalObject*, JSValue key, JSValue = JSValue());

    ALWAYS_INLINE HashMapBucketType* addNormalized(JSGlobalObject*, JSValue key, JSValue, uint32_t hash);

    ALWAYS_INLINE bool remove(JSGlobalObject*, JSValue key);

    ALWAYS_INLINE uint32_t size() const
    {
        return m_keyCount;
    }

    ALWAYS_INLINE void clear(JSGlobalObject*);

    ALWAYS_INLINE size_t bufferSizeInBytes() const
    {
        return m_capacity * sizeof(HashMapBucketType*);
    }

    static ptrdiff_t offsetOfHead()
    {
        return OBJECT_OFFSETOF(HashMapImpl<HashMapBucketType>, m_head);
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
        size += m_keyCount * sizeof(HashMapBucketType); // Number of members that are on the list.
        return size;
    }

private:
    ALWAYS_INLINE uint32_t shouldRehashAfterAdd() const
    {
        return JSC::shouldRehashAfterAdd(m_capacity, m_keyCount, m_deleteCount);
    }

    ALWAYS_INLINE uint32_t shouldShrink() const
    {
        return JSC::shouldShrink(m_capacity, m_keyCount);
    }

    ALWAYS_INLINE void setUpHeadAndTail(JSGlobalObject*, VM&);

    ALWAYS_INLINE void addNormalizedNonExistingForCloning(JSGlobalObject*, JSValue key, JSValue = JSValue());

    template<typename CanUseBucket>
    ALWAYS_INLINE void addNormalizedInternal(JSGlobalObject*, JSValue key, JSValue, const CanUseBucket&);

    template<typename CanUseBucket>
    ALWAYS_INLINE HashMapBucketType* addNormalizedInternal(VM&, JSValue key, JSValue, uint32_t hash, const CanUseBucket&);

    ALWAYS_INLINE HashMapBucketType** findBucketAlreadyHashedAndNormalized(JSGlobalObject*, JSValue key, uint32_t hash);

    void rehash(JSGlobalObject*);

    ALWAYS_INLINE void checkConsistency() const;

    void makeAndSetNewBuffer(JSGlobalObject*, VM&);

    ALWAYS_INLINE void assertBufferIsEmpty() const;

    WriteBarrier<HashMapBucketType> m_head;
    WriteBarrier<HashMapBucketType> m_tail;
    AuxiliaryBarrier<HashMapBufferType*> m_buffer;
    uint32_t m_keyCount;
    uint32_t m_deleteCount;
    uint32_t m_capacity;
};

} // namespace JSC
