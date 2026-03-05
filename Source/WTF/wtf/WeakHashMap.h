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

#include <algorithm>
#include <wtf/HashTable.h>
#include <wtf/WeakPtr.h>

namespace WTF {

// Value will be deleted lazily upon rehash or amortized over time. For manual cleanup, call removeNullReferences().
template<typename KeyType, typename ValueType, typename WeakPtrImplType>
class WeakHashMap final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WeakHashMap);
public:
    using WeakKeyType = WeakPtr<KeyType, WeakPtrImplType>;
    using ValueTraits = HashTraits<ValueType>;
    using MapType = HashMap<WeakKeyType, ValueType>;

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

    template<typename MapType, typename IteratorType, typename IteratorPeekPtrType, typename IteratorPeekType>
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
        }
    
        ~WeakHashMapIteratorBase() = default;

        ALWAYS_INLINE IteratorPeekType makePeek()
        {
            auto* entry = m_position.get();
            return IteratorPeekType { *entry->key.get(), entry->value };
        }

        ALWAYS_INLINE IteratorPeekType makePeek() const
        {
            auto* entry = m_position.get();
            return IteratorPeekType { *entry->key.get(), const_cast<ValueType&>(entry->value) };
        }

        void advance()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            ++m_advanceCount;
            m_weakHashMap.increaseOperationCountSinceLastCleanup();
        }

        const MapType& m_weakHashMap;
        IteratorType m_position;
        IteratorType m_endPosition;
        unsigned m_advanceCount { 0 };
    };

    class WeakHashMapIterator : public WeakHashMapIteratorBase<WeakHashMap, typename MapType::iterator, PeekPtrType, PeekType> {
    public:
        using Base = WeakHashMapIteratorBase<WeakHashMap, typename MapType::iterator, PeekPtrType, PeekType>;

        bool operator==(const WeakHashMapIterator& other) const { return Base::m_position == other.Base::m_position; }

        PeekPtrType get() { return Base::makePeek(); }
        PeekType operator*() { return Base::makePeek(); }
        PeekPtrType operator->() { return Base::makePeek(); }

        WeakHashMapIterator& operator++()
        {
            Base::advance();
            return *this;
        }

    private:
        WeakHashMapIterator(WeakHashMap& map, typename MapType::iterator position)
            : Base { map, position }
        { }

        template<typename, typename, typename> friend class WeakHashMap;
    };

    class WeakHashMapConstIterator : public WeakHashMapIteratorBase<const WeakHashMap, typename MapType::const_iterator, const PeekPtrType, const PeekType> {
    public:
        using Base = WeakHashMapIteratorBase<const WeakHashMap, typename MapType::const_iterator, const PeekPtrType, const PeekType>;

        bool operator==(const WeakHashMapConstIterator& other) const { return Base::m_position == other.Base::m_position; }

        const PeekPtrType get() const { return Base::makePeek(); }
        const PeekType operator*() const { return Base::makePeek(); }
        const PeekPtrType operator->() const { return Base::makePeek(); }

        WeakHashMapConstIterator& operator++()
        {
            Base::advance();
            return *this;
        }

    private:
        WeakHashMapConstIterator(const WeakHashMap& map, typename MapType::const_iterator position)
            : Base { map, position }
        { }

        template<typename, typename, typename> friend class WeakHashMap;
    };

    struct AddResult {
        AddResult() : isNewEntry(false) { }
        AddResult(WeakHashMapIterator&& it, bool isNewEntry)
            : iterator(WTF::move(it))
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

    template<typename Functor>
    AddResult ensure(const KeyType* key, NOESCAPE Functor&& functor)
    {
        amortizedCleanupIfNeeded();
        auto result = m_map.ensure(key, std::forward<Functor>(functor));
        return AddResult { WeakHashMapIterator(*this, result.iterator), result.isNewEntry };
    }

    template<typename Functor>
    AddResult ensure(const KeyType& key, NOESCAPE Functor&& functor)
    {
        return ensure(&key, std::forward<Functor>(functor));
    }

    template<typename T>
    AddResult add(const KeyType* key, T&& value)
    {
        amortizedCleanupIfNeeded();
        auto addResult = m_map.add(key, std::forward<T>(value));
        return AddResult { WeakHashMapIterator(*this, addResult.iterator), addResult.isNewEntry };
    }

    template<typename T>
    AddResult add(const KeyType& key, T&& value)
    {
        return add(&key, std::forward<T>(value));
    }

    template<typename V>
    AddResult set(const KeyType* key, V&& value)
    {
        amortizedCleanupIfNeeded();
        auto addResult = m_map.set(key, std::forward<V>(value));
        return AddResult { WeakHashMapIterator(*this, addResult.iterator), addResult.isNewEntry };
    }

    template<typename V>
    AddResult set(const KeyType& key, V&& value)
    {
        return set(&key, std::forward<V>(value));
    }

    iterator find(const KeyType* key)
    {
        increaseOperationCountSinceLastCleanup();
        return WeakHashMapIterator(*this, m_map.find(key));
    }

    iterator find(const KeyType& key)
    {
        return find(&key);
    }

    const_iterator find(const KeyType* key) const
    {
        increaseOperationCountSinceLastCleanup();
        return WeakHashMapConstIterator(*this, m_map.find(key));
    }

    const_iterator find(const KeyType& key) const
    {
        return find(&key);
    }

    bool contains(const KeyType* key) const
    {
        increaseOperationCountSinceLastCleanup();
        return m_map.contains(key);
    }

    bool contains(const KeyType& key) const
    {
        return contains(&key);
    }

    typename ValueTraits::TakeType take(const KeyType* key)
    {
        amortizedCleanupIfNeeded();
        return m_map.take(key);
    }

    typename ValueTraits::TakeType take(const KeyType& key)
    {
        return take(&key);
    }

    std::optional<ValueType> takeOptional(const KeyType* key)
    {
        amortizedCleanupIfNeeded();
        return m_map.takeOptional(key);
    }

    std::optional<ValueType> takeOptional(const KeyType& key)
    {
        return takeOptional(&key);
    }

    typename ValueTraits::PeekType get(const KeyType* key) const
    {
        increaseOperationCountSinceLastCleanup();
        return m_map.get(key);
    }

    typename ValueTraits::PeekType get(const KeyType& key) const
    {
        return get(&key);
    }

    std::optional<ValueType> getOptional(const KeyType* key) const
    {
        increaseOperationCountSinceLastCleanup();
        return m_map.getOptional(key);
    }

    std::optional<ValueType> getOptional(const KeyType& key) const
    {
        return getOptional(&key);
    }

    bool remove(iterator it)
    {
        auto didRemove = m_map.remove(it.m_position);
        amortizedCleanupIfNeeded();
        return didRemove;
    }

    bool remove(const KeyType* key)
    {
        amortizedCleanupIfNeeded();
        return m_map.remove(key);
    }

    bool remove(const KeyType& key)
    {
        return remove(&key);
    }

    template<typename Functor>
    bool removeIf(NOESCAPE Functor&& functor)
    {
        bool result = m_map.removeIf([&](auto& entry) {
            auto* key = entry.key.get();
            if (!key)
                return true;
            PeekType peek { *key, entry.value };
            return functor(peek);
        });
        cleanupHappened();
        return result;
    }

    void clear()
    {
        m_map.clear();
        cleanupHappened();
    }

    unsigned capacity() const { return m_map.capacity(); }

    bool isEmptyIgnoringNullReferences() const
    {
        if (m_map.isEmpty())
            return true;

        auto onlyContainsNullReferences = begin() == end();
        if (onlyContainsNullReferences) [[unlikely]]
            const_cast<WeakHashMap&>(*this).clear();
        return onlyContainsNullReferences;
    }

    bool hasNullReferences() const
    {
        unsigned count = 0;
        for (auto _ : m_map) {
            UNUSED_VARIABLE(_);
            ++count;
        }

        bool result = count != m_map.size();

        if (result)
            increaseOperationCountSinceLastCleanup(count);
        else
            cleanupHappened();
        return result;
    }

    unsigned computeSize() const
    {
        const_cast<WeakHashMap&>(*this).removeNullReferences();
        return m_map.size();
    }

    NEVER_INLINE void removeNullReferences()
    {
        m_map.removeWeakNullEntries();
        cleanupHappened();
    }

#if ASSERT_ENABLED
    void checkConsistency() const { m_map.checkConsistency(); }
#else
    void checkConsistency() const { }
#endif

private:
    ALWAYS_INLINE void cleanupHappened() const
    {
        m_operationCountSinceLastCleanup = 0;
        m_maxOperationCountWithoutCleanup = std::min(std::numeric_limits<unsigned>::max() / 2, m_map.size()) * 2;
    }

    ALWAYS_INLINE unsigned increaseOperationCountSinceLastCleanup(unsigned operationsPerformed = 1) const
    {
        unsigned currentCount = m_operationCountSinceLastCleanup;
        m_operationCountSinceLastCleanup += operationsPerformed;
        return currentCount;
    }

    ALWAYS_INLINE void amortizedCleanupIfNeeded(unsigned operationsPerformed = 1) const
    {
        unsigned currentCount = increaseOperationCountSinceLastCleanup(operationsPerformed);
        if (currentCount > m_maxOperationCountWithoutCleanup)
            const_cast<WeakHashMap&>(*this).removeNullReferences();
    }

    MapType m_map;
    mutable unsigned m_operationCountSinceLastCleanup { 0 };
    mutable unsigned m_maxOperationCountWithoutCleanup { 0 };

    template<typename, typename, typename, typename> friend class WeakHashMapIteratorBase;
};

} // namespace WTF

using WTF::WeakHashMap;
