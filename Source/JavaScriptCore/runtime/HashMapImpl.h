/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "JSCJSValueInlines.h"
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

    static void visitChildren(JSCell*, SlotVisitor&);

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
        return (capacity * sizeof(BucketType*)).unsafeGet();
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

ALWAYS_INLINE static bool areKeysEqual(JSGlobalObject* globalObject, JSValue a, JSValue b)
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
    if (!key.isNumber())
        return key;

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

static ALWAYS_INLINE uint32_t wangsInt64Hash(uint64_t key)
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

ALWAYS_INLINE uint32_t jsMapHash(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    ASSERT_WITH_MESSAGE(normalizeMapKey(value) == value, "We expect normalized values flowing into this function.");

    if (value.isString()) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        const String& wtfString = asString(value)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, UINT_MAX);
        return wtfString.impl()->hash();
    }

    return wangsInt64Hash(JSValue::encode(value));
}

ALWAYS_INLINE Optional<uint32_t> concurrentJSMapHash(JSValue key)
{
    key = normalizeMapKey(key);
    if (key.isString()) {
        JSString* string = asString(key);
        if (string->length() > 10 * 1024)
            return WTF::nullopt;
        const StringImpl* impl = string->tryGetValueImpl();
        if (!impl)
            return WTF::nullopt;
        return impl->concurrentHash();
    }

    uint64_t rawValue = JSValue::encode(key);
    return wangsInt64Hash(rawValue);
}

ALWAYS_INLINE uint32_t shouldShrink(uint32_t capacity, uint32_t keyCount)
{
    return 8 * keyCount <= capacity && capacity > 4;
}

ALWAYS_INLINE uint32_t shouldRehashAfterAdd(uint32_t capacity, uint32_t keyCount, uint32_t deleteCount)
{
    return 2 * (keyCount + deleteCount) >= capacity;
}

