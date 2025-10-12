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

namespace WTF {

template<typename T> struct KeyValuePairKeyExtractor {
    static const typename T::KeyType& extract(const T& p) { return p.key; }
};

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename Malloc>
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
    
    iterator random() { return m_impl.random(); }
    const_iterator random() const { return m_impl.random(); }

    KeysIteratorRange keys() { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }
    const KeysConstIteratorRange keys() const { return makeSizedIteratorRange(*this, begin().keys(), end().keys()); }

    ValuesIteratorRange values() { return makeSizedIteratorRange(*this, begin().values(), end().values()); }
    const ValuesConstIteratorRange values() const { return makeSizedIteratorRange(*this, begin().values(), end().values()); }

    iterator find(const KeyType&);
    const_iterator find(const KeyType&) const;
    bool contains(const KeyType&) const;
    MappedPeekType get(const KeyType&) const;
    std::optional<MappedType> getOptional(const KeyType&) const;

    // Same as get(), but aggressively inlined.
    MappedPeekType inlineGet(const KeyType&) const;

    ALWAYS_INLINE bool isNullStorage() const { return m_impl.isNullStorage(); }

    // Replaces the value but not the key if the key is already present.
    // Return value includes both an iterator to the key location,
    // and an isNewEntry boolean that's true if a new entry was added.
    template<typename V> AddResult set(const KeyType&, V&&);
    template<typename V> AddResult set(KeyType&&, V&&);

    // Does nothing if the key is already present.
    // Return value includes both an iterator to the key location,
    // and an isNewEntry boolean that's true if a new entry was added.
    template<typename V> AddResult add(const KeyType&, V&&);
    template<typename V> AddResult add(KeyType&&, V&&);

    // Same as add(), but aggressively inlined.
    template<typename V> AddResult fastAdd(const KeyType&, V&&);
    template<typename V> AddResult fastAdd(KeyType&&, V&&);

    AddResult ensure(const KeyType&, NOESCAPE const Invocable<MappedType()> auto&);
    AddResult ensure(KeyType&&, NOESCAPE const Invocable<MappedType()> auto&);

    bool remove(const KeyType&);
    bool remove(iterator);
    // FIXME: This feels like it should be Invocable<bool(const KeyValuePairType&)>
    bool removeIf(NOESCAPE const Invocable<bool(KeyValuePairType&)> auto&);
    void clear();

    MappedTakeType take(const KeyType&); // efficient combination of get with remove
    MappedTakeType take(iterator);
    std::optional<MappedType> takeOptional(const KeyType&);
    MappedTakeType takeFirst();

    // Alternate versions of find() / contains() / get() / remove() that find the object
    // by hashing and comparing with some other type, to avoid the cost of type conversion.
    // HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator, typename T> iterator find(const T&);
    template<typename HashTranslator, typename T> const_iterator find(const T&) const;
    template<typename HashTranslator, typename T> bool contains(const T&) const;
    template<typename HashTranslator, typename T> MappedPeekType get(const T&) const;
    template<typename HashTranslator, typename T> MappedPeekType inlineGet(const T&) const;
    template<typename HashTranslator, typename T> bool remove(const T&);

    // Alternate versions of add() / ensure() that find the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table. HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    //   static translate(ValueType&, const T&, unsigned hashCode);
    template<typename HashTranslator, typename K, typename V> AddResult add(K&&, V&&);
    template<typename HashTranslator> AddResult ensure(auto&& key, NOESCAPE const Invocable<MappedType()> auto&);

    // Overloads for smart pointer keys that take the raw pointer type as the parameter.
    template<SmartPtr K = KeyType>
    iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*);
    template<SmartPtr K = KeyType>
    const_iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType>
    bool contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType>
    MappedPeekType inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType>
    MappedPeekType get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*) const;
    template<SmartPtr K = KeyType>
    bool remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*);
    template<SmartPtr K = KeyType>
    MappedTakeType take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>*);

    // Overloads for non-nullable smart pointer values that take the raw reference type as the parameter.
    template<NonNullableSmartPtr K = KeyType>
    iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&);
    template<NonNullableSmartPtr K = KeyType>
    const_iterator find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&) const;
    template<NonNullableSmartPtr K = KeyType>
    bool contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&) const;
    template<NonNullableSmartPtr K = KeyType>
    MappedPeekType inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&) const;
    template<NonNullableSmartPtr K = KeyType>
    MappedPeekType get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&) const;
    template<NonNullableSmartPtr K = KeyType>
    bool remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&);
    template<NonNullableSmartPtr K = KeyType>
    MappedTakeType take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>&);

    void checkConsistency() const;

    static bool isValidKey(const KeyType&);

private:
    template<typename K, typename V>
    AddResult inlineSet(K&&, V&&);

    template<typename K, typename V>
    AddResult inlineAdd(K&&, V&&);

    AddResult inlineEnsure(auto&& key, NOESCAPE const Invocable<MappedType()> auto&);

    template<typename... Items>
    void addForInitialization(KeyValuePairType&& item, Items&&... items)
    {
        add(WTFMove(item.key), WTFMove(item.value));
        addForInitialization(std::forward<Items>(items)...);
    }

    void addForInitialization(KeyValuePairType&& item)
    {
        add(WTFMove(item.key), WTFMove(item.value));
    }

    HashTableType m_impl;
};

