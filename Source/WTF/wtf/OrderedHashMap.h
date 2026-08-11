/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <initializer_list>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/IteratorRange.h>
#include <wtf/KeyValuePair.h>
#include <wtf/OrderedHashTable.h>

namespace WTF {

template<typename TableType, typename KeyType, typename MappedType>
struct OrderedHashTableConstKeysIterator;
template<typename TableType, typename KeyType, typename MappedType>
struct OrderedHashTableKeysIterator;
template<typename TableType, typename KeyType, typename MappedType>
struct OrderedHashTableConstValuesIterator;
template<typename TableType, typename KeyType, typename MappedType>
struct OrderedHashTableValuesIterator;

// Iterator adapters for OrderedHashMap (KeyValuePair specialization)

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableConstIteratorAdapter {
    using iterator_category = std::forward_iterator_tag;
    using value_type = KeyValuePair<KeyType, MappedType>;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using ValueType = KeyValuePair<KeyType, MappedType>;

    using Keys = OrderedHashTableConstKeysIterator<HashTableType, KeyType, MappedType>;
    using Values = OrderedHashTableConstValuesIterator<HashTableType, KeyType, MappedType>;

    OrderedHashTableConstIteratorAdapter() = default;
    OrderedHashTableConstIteratorAdapter(const typename HashTableType::const_iterator& impl)
        : m_impl(impl)
    { }

    const ValueType* get() const { return m_impl.get(); }
    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstIteratorAdapter& operator++() { ++m_impl; return *this; }

    Keys keys() { return Keys(*this); }
    Values values() { return Values(*this); }

    typename HashTableType::const_iterator m_impl;
};

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableIteratorAdapter {
    using iterator_category = std::forward_iterator_tag;
    using value_type = KeyValuePair<KeyType, MappedType>;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using ValueType = KeyValuePair<KeyType, MappedType>;

    using Keys = OrderedHashTableKeysIterator<HashTableType, KeyType, MappedType>;
    using Values = OrderedHashTableValuesIterator<HashTableType, KeyType, MappedType>;

    OrderedHashTableIteratorAdapter() = default;
    OrderedHashTableIteratorAdapter(const typename HashTableType::iterator& impl)
        : m_impl(impl)
    { }

    ValueType* get() const { return m_impl.get(); }
    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableIteratorAdapter& operator++() { ++m_impl; return *this; }

    operator OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType>() const
    {
        return OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType>(m_impl);
    }

    Keys keys() { return Keys(*this); }
    Values values() { return Values(*this); }

    typename HashTableType::iterator m_impl;
};

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableConstIteratorAdapter<T, U, V>& a, const OrderedHashTableConstIteratorAdapter<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableIteratorAdapter<T, U, V>& a, const OrderedHashTableIteratorAdapter<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

// Const/non-const comparison
template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableIteratorAdapter<T, U, V>& a, const OrderedHashTableConstIteratorAdapter<T, U, V>& b)
{
    return OrderedHashTableConstIteratorAdapter<T, U, V>(a) == b;
}

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableConstIteratorAdapter<T, U, V>& a, const OrderedHashTableIteratorAdapter<T, U, V>& b)
{
    return a == OrderedHashTableConstIteratorAdapter<T, U, V>(b);
}

// Keys iterators

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableConstKeysIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = KeyType;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    OrderedHashTableConstKeysIterator() = default;
    OrderedHashTableConstKeysIterator(const OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType>& impl)
        : m_impl(impl)
    { }

    const KeyType* get() const { return &(m_impl.get()->key); }
    const KeyType& operator*() const { return *get(); }
    const KeyType* operator->() const { return get(); }

    OrderedHashTableConstKeysIterator& operator++() { ++m_impl; return *this; }

    OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType> m_impl;
};

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableKeysIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = KeyType;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    OrderedHashTableKeysIterator() = default;
    OrderedHashTableKeysIterator(const OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType>& impl)
        : m_impl(impl)
    { }

    KeyType* get() const { return &(m_impl.get()->key); }
    KeyType& operator*() const { return *get(); }
    KeyType* operator->() const { return get(); }

    OrderedHashTableKeysIterator& operator++() { ++m_impl; return *this; }

    operator OrderedHashTableConstKeysIterator<HashTableType, KeyType, MappedType>() const
    {
        return OrderedHashTableConstKeysIterator<HashTableType, KeyType, MappedType>(m_impl);
    }

    OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType> m_impl;
};

// Values iterators

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableConstValuesIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = MappedType;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    OrderedHashTableConstValuesIterator() = default;
    OrderedHashTableConstValuesIterator(const OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType>& impl)
        : m_impl(impl)
    { }

    const MappedType* get() const { return std::addressof(m_impl.get()->value); }
    const MappedType& operator*() const { return *get(); }
    const MappedType* operator->() const { return get(); }

    OrderedHashTableConstValuesIterator& operator++() { ++m_impl; return *this; }

    OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType> m_impl;
};