ALWAYS_INLINE uint32_t nextCapacity(uint32_t capacity, uint32_t keyCount)
{
    if (shouldShrink(capacity, keyCount)) {
        ASSERT((capacity / 2) >= 4);
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
    return (Checked<uint32_t>(capacity) * 2).unsafeGet();
}

template <typename HashMapBucketType>
class HashMapImpl : public JSNonFinalObject {
    using Base = JSNonFinalObject;
    using HashMapBufferType = HashMapBuffer<HashMapBucketType>;

public:
    using BucketType = HashMapBucketType;

    static void visitChildren(JSCell*, SlotVisitor&);

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
        uint32_t capacity = ((Checked<uint32_t>(sizeHint) * 2) + 1).unsafeGet();
        capacity = std::max<uint32_t>(WTF::roundUpToPowerOfTwo(capacity), 4U);
        m_capacity = capacity;
    }

    ALWAYS_INLINE HashMapBucketType** buffer() const
    {
        return m_buffer->buffer();
    }

    void finishCreation(JSGlobalObject* globalObject, VM& vm)
    {
        ASSERT_WITH_MESSAGE(HashMapBucket<HashMapBucketDataKey>::offsetOfKey() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey(), "We assume this to be true in the DFG and FTL JIT.");

        auto scope = DECLARE_THROW_SCOPE(vm);
        Base::finishCreation(vm);

        makeAndSetNewBuffer(globalObject, vm);
        RETURN_IF_EXCEPTION(scope, void());

        setUpHeadAndTail(globalObject, vm);
    }

    void finishCreation(JSGlobalObject* globalObject, VM& vm, HashMapImpl* base)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        Base::finishCreation(vm);

        // This size should be the same to the case when you clone the map by calling add() repeatedly.
        uint32_t capacity = ((Checked<uint32_t>(base->m_keyCount) * 2) + 1).unsafeGet();
        RELEASE_ASSERT(capacity <= (1U << 31));
        capacity = std::max<uint32_t>(WTF::roundUpToPowerOfTwo(capacity), 4U);
        m_capacity = capacity;
        makeAndSetNewBuffer(globalObject, vm);
        RETURN_IF_EXCEPTION(scope, void());

        setUpHeadAndTail(globalObject, vm);

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

    ALWAYS_INLINE HashMapBucketType** findBucket(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        key = normalizeMapKey(key);
        uint32_t hash = jsMapHash(globalObject, vm, key);
        RETURN_IF_EXCEPTION(scope, nullptr);
        return findBucket(globalObject, key, hash);
    }

    ALWAYS_INLINE HashMapBucketType** findBucket(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");
        return findBucketAlreadyHashedAndNormalized(globalObject, key, hash);
    }

    template <typename T = HashMapBucketType>
    ALWAYS_INLINE typename std::enable_if<std::is_same<T, HashMapBucket<HashMapBucketDataKeyValue>>::value, JSValue>::type get(JSGlobalObject* globalObject, JSValue key)
    {
        if (HashMapBucketType** bucket = findBucket(globalObject, key))
            return (*bucket)->value();
        return jsUndefined();
    }

    ALWAYS_INLINE bool has(JSGlobalObject* globalObject, JSValue key)
    {
        return !!findBucket(globalObject, key);
    }

    ALWAYS_INLINE void add(JSGlobalObject* globalObject, JSValue key, JSValue value = JSValue())
    {
        key = normalizeMapKey(key);
        addNormalizedInternal(globalObject, key, value, [&] (HashMapBucketType* bucket) {
            return !isDeleted(bucket) && areKeysEqual(globalObject, key, bucket->key());
        });
        if (shouldRehashAfterAdd())
            rehash(globalObject);
    }

    ALWAYS_INLINE HashMapBucketType* addNormalized(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash)
    {
        ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");
        ASSERT_WITH_MESSAGE(jsMapHash(globalObject, getVM(globalObject), key) == hash, "We expect hash value is what we expect.");

        auto* bucket = addNormalizedInternal(getVM(globalObject), key, value, hash, [&] (HashMapBucketType* bucket) {
            return !isDeleted(bucket) && areKeysEqual(globalObject, key, bucket->key());
        });
        if (shouldRehashAfterAdd())
            rehash(globalObject);
        return bucket;
    }

    ALWAYS_INLINE bool remove(JSGlobalObject* globalObject, JSValue key)
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
            rehash(globalObject);

        return true;
    }

    ALWAYS_INLINE uint32_t size() const
    {
        return m_keyCount;
    }

    ALWAYS_INLINE void clear(JSGlobalObject* globalObject)
    {
        VM& vm = getVM(globalObject);
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
        m_capacity = 4;
        makeAndSetNewBuffer(globalObject, vm);
        checkConsistency();
    }

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

    ALWAYS_INLINE void setUpHeadAndTail(JSGlobalObject*, VM& vm)
    {
        m_head.set(vm, this, HashMapBucketType::create(vm));
        m_tail.set(vm, this, HashMapBucketType::create(vm));

        m_head->setNext(vm, m_tail.get());
        m_tail->setPrev(vm, m_head.get());
        ASSERT(m_head->deleted());
        ASSERT(m_tail->deleted());
    }

    ALWAYS_INLINE void addNormalizedNonExistingForCloning(JSGlobalObject* globalObject, JSValue key, JSValue value = JSValue())
    {
        addNormalizedInternal(globalObject, key, value, [&] (HashMapBucketType*) {
            return false;
        });
    }

    template<typename CanUseBucket>
    ALWAYS_INLINE void addNormalizedInternal(JSGlobalObject* globalObject, JSValue key, JSValue value, const CanUseBucket& canUseBucket)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        uint32_t hash = jsMapHash(globalObject, vm, key);
        RETURN_IF_EXCEPTION(scope, void());
        scope.release();
        addNormalizedInternal(vm, key, value, hash, canUseBucket);
    }

    template<typename CanUseBucket>
    ALWAYS_INLINE HashMapBucketType* addNormalizedInternal(VM& vm, JSValue key, JSValue value, uint32_t hash, const CanUseBucket& canUseBucket)
    {
        ASSERT_WITH_MESSAGE(normalizeMapKey(key) == key, "We expect normalized values flowing into this function.");

        const uint32_t mask = m_capacity - 1;
        uint32_t index = hash & mask;
        HashMapBucketType** buffer = this->buffer();
        HashMapBucketType* bucket = buffer[index];
        while (!isEmpty(bucket)) {
            if (canUseBucket(bucket)) {
                bucket->setValue(vm, value);
                return bucket;
            }
            index = (index + 1) & mask;
            bucket = buffer[index];
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

    ALWAYS_INLINE HashMapBucketType** findBucketAlreadyHashedAndNormalized(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
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

    void rehash(JSGlobalObject* globalObject)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        uint32_t oldCapacity = m_capacity;
        m_capacity = nextCapacity(m_capacity, m_keyCount);

        if (m_capacity != oldCapacity) {
            makeAndSetNewBuffer(globalObject, vm);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            m_buffer->reset(m_capacity);
            assertBufferIsEmpty();
        }

        HashMapBucketType* iter = m_head->next();
        HashMapBucketType* end = m_tail.get();
        const uint32_t mask = m_capacity - 1;
        RELEASE_ASSERT(!(m_capacity & (m_capacity - 1)));
        HashMapBucketType** buffer = this->buffer();
        while (iter != end) {
            uint32_t index = jsMapHash(globalObject, vm, iter->key()) & mask;
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

    ALWAYS_INLINE void checkConsistency() const
    {
        if (ASSERT_ENABLED) {
            HashMapBucketType* iter = m_head->next();
            HashMapBucketType* end = m_tail.get();
            uint32_t size = 0;
            while (iter != end) {
                ++size;
                iter = iter->next();
            }
            ASSERT(size == m_keyCount);
        }
    }

    void makeAndSetNewBuffer(JSGlobalObject* globalObject, VM& vm)
    {
        ASSERT(!(m_capacity & (m_capacity - 1)));

        HashMapBufferType* buffer = HashMapBufferType::create(globalObject, vm, this, m_capacity);
        if (UNLIKELY(!buffer))
            return;

        m_buffer.set(vm, this, buffer);
        assertBufferIsEmpty();
    }

    ALWAYS_INLINE void assertBufferIsEmpty() const
    {
        if (ASSERT_ENABLED) {
            for (unsigned i = 0; i < m_capacity; i++)
                ASSERT(isEmpty(buffer()[i]));
        }
    }

    WriteBarrier<HashMapBucketType> m_head;
    WriteBarrier<HashMapBucketType> m_tail;
    AuxiliaryBarrier<HashMapBufferType*> m_buffer;
    uint32_t m_keyCount;
    uint32_t m_deleteCount;
    uint32_t m_capacity;
};

} // namespace JSC
