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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Algorithms.h>
#include <wtf/HashTable.h>
#include <wtf/WeakPtr.h>

namespace WTF {

// Value will be deleted lazily upon rehash or amortized over time. For manual cleanup, call removeNullReferences().
template<typename KeyType, typename ValueType, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakHashMap final {
    WTF_MAKE_FAST_ALLOCATED;
public:

    using RefType = Ref<WeakPtrImpl>;
    using KeyTraits = HashTraits<KeyType>;
    using ValueTraits = HashTraits<ValueType>;
    using WeakHashImplMap = HashMap<RefType, ValueType>;

    struct PeekKeyValuePairTraits : KeyValuePairHashTraits<HashTraits<KeyType&>, HashTraits<ValueType&>> {
        static constexpr bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const typename KeyValuePairHashTraits<HashTraits<KeyType&>, HashTraits<ValueType&>>::TraitType& value)
        {
            return isHashTraitsEmptyValue<KeyTraits>(value.key);
        }
    };
    using PeekKeyValuePairType = typename PeekKeyValuePairTraits::TraitType;

    struct PeekType {
        KeyType& key;
        ValueType& value;
    };

    struct PeekPtrType {
        PeekPtrType(const PeekType& peek)
            : m_peek(peek)
        { }

        const PeekType& operator*() const { return m_peek; }
        PeekType& operator*() { return m_peek; }

        const PeekType* operator->() const { return &m_peek; }
        PeekType* operator->() { return &m_peek; }

    private:
        PeekType m_peek;
    };

    template <typename MapType, typename IteratorType, typename IteratorPeekPtrType, typename IteratorPeekType>
    class WeakHashMapIteratorBase {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = IteratorPeekType;
        using difference_type = ptrdiff_t;
        using pointer = IteratorPeekPtrType;
        using reference = IteratorPeekType;

    protected:
        WeakHashMapIteratorBase(MapType& weakHashMap, IteratorType position)
            : m_weakHashMap { weakHashMap }
            , m_position { position }
            , m_endPosition { weakHashMap.m_map.end() }
        {
            skipEmptyBuckets();
        }
    
        ~WeakHashMapIteratorBase() = default;

        ALWAYS_INLINE IteratorPeekType makePeek()
        {
            auto* entry = m_position.get();
            auto* key = static_cast<KeyType*>(entry->key->template get<KeyType>());
            return IteratorPeekType { *key, entry->value };
        }

        ALWAYS_INLINE IteratorPeekType makePeek() const
        {
            auto* entry = m_position.get();
            auto* key = static_cast<KeyType*>(entry->key->template get<KeyType>());
            return IteratorPeekType { *key, const_cast<ValueType&>(entry->value) };
        }