template<typename ValueTraits, typename HashFunctions>
struct HashMapTranslator {
    static unsigned hash(const auto& key) { return HashFunctions::hash(key); }
    static bool equal(const auto& a, const auto& b) { return HashFunctions::equal(a, b); }
    template<typename U> static void translate(auto& location, U&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor)
    {
        ValueTraits::KeyTraits::assignToEmpty(location.key, std::forward<U>(key));
        ValueTraits::ValueTraits::assignToEmpty(location.value, functor());
    }
};

template<typename ValueTraits, typename HashFunctions>
struct HashMapEnsureTranslator {
    static unsigned hash(const auto& key) { return HashFunctions::hash(key); }
    static bool equal(const auto& a, const auto& b) { return HashFunctions::equal(a, b); }
    template<typename U> static void translate(auto& location, U&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor)
    {
        ValueTraits::KeyTraits::assignToEmpty(location.key, std::forward<U>(key));
        ValueTraits::ValueTraits::assignToEmpty(location.value, functor());
    }
};

template<typename ValueTraits, typename Translator>
struct HashMapTranslatorAdapter {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(auto& location, auto&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor, unsigned hashCode)
    {
        Translator::translate(location.key, key, hashCode);
        location.value = functor();
    }
};

template<typename ValueTraits, typename Translator>
struct HashMapEnsureTranslatorAdapter {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(auto& location, auto&& key, NOESCAPE const Invocable<typename ValueTraits::ValueTraits::TraitType()> auto& functor, unsigned hashCode)
    {
        Translator::translate(location.key, key, hashCode);
        location.value = functor();
    }
};

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline void HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::swap(HashMap& other)
{
    m_impl.swap(other.m_impl);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline unsigned HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::size() const
{
    return m_impl.size(); 
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline unsigned HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::capacity() const
{ 
    return m_impl.capacity(); 
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline size_t HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::byteSize() const
{
    return m_impl.byteSize();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::isEmpty() const
{
    return m_impl.isEmpty();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::begin() -> iterator
{
    return m_impl.begin();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::end() -> iterator
{
    return m_impl.end();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::begin() const -> const_iterator
{
    return m_impl.begin();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::end() const -> const_iterator
{
    return m_impl.end();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(const KeyType& key) -> iterator
{
    return m_impl.template find<shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(const KeyType& key) const -> const_iterator
{
    return m_impl.template find<shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::contains(const KeyType& key) const
{
    return m_impl.template contains<shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(const TYPE& value) -> iterator
{
    return m_impl.template find<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(const TYPE& value) const -> const_iterator
{
    return m_impl.template find<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::get(const TYPE& value) const -> MappedPeekType
{
    auto* entry = const_cast<HashTableType&>(m_impl).template lookup<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(value);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::inlineGet(const TYPE& value) const -> MappedPeekType
{
    auto* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(value);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::contains(const TYPE& value) const
{
    return m_impl.template contains<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename TYPE>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::remove(const TYPE& value)
{
    auto it = find<HashTranslator>(value);
    if (it == end())
        return false;
    remove(it);
    return true;
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename K, typename V>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::inlineSet(K&& key, V&& value) -> AddResult
{
    AddResult result = inlineAdd(std::forward<K>(key), std::forward<V>(value));
    if (!result.isNewEntry) {
        // The inlineAdd call above found an existing hash table entry; we need to set the mapped value.
        result.iterator->value = std::forward<V>(value);
    }
    return result;
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename K, typename V>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::inlineAdd(K&& key, V&& value) -> AddResult
{
    return m_impl.template add<HashMapTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(std::forward<K>(key), [&] () ALWAYS_INLINE_LAMBDA -> MappedType { return std::forward<V>(value); });
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename K>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::inlineEnsure(K&& key, NOESCAPE const Invocable<MappedType()> auto& functor) -> AddResult
{
    return m_impl.template add<HashMapEnsureTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(std::forward<K>(key), functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::set(const KeyType& key, T&& mapped) -> AddResult
{
    return inlineSet(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::set(KeyType&& key, T&& mapped) -> AddResult
{
    return inlineSet(WTFMove(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename K>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::ensure(K&& key, NOESCAPE const Invocable<MappedType()> auto& functor) -> AddResult
{
    return m_impl.template addPassingHashCode<HashMapEnsureTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(std::forward<K>(key), functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename HashTranslator, typename K, typename V>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::add(K&& key, V&& value) -> AddResult
{
    return m_impl.template addPassingHashCode<HashMapTranslatorAdapter<KeyValuePairTraits, HashTranslator>, shouldValidateKey>(std::forward<K>(key), [&] () ALWAYS_INLINE_LAMBDA -> MappedType { return std::forward<V>(value); });
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::add(const KeyType& key, T&& mapped) -> AddResult
{
    return inlineAdd(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::add(KeyType&& key, T&& mapped) -> AddResult
{
    return inlineAdd(WTFMove(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::fastAdd(const KeyType& key, T&& mapped) -> AddResult
{
    return inlineAdd(key, std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
template<typename T>
ALWAYS_INLINE auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::fastAdd(KeyType&& key, T&& mapped) -> AddResult
{
    return inlineAdd(WTFMove(key), std::forward<T>(mapped));
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::ensure(const KeyType& key, NOESCAPE const Invocable<MappedType()> auto& functor) -> AddResult
{
    return inlineEnsure(key, functor);
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg, typename TableTraitsArg, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, TableTraitsArg, shouldValidateKey, M>::ensure(KeyType&& key, NOESCAPE const Invocable<MappedType()> auto& functor) -> AddResult
{
    return inlineEnsure(std::forward<KeyType>(key), functor);
}
    
template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::get(const KeyType& key) const -> MappedPeekType
{
    return get<IdentityTranslatorType>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::getOptional(const KeyType& key) const -> std::optional<MappedType>
{
    auto* entry = const_cast<HashTableType&>(m_impl).template lookup<IdentityTranslatorType, shouldValidateKey>(key);
    if (!entry)
        return { };
    return { entry->value };
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, ShouldValidateKey shouldValidateKey, typename M>
ALWAYS_INLINE auto HashMap<T, U, V, W, MappedTraits, Y, shouldValidateKey, M>::inlineGet(const KeyType& key) const -> MappedPeekType
{
    KeyValuePairType* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<IdentityTranslatorType, shouldValidateKey>(key);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::remove(iterator it)
{
    if (it.m_impl == m_impl.end())
        return false;
    m_impl.internalCheckTableConsistency();
    m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    return true;
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::removeIf(NOESCAPE const Invocable<bool(KeyValuePairType&)> auto& functor)
{
    return m_impl.removeIf(functor);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::remove(const KeyType& key)
{
    return remove(find(key));
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline void HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::clear()
{
    m_impl.clear();
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, shouldValidateKey, M>::take(const KeyType& key) -> MappedTakeType
{
    return take(find(key));
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, shouldValidateKey, M>::take(iterator it) -> MappedTakeType
{
    if (it == end())
        return MappedTraits::take(MappedTraits::emptyValue());
    auto value = MappedTraits::take(WTFMove(it->value));
    remove(it);
    return value;
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, shouldValidateKey, M>::takeOptional(const KeyType& key) -> std::optional<MappedType>
{
    auto it = find(key);
    if (it == end())
        return std::nullopt;
    return take(it);
}

template<typename T, typename U, typename V, typename W, typename MappedTraits, typename Y, ShouldValidateKey shouldValidateKey, typename M>
auto HashMap<T, U, V, W, MappedTraits, Y, shouldValidateKey, M>::takeFirst() -> MappedTakeType
{
    return take(begin());
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) -> iterator
{
    return m_impl.template find<HashMapTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> const_iterator
{
    return m_impl.template find<HashMapTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> bool
{
    return m_impl.template contains<HashMapTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> MappedPeekType
{
    KeyValuePairType* entry = const_cast<HashTableType&>(m_impl).template inlineLookup<HashMapTranslator<KeyValuePairTraits, HashFunctions>, shouldValidateKey>(key);
    if (!entry)
        return MappedTraits::peek(MappedTraits::emptyValue());
    return MappedTraits::peek(entry->value);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) const -> MappedPeekType
{
    return inlineGet(key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) -> bool
{
    return remove(find(key));
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<SmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>* key) -> MappedTakeType
{
    iterator it = find(key);
    if (it == end())
        return MappedTraits::take(MappedTraits::emptyValue());
    auto value = MappedTraits::take(WTFMove(it->value));
    remove(it);
    return value;
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) -> iterator
{
    return find(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::find(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) const -> const_iterator
{
    return find(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::contains(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) const -> bool
{
    return contains(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::inlineGet(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) const -> MappedPeekType
{
    return inlineGet(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::get(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) const -> MappedPeekType
{
    return inlineGet(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::remove(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) -> bool
{
    return remove(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
template<NonNullableSmartPtr K>
inline auto HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::take(std::add_const_t<typename GetPtrHelper<K>::UnderlyingType>& key) -> MappedTakeType
{
    return take(&key);
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline void HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::checkConsistency() const
{
    m_impl.checkTableConsistency();
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
inline bool HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::isValidKey(const KeyType& key)
{
    if (KeyTraits::isDeletedValue(key))
        return false;

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
        if (key == KeyTraits::emptyValue())
            return false;
    } else {
        if (isHashTraitsEmptyValue<KeyTraits>(key))
            return false;
    }

    return true;
}

template<typename T, typename U, typename V, typename W, typename X, typename Y, ShouldValidateKey shouldValidateKey, typename M>
bool operator==(const HashMap<T, U, V, W, X, Y, shouldValidateKey, M>& a, const HashMap<T, U, V, W, X, Y, shouldValidateKey>& b)
{
    if (a.size() != b.size())
        return false;

    typedef typename HashMap<T, U, V, W, X, Y, shouldValidateKey, M>::const_iterator const_iterator;

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
