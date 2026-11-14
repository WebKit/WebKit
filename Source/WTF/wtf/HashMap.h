/*
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <initializer_list>
#include <wtf/Compiler.h>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashIterators.h>
#include <wtf/HashTable.h>
#include <wtf/HashTraits.h>
#include <wtf/IteratorRange.h>
#include <wtf/KeyValuePair.h>
#include <wtf/OptionSetHash.h>
#include <wtf/Packed.h>
#include <wtf/RangeAdaptors.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

template<typename T> struct KeyValuePairKeyExtractor {
    static const typename T::KeyType& extract(const T& p) { return p.key; }
};

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename Malloc>
class HashMap final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HashMap);
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

    using HashTableType = typename TableTraitsArg::template TableType<KeyType, KeyValuePairType, KeyValuePairKeyExtractor<KeyValuePairType>, HashFunctions, KeyValuePairTraits, KeyTraits, Malloc>;

    class HashMapKeysProxy;
    class HashMapValuesProxy;

    using IdentityTranslatorType = typename HashTableType::IdentityTranslatorType;

public:
    /*
     * Since figuring out the entries of an iterator is confusing, here is a cheat sheet:
     * const KeyType& key = iterator->key;
     * ValueType& value = iterator->value;
     */
    using iterator = HashTableIteratorAdapter<HashTableType, KeyValuePairType>;
    using const_iterator = HashTableConstIteratorAdapter<HashTableType, KeyValuePairType>;

    using KeysIteratorRange = SizedIteratorRange<HashMap, typename iterator::Keys>;
    using KeysConstIteratorRange = SizedIteratorRange<HashMap, typename const_iterator::Keys>;
    using ValuesIteratorRange = SizedIteratorRange<HashMap, typename iterator::Values>;
    using ValuesConstIteratorRange = SizedIteratorRange<HashMap, typename const_iterator::Values>;

    /*
     * Since figuring out the entries of an AddResult is confusing, here is a cheat sheet:
     * iterator iter = addResult.iterator;
     * bool isNewEntry = addResult.isNewEntry;
     */
    using AddResult = typename HashTableType::AddResult;