template<typename HashTableType, typename KeyType, typename MappedType>
struct OrderedHashTableValuesIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = MappedType;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    OrderedHashTableValuesIterator() = default;
    OrderedHashTableValuesIterator(const OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType>& impl)
        : m_impl(impl)
    { }

    MappedType* get() const { return std::addressof(m_impl.get()->value); }
    MappedType& operator*() const { return *get(); }
    MappedType* operator->() const { return get(); }

    OrderedHashTableValuesIterator& operator++() { ++m_impl; return *this; }

    operator OrderedHashTableConstValuesIterator<HashTableType, KeyType, MappedType>() const
    {
        return OrderedHashTableConstValuesIterator<HashTableType, KeyType, MappedType>(m_impl);
    }

    OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType> m_impl;
};

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableConstKeysIterator<T, U, V>& a, const OrderedHashTableConstKeysIterator<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableKeysIterator<T, U, V>& a, const OrderedHashTableKeysIterator<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableConstValuesIterator<T, U, V>& a, const OrderedHashTableConstValuesIterator<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

template<typename T, typename U, typename V>
inline bool operator==(const OrderedHashTableValuesIterator<T, U, V>& a, const OrderedHashTableValuesIterator<T, U, V>& b)
{
    return a.m_impl == b.m_impl;
}

// OrderedHashMap

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename Malloc>
class OrderedHashMap final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OrderedHashMap);
private:
    using KeyTraits = KeyTraitsArg;
    using MappedTraits = MappedTraitsArg;

    struct KeyValuePairTraits : KeyValuePairHashTraits<KeyTraits, MappedTraits> {
        static constexpr bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const typename KeyValuePairHashTraits<KeyTraits, MappedTraits>::TraitType& value)
        {
            return isHashTraitsEmptyValue<KeyTraits>(value.key);
        }
    };

public:
    using KeyType = typename KeyTraits::TraitType;
    using MappedType = typename MappedTraits::TraitType;
    using KeyValuePairType = typename KeyValuePairTraits::TraitType;

private:
    using MappedPeekType = typename MappedTraits::PeekType;
    using MappedTakeType = typename MappedTraits::TakeType;

    using HashFunctions = HashArg;

    using HashTableType = OrderedHashTable<KeyType, KeyValuePairType, KeyValuePairKeyExtractor<KeyValuePairType>, HashFunctions, KeyValuePairTraits, KeyTraits, Malloc>;