        void advance()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            ++m_advanceCount;
            skipEmptyBuckets();
            m_weakHashMap.increaseOperationCountSinceLastCleanup();
        }

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && !m_position->key.get())
                ++m_position;
        }

        const MapType& m_weakHashMap;
        IteratorType m_position;
        IteratorType m_endPosition;
        unsigned m_advanceCount { 0 };
    };

    class WeakHashMapIterator : public WeakHashMapIteratorBase<WeakHashMap, typename WeakHashImplMap::iterator, PeekPtrType, PeekType> {
    public:
        using Base = WeakHashMapIteratorBase<WeakHashMap, typename WeakHashImplMap::iterator, PeekPtrType, PeekType>;

        bool operator==(const WeakHashMapIterator& other) const { return Base::m_position == other.Base::m_position; }
        bool operator!=(const WeakHashMapIterator& other) const { return Base::m_position != other.Base::m_position; }

        PeekPtrType get() { return Base::makePeek(); }
        PeekType operator*() { return Base::makePeek(); }
        PeekPtrType operator->() { return Base::makePeek(); }

        WeakHashMapIterator& operator++()
        {
            Base::advance();
            return *this;
        }

    private:
        WeakHashMapIterator(WeakHashMap& map, typename WeakHashImplMap::iterator position)
            : Base { map, position }
        { }

        template <typename, typename, typename> friend class WeakHashMap;
    };

    class WeakHashMapConstIterator : public WeakHashMapIteratorBase<const WeakHashMap, typename WeakHashImplMap::const_iterator, const PeekPtrType, const PeekType> {
    public:
        using Base = WeakHashMapIteratorBase<const WeakHashMap, typename WeakHashImplMap::const_iterator, const PeekPtrType, const PeekType>;

        bool operator==(const WeakHashMapConstIterator& other) const { return Base::m_position == other.Base::m_position; }
        bool operator!=(const WeakHashMapConstIterator& other) const { return Base::m_position != other.Base::m_position; }

        const PeekPtrType get() const { return Base::makePeek(); }
        const PeekType operator*() const { return Base::makePeek(); }
        const PeekPtrType operator->() const { return Base::makePeek(); }

        WeakHashMapConstIterator& operator++()
        {
            Base::advance();
            return *this;
        }

    private:
        WeakHashMapConstIterator(const WeakHashMap& map, typename WeakHashImplMap::const_iterator position)
            : Base { map, position }
        { }

        template <typename, typename, typename> friend class WeakHashMap;
    };

    struct AddResult {
        AddResult() : isNewEntry(false) { }
        AddResult(WeakHashMapIterator&& it, bool isNewEntry)
            : iterator(WTFMove(it))
            , isNewEntry(isNewEntry)
        { }
        WeakHashMapIterator iterator;
        bool isNewEntry;

        explicit operator bool() const { return isNewEntry; }
    };

    using iterator = WeakHashMapIterator;
    using const_iterator = WeakHashMapConstIterator;

    iterator begin() { return WeakHashMapIterator(*this, m_map.begin()); }
    iterator end() { return WeakHashMapIterator(*this, m_map.end()); }
    const_iterator begin() const { return WeakHashMapConstIterator(*this, m_map.begin()); }
    const_iterator end() const { return WeakHashMapConstIterator(*this, m_map.end()); }

    template <typename Functor>
    AddResult ensure(const KeyType& key, Functor&& functor)
    {
        amortizedCleanupIfNeeded();
        auto result = m_map.ensure(makeKeyImpl(key), functor);
        return AddResult { WeakHashMapIterator(*this, result.iterator), result.isNewEntry };
    }

    template<typename T>
    AddResult add(const KeyType& key, T&& value)
    {
        amortizedCleanupIfNeeded();
        auto addResult = m_map.add(makeKeyImpl(key), std::forward<T>(value));
        return AddResult { WeakHashMapIterator(*this, addResult.iterator), addResult.isNewEntry };
    }

    template<typename T, typename V>
    void set(const T& key, V&& value)
    {
        amortizedCleanupIfNeeded();
        m_map.set(makeKeyImpl(key), std::forward<V>(value));
    }

    iterator find(const KeyType& key)
    {
        increaseOperationCountSinceLastCleanup();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return end();
        return WeakHashMapIterator(*this, m_map.find(*keyImpl));
    }

    const_iterator find(const KeyType& key) const
    {
        increaseOperationCountSinceLastCleanup();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return end();
        return WeakHashMapConstIterator(*this, m_map.find(*keyImpl));
    }

    bool contains(const KeyType& key) const
    {
        increaseOperationCountSinceLastCleanup();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return false;
        return m_map.contains(*keyImpl);
    }

    typename ValueTraits::TakeType take(const KeyType& key)
    {
        amortizedCleanupIfNeeded();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return ValueTraits::take(ValueTraits::emptyValue());
        return m_map.take(*keyImpl);
    }

    typename ValueTraits::PeekType get(const KeyType& key)
    {
        increaseOperationCountSinceLastCleanup();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return ValueTraits::peek(ValueTraits::emptyValue());
        return m_map.get(*keyImpl);
    }

    bool remove(iterator it)
    {
        auto didRemove = m_map.remove(it.m_position);
        amortizedCleanupIfNeeded();
        return didRemove;
    }

    bool remove(const KeyType& key)
    {
        amortizedCleanupIfNeeded();
        auto* keyImpl = keyImplIfExists(key);
        if (!keyImpl)
            return false;
        return m_map.remove(keyImpl);
    }

    template<typename Functor>
    bool removeIf(Functor&& functor)
    {
        m_operationCountSinceLastCleanup = 0;
        return m_map.removeIf([&](auto& entry) {
            auto* key = static_cast<KeyType*>(entry.key->template get<KeyType>());
            bool isReleasedWeakKey = !key;
            if (isReleasedWeakKey)
                return true;
            PeekType peek { *key, entry.value };
            return functor(peek);
        });
    }

    void clear()
    {
        m_operationCountSinceLastCleanup = 0;
        m_map.clear();
    }

    unsigned capacity() const { return m_map.capacity(); }

    bool isEmptyIgnoringNullReferences() const
    {
        if (m_map.isEmpty())
            return true;

        auto onlyContainsNullReferences = begin() == end();
        if (UNLIKELY(onlyContainsNullReferences))
            const_cast<WeakHashMap&>(*this).clear();
        return onlyContainsNullReferences;
    }

    bool hasNullReferences() const
    {
        unsigned count = 0;
        auto result = WTF::anyOf(m_map, [&] (auto& iterator) {
            ++count;
            return !iterator.key.get();
        });
        if (result)
            amortizedCleanupIfNeeded(count);
        else
            m_operationCountSinceLastCleanup = 0;
        return result;
    }

    unsigned computeSize() const
    {
        const_cast<WeakHashMap&>(*this).removeNullReferences();
        return m_map.size();
    }

    NEVER_INLINE bool removeNullReferences()
    {
        m_operationCountSinceLastCleanup = 0;
        return m_map.removeIf([](auto& iterator) { return !iterator.key.get(); });
    }

#if ASSERT_ENABLED
    void checkConsistency() const { m_map.checkConsistency(); }
#else
    void checkConsistency() const { }
#endif

private:
    ALWAYS_INLINE unsigned increaseOperationCountSinceLastCleanup(unsigned operationsPerformed = 1) const
    {
        unsigned currentCount = m_operationCountSinceLastCleanup;
        m_operationCountSinceLastCleanup += operationsPerformed;
        return currentCount;
    }

    ALWAYS_INLINE void amortizedCleanupIfNeeded(unsigned operationsPerformed = 1) const
    {
        unsigned currentCount = increaseOperationCountSinceLastCleanup(operationsPerformed);
        if (currentCount / 2 > m_map.size())
            const_cast<WeakHashMap&>(*this).removeNullReferences();
    }

    template <typename T>
    static RefType makeKeyImpl(const T& key)
    {
        return *key.weakPtrFactory().template createWeakPtr<T>(const_cast<T&>(key)).m_impl;
    }

    template <typename T>
    static WeakPtrImpl* keyImplIfExists(const T& key)
    {
        auto& weakPtrImpl = key.weakPtrFactory().m_impl;
        if (auto* pointer = weakPtrImpl.pointer(); pointer && *pointer)
            return pointer;
        return nullptr;
    }

    WeakHashImplMap m_map;
    mutable unsigned m_operationCountSinceLastCleanup { 0 }; // FIXME: Store this as a HashTable meta data.

    template <typename, typename, typename, typename> friend class WeakHashMapIteratorBase;
};

} // namespace WTF

using WTF::WeakHashMap;