public:
    HashMap() = default;

    template<typename Range>
        requires std::ranges::input_range<Range> && std::convertible_to<std::ranges::range_value_t<Range>, KeyValuePairType>
    explicit HashMap(FromRange, Range&& range)
    {
        if constexpr (std::ranges::sized_range<Range>)
            reserveInitialCapacity(std::ranges::size(range));
        for (auto&& keyValuePair : range) {
            if constexpr (std::is_rvalue_reference_v<Range&&>)
                add(WTF::move(keyValuePair.key), WTF::move(keyValuePair.value));
            else
                add(keyValuePair.key, keyValuePair.value);
        }
    }

    HashMap(std::initializer_list<KeyValuePairType> initializerList)
    {
        reserveInitialCapacity(initializerList.size());
        for (const auto& keyValuePair : initializerList)
            add(keyValuePair.key, keyValuePair.value);
    }
    
    template<typename... Items>
    static HashMap from(Items&&... items)
    {
        HashMap result;
        result.reserveInitialCapacity(sizeof...(items));
        result.addForInitialization(std::forward<Items>(items)...);
        return result;
    }

    void swap(HashMap&);

    unsigned size() const;
    unsigned capacity() const;
    size_t byteSize() const;
    bool isEmpty() const;

    void reserveInitialCapacity(unsigned keyCount) { m_impl.reserveInitialCapacity(keyCount); }

    // iterators iterate over pairs of keys and values
    iterator begin() LIFETIME_BOUND;
    iterator end() LIFETIME_BOUND;
    const_iterator begin() const LIFETIME_BOUND;
    const_iterator end() const LIFETIME_BOUND;
    
    iterator random() LIFETIME_BOUND { return m_impl.random(); }
    const_iterator random() const LIFETIME_BOUND { return m_impl.random(); }

    KeysIteratorRange keys() LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }
    const KeysConstIteratorRange keys() const LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }

    ValuesIteratorRange values() LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().values(), end().values()); }
    const ValuesConstIteratorRange values() const LIFETIME_BOUND { return makeSizedIteratorRange(*this, begin().values(), end().values()); }

    iterator find(const KeyType&) LIFETIME_BOUND;
    const_iterator find(const KeyType&) const LIFETIME_BOUND;
    bool contains(const KeyType&) const;
    MappedPeekType get(const KeyType&) const;
    std::optional<MappedType> getOptional(const KeyType&) const;

    // Same as get(), but aggressively inlined.
    MappedPeekType inlineGet(const KeyType&) const;

    ALWAYS_INLINE bool isNullStorage() const { return m_impl.isNullStorage(); }

    // Replaces the value but not the key if the key is already present.
    // Return value includes both an iterator to the key location,
    // and an isNewEntry boolean that's true if a new entry was added.
    template<typename V> AddResult set(const KeyType&, V&&) LIFETIME_BOUND;
    template<typename V> AddResult set(KeyType&&, V&&) LIFETIME_BOUND;

    // Does nothing if the key is already present.
    // Return value includes both an iterator to the key location,
    // and an isNewEntry boolean that's true if a new entry was added.
    template<typename V> AddResult add(const KeyType&, V&&) LIFETIME_BOUND;
    template<typename V> AddResult add(KeyType&&, V&&) LIFETIME_BOUND;

    // Same as add(), but aggressively inlined.
    template<typename V> AddResult fastAdd(const KeyType&, V&&) LIFETIME_BOUND;
    template<typename V> AddResult fastAdd(KeyType&&, V&&) LIFETIME_BOUND;

    AddResult ensure(const KeyType&, NOESCAPE const Invocable<MappedType()> auto&) LIFETIME_BOUND;
    AddResult ensure(KeyType&&, NOESCAPE const Invocable<MappedType()> auto&) LIFETIME_BOUND;

    bool remove(const KeyType&);
    bool remove(iterator);
    // FIXME: This feels like it should be Invocable<bool(const KeyValuePairType&)>
    bool removeIf(NOESCAPE const Invocable<bool(KeyValuePairType&)> auto&);
    void clear();

    MappedTakeType take(const KeyType&); // efficient combination of get with remove
    MappedTakeType take(iterator);
    std::optional<MappedType> takeOptional(const KeyType&);
    MappedTakeType takeFirst();

    // Useful when the key type is WeakPtr
    size_t computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction);
    size_t computeSize() requires (KeyTraits::hasIsWeakNullValueFunction);
    bool computeIsEmpty() const requires (KeyTraits::hasIsWeakNullValueFunction);
    bool computeIsEmpty() requires (KeyTraits::hasIsWeakNullValueFunction);
    void removeWeakNullEntries() requires (KeyTraits::hasIsWeakNullValueFunction);

    // Alternate versions of find() / contains() / get() / remove() that find the object
    // by hashing and comparing with some other type, to avoid the cost of type conversion.
    // HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator, typename T> iterator find(const T&) LIFETIME_BOUND;
    template<typename HashTranslator, typename T> const_iterator find(const T&) const LIFETIME_BOUND;
    template<typename HashTranslator, typename T> bool contains(const T&) const;
    template<typename HashTranslator, typename T> MappedPeekType get(const T&) const;
    template<typename HashTranslator, typename T> MappedPeekType inlineGet(const T&) const;
    template<typename HashTranslator, typename T> bool remove(const T&);

    // Alternate versions of add() / ensure() that find the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table. HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    //   static void translate(KeyType* uninitializedLocation, const T&, unsigned hashCode);
    template<typename HashTranslator, typename K, typename V> AddResult add(K&&, V&&) LIFETIME_BOUND;
    template<typename HashTranslator> AddResult ensure(auto&& key, NOESCAPE const Invocable<MappedType()> auto&) LIFETIME_BOUND;

    // Overloads for smart pointer keys that take the raw pointer type as the parameter.
    template<SmartPtr K = KeyType> iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) LIFETIME_BOUND;
    template<SmartPtr K = KeyType> const_iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const LIFETIME_BOUND;
    template<SmartPtr K = KeyType> bool contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType> MappedPeekType inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType> MappedPeekType get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType> std::optional<MappedType> getOptional(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType> bool remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*);
    template<SmartPtr K = KeyType> MappedTakeType take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*);

    // Overloads for smart pointer keys that take the raw reference type as the parameter.
    template<SmartPtr K = KeyType> iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) LIFETIME_BOUND { return find(&ref); }
    template<SmartPtr K = KeyType> const_iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) const LIFETIME_BOUND { return find(&ref); }
    template<SmartPtr K = KeyType> bool contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) const { return contains(&ref); }
    template<SmartPtr K = KeyType> MappedPeekType inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) const { return inlineGet(&ref); }
    template<SmartPtr K = KeyType> MappedPeekType get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) const { return get(&ref); }
    template<SmartPtr K = KeyType> std::optional<MappedType> getOptional(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) const { return getOptional(&ref); }
    template<SmartPtr K = KeyType> bool remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) { return remove(&ref); }
    template<SmartPtr K = KeyType> MappedTakeType take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& ref) { return take(&ref); }

    void checkConsistency() const;

    static bool isValidKey(const KeyType& key) { return HashTableType::isValidKey(key); }