public:
    struct AddResult {
        using IteratorType = OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType>;
        IteratorType iterator;
        bool isNewEntry;
    };

    using iterator = OrderedHashTableIteratorAdapter<HashTableType, KeyType, MappedType>;
    using const_iterator = OrderedHashTableConstIteratorAdapter<HashTableType, KeyType, MappedType>;

    using KeysIteratorRange = SizedIteratorRange<OrderedHashMap, typename iterator::Keys>;
    using KeysConstIteratorRange = SizedIteratorRange<OrderedHashMap, typename const_iterator::Keys>;
    using ValuesIteratorRange = SizedIteratorRange<OrderedHashMap, typename iterator::Values>;
    using ValuesConstIteratorRange = SizedIteratorRange<OrderedHashMap, typename const_iterator::Values>;

    OrderedHashMap() = default;

    OrderedHashMap(std::initializer_list<KeyValuePairType> initializerList)
    {
        reserveInitialCapacity(initializerList.size());
        for (const auto& keyValuePair : initializerList)
            add(keyValuePair.key, keyValuePair.value);
    }

    void swap(OrderedHashMap& other) { m_impl.swap(other.m_impl); }

    unsigned size() const { return m_impl.size(); }
    unsigned capacity() const { return m_impl.capacity(); }
    bool isEmpty() const { return m_impl.isEmpty(); }

    void reserveInitialCapacity(unsigned keyCount) { m_impl.reserveInitialCapacity(keyCount); }

    iterator begin() LIFETIME_BOUND { return m_impl.begin(); }
    iterator end() LIFETIME_BOUND { return m_impl.end(); }
    const_iterator begin() const LIFETIME_BOUND { return m_impl.begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_impl.end(); }

    KeysIteratorRange keys() LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }
    const KeysConstIteratorRange keys() const LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }

    ValuesIteratorRange values() LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().values(), end().values()); }
    const ValuesConstIteratorRange values() const LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().values(), end().values()); }

    iterator find(const KeyType& key) LIFETIME_BOUND { return m_impl.find(key); }
    const_iterator find(const KeyType& key) const LIFETIME_BOUND { return m_impl.find(key); }
    bool contains(const KeyType& key) const { return m_impl.contains(key); }

    MappedPeekType get(const KeyType& key) const
    {
        auto* entry = m_impl.lookup(key);
        if (!entry)
            return MappedTraits::peek(MappedTraits::emptyValue());
        return MappedTraits::peek(entry->value);
    }

    std::optional<MappedType> getOptional(const KeyType& key) const
    {
        auto* entry = m_impl.lookup(key);
        if (!entry)
            return { };
        return { entry->value };
    }

    MappedPeekType inlineGet(const KeyType& key) const { return get(key); }

    template<typename V>
    AddResult set(const KeyType& key, V&& value) LIFETIME_BOUND
    {
        return inlineSet(key, std::forward<V>(value));
    }

    template<typename V>
    AddResult set(KeyType&& key, V&& value) LIFETIME_BOUND
    {
        return inlineSet(std::forward<KeyType>(key), std::forward<V>(value));
    }

    template<typename V>
    AddResult add(const KeyType& key, V&& value) LIFETIME_BOUND
    {
        return inlineAdd(key, std::forward<V>(value));
    }

    template<typename V>
    AddResult add(KeyType&& key, V&& value) LIFETIME_BOUND
    {
        return inlineAdd(std::forward<KeyType>(key), std::forward<V>(value));
    }

    AddResult ensure(const KeyType& key, NOESCAPE const Invocable<MappedType()> auto& functor) LIFETIME_BOUND
    {
        return inlineEnsure(key, functor);
    }

    AddResult ensure(KeyType&& key, NOESCAPE const Invocable<MappedType()> auto& functor) LIFETIME_BOUND
    {
        return inlineEnsure(std::forward<KeyType>(key), functor);
    }

    bool remove(const KeyType& key)
    {
        auto it = find(key);
        if (it == end())
            return false;
        remove(it);
        return true;
    }

    bool remove(iterator it)
    {
        if (it == end())
            return false;
        m_impl.remove(it.m_impl);
        return true;
    }

    bool removeIf(NOESCAPE const Invocable<bool(KeyValuePairType&)> auto& functor)
    {
        return m_impl.removeIf(functor);
    }

    void clear() { m_impl.clear(); }

    MappedTakeType take(const KeyType& key)
    {
        return take(find(key));
    }

    MappedTakeType take(iterator it)
    {
        if (it == end())
            return MappedTraits::take(MappedTraits::emptyValue());
        auto value = MappedTraits::take(WTF::move(it->value));
        remove(it);
        return value;
    }

    std::optional<MappedType> takeOptional(const KeyType& key)
    {
        auto it = find(key);
        if (it == end())
            return std::nullopt;
        return take(it);
    }

    MappedTakeType takeFirst() { return take(begin()); }

    // HashTranslator versions
    template<typename HashTranslator, typename T>
    iterator find(const T& key) LIFETIME_BOUND
    {
        return m_impl.template find<HashTranslator>(key);
    }

    template<typename HashTranslator, typename T>
    const_iterator find(const T& key) const LIFETIME_BOUND
    {
        return m_impl.template find<HashTranslator>(key);
    }

    template<typename HashTranslator, typename T>
    bool contains(const T& key) const
    {
        return m_impl.template contains<HashTranslator>(key);
    }

    template<typename HashTranslator, typename T>
    MappedPeekType get(const T& key) const
    {
        auto* entry = m_impl.template lookup<HashTranslator>(key);
        if (!entry)
            return MappedTraits::peek(MappedTraits::emptyValue());
        return MappedTraits::peek(entry->value);
    }

    static bool isValidKey(const KeyType& key)
    {
        if (KeyTraits::isDeletedValue(key))
            return false;
        if constexpr (HashFunctions::safeToCompareToEmptyOrDeleted) {
            if (key == KeyTraits::emptyValue())
                return false;
        } else {
            if (isHashTraitsEmptyValue<KeyTraits>(key))
                return false;
        }
        return true;
    }

private:
    template<typename K, typename V>
    AddResult inlineSet(K&& key, V&& value)
    {
        auto tableResult = m_impl.add(key, [&]() ALWAYS_INLINE_LAMBDA -> KeyValuePairType {
            return KeyValuePairType(std::forward<K>(key), std::forward<V>(value));
        });
        auto it = iterator(tableResult.iterator);
        if (!tableResult.isNewEntry)
            it->value = std::forward<V>(value);
        return { it, tableResult.isNewEntry };
    }

    template<typename K, typename V>
    AddResult inlineAdd(K&& key, V&& value)
    {
        auto tableResult = m_impl.add(key, [&]() ALWAYS_INLINE_LAMBDA -> KeyValuePairType {
            return KeyValuePairType(std::forward<K>(key), std::forward<V>(value));
        });
        return { iterator(tableResult.iterator), tableResult.isNewEntry };
    }

    template<typename K>
    AddResult inlineEnsure(K&& key, NOESCAPE const Invocable<MappedType()> auto& functor)
    {
        auto tableResult = m_impl.add(key, [&]() ALWAYS_INLINE_LAMBDA -> KeyValuePairType {
            return KeyValuePairType(std::forward<K>(key), functor());
        });
        return { iterator(tableResult.iterator), tableResult.isNewEntry };
    }

    HashTableType m_impl;
};

template<typename T, typename U, typename V, typename W, typename X, typename M, typename M2>
bool equalIgnoringOrder(const OrderedHashMap<T, U, V, W, X, M>& a, const OrderedHashMap<T, U, V, W, X, M2>& b)
{
    if (a.size() != b.size())
        return false;
    for (auto it = a.begin(); it != a.end(); ++it) {
        auto bPos = b.find(it->key);
        if (bPos == b.end() || it->value != bPos->value)
            return false;
    }
    return true;
}

} // namespace WTF

using WTF::OrderedHashMap;