private:
    template<typename K, typename V>
    AddResult inlineSet(K&&, V&&);

    template<typename K, typename V>
    AddResult inlineAdd(K&&, V&&);

    AddResult inlineEnsure(auto&& key, NOESCAPE const Invocable<MappedType()> auto&);

    template<typename... Items>
    void addForInitialization(KeyValuePairType&& item, Items&&... items)
    {
        add(WTF::move(item.key), WTF::move(item.value));
        addForInitialization(std::forward<Items>(items)...);
    }

    void addForInitialization(KeyValuePairType&& item)
    {
        add(WTF::move(item.key), WTF::move(item.value));
    }

    HashTableType m_impl;
} SWIFT_SELF_CONTAINED_IF(KeyArg, MappedArg) SWIFT_COPYABLE_IF(KeyArg, MappedArg);

template<typename ValueTraits, typename HashFunctions>
struct HashMapTranslator {
    static unsigned hash(const auto& key) { return HashFunctions::hash(key); }
    static bool equal(const auto& a, const auto& b) { return HashFunctions::equal(a, b); }
    template<typename U> static void translate(typename ValueTraits::TraitType* location, U&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor)
    {
        std::construct_at(std::addressof(location->key), std::forward<U>(key));
        std::construct_at(std::addressof(location->value), functor());
    }
};

template<typename ValueTraits, typename Translator>
struct HashMapTranslatorAdapter {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(typename ValueTraits::TraitType* location, auto&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor, unsigned hashCode)
    {
        Translator::translate(std::addressof(location->key), key, hashCode);
        std::construct_at(std::addressof(location->value), functor());
    }
};

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline void HashMap<T, U, V, W, X, Y, M>::swap(HashMap& other)
{
    m_impl.swap(other.m_impl);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline unsigned HashMap<T, U, V, W, X, Y, M>::size() const
{
    return m_impl.size(); 
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline unsigned HashMap<T, U, V, W, X, Y, M>::capacity() const
{ 
    return m_impl.capacity(); 
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline size_t HashMap<T, U, V, W, X, Y, M>::byteSize() const
{
    return m_impl.byteSize();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline bool HashMap<T, U, V, W, X, Y, M>::isEmpty() const
{
    return m_impl.isEmpty();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::begin() LIFETIME_BOUND -> iterator
{
    return m_impl.begin();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::end() LIFETIME_BOUND -> iterator
{
    return m_impl.end();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::begin() const LIFETIME_BOUND -> const_iterator
{
    return m_impl.begin();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::end() const LIFETIME_BOUND -> const_iterator
{
    return m_impl.end();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::find(const KeyType& key) LIFETIME_BOUND -> iterator
{
    return m_impl.find(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::find(const KeyType& key) const LIFETIME_BOUND -> const_iterator
{
    return m_impl.find(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline bool HashMap<T, U, V, W, X, Y, M>::contains(const KeyType& key) const
{
    return m_impl.contains(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
inline auto HashMap<T, U, V, W, X, Y, M>::find(const TYPE& value) LIFETIME_BOUND -> iterator
{
    return m_impl.template find<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
inline auto HashMap<T, U, V, W, X, Y, M>::find(const TYPE& value) const LIFETIME_BOUND -> const_iterator
{
    return m_impl.template find<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
auto HashMap<T, U, V, W, X, Y, M>::get(const TYPE& value) const -> MappedPeekType
{
    auto* entry = const_cast<HashTableType&>(m_impl).template lookup<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(value);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
auto HashMap<T, U, V, W, X, Y, M>::inlineGet(const TYPE& value) const -> MappedPeekType
{
    auto* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(value);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
inline bool HashMap<T, U, V, W, X, Y, M>::contains(const TYPE& value) const
{
    return m_impl.template contains<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<typename HashTranslator, typename TYPE>
inline bool HashMap<T, U, V, W, X, Y, M>::remove(const TYPE& value)
{
    auto it = find<HashTranslator>(value);
    if (it == end())
        return false;
    remove(it);
    return true;
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename K, typename V>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::inlineSet(K&& key, V&& value) -> AddResult
{
    AddResult result = inlineAdd(std::forward<K>(key), std::forward<V>(value));
    if (!result.isNewEntry) {
        // The inlineAdd call above found an existing hash table entry; we need to set the mapped value.
        result.iterator->value = std::forward<V>(value);
    }
    return result;
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename K, typename V>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::inlineAdd(K&& key, V&& value) -> AddResult
{
    return m_impl.template add<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(std::forward<K>(key), [&] ALWAYS_INLINE_LAMBDA -> MappedType { return std::forward<V>(value); });
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename K>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::inlineEnsure(K&& key, NOESCAPE const Invocable<MappedType()> auto& functor) -> AddResult
{
    return m_impl.template add<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(std::forward<K>(key), functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::set(const KeyType& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineSet(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::set(KeyType&& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineSet(WTF::move(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename HashTranslator, typename K>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::ensure(K&& key, NOESCAPE const Invocable<MappedType()> auto& functor) LIFETIME_BOUND -> AddResult
{
    return m_impl.template addPassingHashCode<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(std::forward<K>(key), functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename HashTranslator, typename K, typename V>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::add(K&& key, V&& value) LIFETIME_BOUND -> AddResult
{
    return m_impl.template addPassingHashCode<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>>(std::forward<K>(key), [&] ALWAYS_INLINE_LAMBDA -> MappedType { return std::forward<V>(value); });
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::add(const KeyType& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineAdd(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::add(KeyType&& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineAdd(WTF::move(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::fastAdd(const KeyType& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineAdd(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
template<typename T>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::fastAdd(KeyType&& key, T&& mapped) LIFETIME_BOUND -> AddResult
{
    return inlineAdd(WTF::move(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::ensure(const KeyType& key, NOESCAPE const Invocable<MappedType()> auto& functor) LIFETIME_BOUND -> AddResult
{
    return inlineEnsure(key, functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, typename M>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, M>::ensure(KeyType&& key, NOESCAPE const Invocable<MappedType()> auto& functor) LIFETIME_BOUND -> AddResult
{
    return inlineEnsure(std::forward<KeyType>(key), functor);
}
    
template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::get(const KeyType& key) const -> MappedPeekType
{
    return get<IdentityTranslatorType>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline auto HashMap<T, U, V, W, X, Y, M>::getOptional(const KeyType& key) const -> std::optional<MappedType>
{
    auto* entry = const_cast<HashTableType&>(m_impl).template lookup<IdentityTranslatorType>(key);
    if (!entry)
        return { };
    return { entry->value };
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, typename M>
ALWAYS_INLINE auto HashMap<T, U, V, W, MappedTraits, Y, M>::inlineGet(const KeyType& key) const -> MappedPeekType
{
    KeyValuePairType* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<IdentityTranslatorType>(key);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline bool HashMap<T, U, V, W, X, Y, M>::remove(iterator it)
{
    if (it.m_impl == m_impl.end())
        return false;
    m_impl.internalCheckTableConsistency();
    m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    return true;
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline bool HashMap<T, U, V, W, X, Y, M>::removeIf(NOESCAPE const Invocable<bool(KeyValuePairType&)> auto& functor)
{
    return m_impl.removeIf(functor);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline bool HashMap<T, U, V, W, X, Y, M>::remove(const KeyType& key)
{
    return remove(find(key));
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline void HashMap<T, U, V, W, X, Y, M>::clear()
{
    m_impl.clear();
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, M>::take(const KeyType& key) -> MappedTakeType
{
    return take(find(key));
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, M>::take(iterator it) -> MappedTakeType
{
    if (it == end()) {
        if constexpr (requires { MappedTraits::emptyTakeValue(); })
            return MappedTraits::emptyTakeValue();
        else
            return MappedTraits::take(MappedTraits::emptyValue());
    }
    auto value = MappedTraits::take(WTF::move(it->value));
    remove(it);
    return value;
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, M>::takeOptional(const KeyType& key) -> std::optional<MappedType>
{
    auto it = find(key);
    if (it == end())
        return std::nullopt;
    return take(it);
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, M>::takeFirst() -> MappedTakeType
{
    return take(begin());
}

template<typename T, typename U, typename V, typename KeyTraits, typename MappedTraits, typename Y, typename M>
inline size_t HashMap<T, U, V, KeyTraits, MappedTraits, Y, M>::computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeSize();
}

template<typename T, typename U, typename V, typename KeyTraits, typename MappedTraits, typename Y, typename M>
inline size_t HashMap<T, U, V, KeyTraits, MappedTraits, Y, M>::computeSize() requires (KeyTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeSize();
}

template<typename T, typename U, typename V, typename KeyTraits, typename MappedTraits, typename Y, typename M>
inline bool HashMap<T, U, V, KeyTraits, MappedTraits, Y, M>::computeIsEmpty() const requires (KeyTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeIsEmpty();
}

template<typename T, typename U, typename V, typename KeyTraits, typename MappedTraits, typename Y, typename M>
inline bool HashMap<T, U, V, KeyTraits, MappedTraits, Y, M>::computeIsEmpty() requires (KeyTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeIsEmpty();
}

template<typename T, typename U, typename V, typename KeyTraits, typename MappedTraits, typename Y, typename M>
inline void HashMap<T, U, V, KeyTraits, MappedTraits, Y, M>::removeWeakNullEntries() requires (KeyTraits::hasIsWeakNullValueFunction)
{
    m_impl.removeWeakNullEntries();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) LIFETIME_BOUND -> iterator
{
    return m_impl.template find<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const LIFETIME_BOUND -> const_iterator
{
    return m_impl.template find<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> bool
{
    return m_impl.template contains<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> MappedPeekType
{
    KeyValuePairType* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(key);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
auto HashMap<T, U, V, W, X, Y, M>::get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> MappedPeekType
{
    return inlineGet(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::getOptional(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> std::optional<MappedType>
{
    auto* entry = const_cast<HashTableType&>(m_impl).template lookup<HashMapTranslator<KeyValuePairTraits, HashFunctions>>(key);
    if (!entry)
        return { };
    return { entry->value };
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) -> bool
{
    return remove(find(key));
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, M>::take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) -> MappedTakeType
{
    iterator it = find(key);
    if (it == end()) {
        if constexpr (requires { MappedTraits::emptyTakeValue(); })
            return MappedTraits::emptyTakeValue();
        else
            return MappedTraits::take(MappedTraits::emptyValue());
    }
    auto value = MappedTraits::take(WTF::move(it->value));
    remove(it);
    return value;
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
inline void HashMap<T, U, V, W, X, Y, M>::checkConsistency() const
{
    m_impl.checkTableConsistency();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, typename M>
bool operator==(const HashMap<T, U, V, W, X, Y, M>& a, const HashMap<T, U, V, W, X, Y, M>& b)
{
    if (a.size() != b.size())
        return false;

    typedef typename HashMap<T, U, V, W, X, Y, M>::const_iterator const_iterator;

    const_iterator end = a.end();
    const_iterator notFound = b.end();
    for (const_iterator it = a.begin(); it != end; ++it) {
        const_iterator bPos = b.find(it->key);
        if (bPos == notFound || it->value != bPos->value)
            return false;
    }

    return true;
}

} // namespace WTF

using WTF::HashMap;
