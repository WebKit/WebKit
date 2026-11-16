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
 *
 */

#pragma once

#include <initializer_list>
#include <wtf/Compiler.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashTable.h>
#include <wtf/HashTraits.h>
#include <wtf/RobinHoodHashTable.h>

namespace WTF {

template<typename ValueArg, typename HashArg, typename TraitsArg, typename TableTraitsArg>
class HashSet final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HashSet);
private:
    using HashFunctions = HashArg;
    using ValueTraits = TraitsArg;
    using TakeType = typename ValueTraits::TakeType;

public:
    using ValueType = typename ValueTraits::TraitType;

private:
    using HashTableType = typename TableTraitsArg::template TableType<ValueType, ValueType, IdentityExtractor, HashFunctions, ValueTraits, ValueTraits, FastMalloc>;

public:
    // HashSet iterators have the following structure:
    //      const ValueType* get() const;
    //      const ValueType& operator*() const;
    //      const ValueType* operator->() const;
    using iterator = HashTableConstIteratorAdapter<HashTableType, ValueType>;
    using const_iterator = HashTableConstIteratorAdapter<HashTableType, ValueType>;

    // HashSet AddResults have the following fields:
    //      IteratorType iterator;
    //      bool isNewEntry;
    using AddResult = typename HashTableType::AddResult;

    HashSet() = default;

    HashSet(std::initializer_list<ValueArg> initializerList)
    {
        if (!initializerList.size())
            return;

        reserveInitialCapacity(initializerList.size());
        for (auto&& value : initializerList)
            add(std::forward<decltype(value)>(value));
    }

    template<typename ContainerType>
    explicit HashSet(ContainerType&& container)
    {
        if (!container.size())
            return;

        reserveInitialCapacity(container.size());
        for (auto&& value : std::forward<ContainerType>(container))
            add(std::forward<decltype(value)>(value));
    }

    void swap(HashSet&);

    unsigned size() const;
    unsigned capacity() const;
    unsigned memoryUse() const;
    bool isEmpty() const;

    // Useful when the key type is WeakPtr
    size_t computeSize() const requires (ValueTraits::hasIsWeakNullValueFunction);
    size_t computeSize() requires (ValueTraits::hasIsWeakNullValueFunction);
    bool computeIsEmpty() const requires (ValueTraits::hasIsWeakNullValueFunction);
    bool computeIsEmpty() requires (ValueTraits::hasIsWeakNullValueFunction);
    void removeWeakNullEntries() requires (ValueTraits::hasIsWeakNullValueFunction);

    void reserveInitialCapacity(unsigned keyCount) { m_impl.reserveInitialCapacity(keyCount); }

    iterator begin() const LIFETIME_BOUND;
    iterator end() const LIFETIME_BOUND;

    iterator random() const LIFETIME_BOUND { return m_impl.random(); }

    iterator find(const ValueType&) const LIFETIME_BOUND;
    bool contains(const ValueType&) const;

    // An alternate version of find() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion. HashTranslator
    // must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator, typename T> iterator find(const T&) const LIFETIME_BOUND;
    template<typename HashTranslator, typename T> bool contains(const T&) const;

    ALWAYS_INLINE bool isNullStorage() const { return m_impl.isNullStorage(); }

    // The return value includes both an iterator to the added value's location,
    // and an isNewEntry bool that indicates if it is a new or existing entry in the set.
    AddResult add(const ValueType&) LIFETIME_BOUND;
    AddResult add(ValueType&&) LIFETIME_BOUND;
    void add(std::initializer_list<std::reference_wrapper<const ValueType>>);

    void addVoid(const ValueType&);
    void addVoid(ValueType&&);

    // An alternate version of add() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table. HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    //   static void translate(ValueType* uninitializedLocation, const T&, unsigned hashCode);
    template<typename HashTranslator, typename T> AddResult add(const T&) LIFETIME_BOUND;
    
    // An alternate version of translated add(), ensure() will still do translation
    // by hashing and comparing with some other type, to avoid the cost of type
    // conversion if the object is already in the table, but rather than a static
    // translate() function, uses the passed in functor to perform lazy creation of
    // the value only if it is not already there. HashTranslator must have the following
    // function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator> AddResult ensure(auto&&, NOESCAPE const Invocable<ValueType()> auto&) LIFETIME_BOUND;

    // Attempts to add a list of things to the set. Returns true if any of
    // them are new to the set. Returns false if the set is unchanged.
    template<typename ContainerType>
    bool addAll(ContainerType&&);
    template<typename ContainerType>
    bool removeAll(const ContainerType&);

    bool remove(const ValueType&);
    bool remove(iterator);
    bool removeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto&);
    void clear();

    TakeType take(const ValueType&);
    TakeType take(iterator);
    TakeType takeAny();

    template<size_t inlineCapacity = 0>
    Vector<TakeType, inlineCapacity> takeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto& functor) { return m_impl.template takeIf<inlineCapacity>(functor); }

    // Returns a new set with the elements of both this and the given
    // collection (a.k.a. OR).
    template<typename OtherCollection>
    HashSet unionWith(const OtherCollection&) const;

    // Returns a new set with the elements that are common to both this
    // set and the given collection (a.k.a. AND).
    //
    // NOTE: OtherCollection is required to implement `bool contains(Value)`.
    template<typename OtherCollection>
    HashSet intersectionWith(const OtherCollection&) const;

    // Returns a new set with the elements of this set that are not in
    // the given collection (a.k.a. A - B).
    //
    // NOTE: OtherCollection is required to implement `bool contains(Value)`.
    template<typename OtherCollection>
    HashSet differenceWith(const OtherCollection&) const;

    // Returns a new set with the elements that are either in this set or
    // in the given collection, but not in both. (a.k.a. XOR).
    template<typename OtherCollection>
    HashSet symmetricDifferenceWith(const OtherCollection&) const;

    // Removes the elements of this set that are in the given collection (a.k.a. A - B).
    //
    // NOTE: OtherCollection is required to implement `bool contains(Value)`.
    template<typename OtherCollection>
    void formDifference(const OtherCollection&);

    // Removes the elements of this set that aren't also in the given
    // collection (a.k.a. AND).
    //
    // NOTE: OtherCollection is required to implement `bool contains(Value)`.
    template<typename OtherCollection>
    void formIntersection(const OtherCollection&);

    // Removes the elements of the set that are also in the given collection
    // and adds the members of the given collection that are not already in
    // the set (a.k.a. XOR).
    template<typename OtherCollection>
    void formSymmetricDifference(const OtherCollection&);

    // Returns true if all the elements of this set are also in the given collection.
    template<typename OtherCollection>
    bool isSubset(const OtherCollection&);

    // Overloads for smart pointer values that take the raw pointer type as the parameter.
    template<SmartPtr V = ValueType> iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const LIFETIME_BOUND;
    template<SmartPtr V = ValueType> bool contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const;
    template<SmartPtr V = ValueType> bool remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*);
    template<SmartPtr V = ValueType> TakeType take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*);

    // Overloads for smart pointer values that take the raw reference type as the parameter.
    template<SmartPtr V = ValueType> iterator find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) const LIFETIME_BOUND { return find(&ref); }
    template<SmartPtr V = ValueType> bool contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) const { return contains(&ref); }
    template<SmartPtr V = ValueType> bool remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) { return remove(&ref); }
    template<SmartPtr V = ValueType> TakeType take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& ref) { return take(&ref); }

    static bool isValidValue(const ValueType& value) { return HashTableType::isValidKey(value); }

    template<typename OtherCollection>
    bool operator==(const OtherCollection&) const;

    void checkConsistency() const;

private:
    HashTableType m_impl;
};

struct IdentityExtractor {
    template<typename T> static const T& extract(const T& t) { return t; }
};

template<typename ValueTraits, typename HashFunctions>
struct HashSetTranslator {
    static unsigned hash(const auto& key) { return HashFunctions::hash(key); }
    static bool equal(const auto& a, const auto& b) { return HashFunctions::equal(a, b); }
    static void translate(typename ValueTraits::TraitType* location, auto&&, NOESCAPE const Invocable<typename ValueTraits::TraitType()> auto& functor)
    {
        std::construct_at(location, functor());
    }
};

template<typename Translator>
struct HashSetTranslatorAdapter {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(auto* location, const auto& key, const auto&, unsigned hashCode)
    {
        Translator::translate(location, key, hashCode);
    }
};

template<typename ValueTraits, typename Translator>
struct HashSetEnsureTranslatorAdaptor {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(typename ValueTraits::TraitType* location, auto&&, NOESCAPE const Invocable<typename ValueTraits::TraitType()> auto& functor)
    {
        std::construct_at(location, functor());
    }
};

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::swap(HashSet& other)
{
    m_impl.swap(other.m_impl); 
}

template<typename T, typename U, typename V, typename W>
inline unsigned HashSet<T, U, V, W>::size() const
{
    return m_impl.size(); 
}

template<typename T, typename U, typename V, typename W>
inline unsigned HashSet<T, U, V, W>::capacity() const
{
    return m_impl.capacity(); 
}

template<typename T, typename U, typename V, typename W>
inline unsigned HashSet<T, U, V, W>::memoryUse() const
{
    return capacity() * sizeof(T);
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::isEmpty() const
{
    return m_impl.isEmpty(); 
}

template<typename T, typename U, typename V, typename W>
inline size_t HashSet<T, U, V, W>::computeSize() const requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeSize();
}

template<typename T, typename U, typename V, typename W>
inline size_t HashSet<T, U, V, W>::computeSize() requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeSize();
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::computeIsEmpty() const requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeIsEmpty();
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::computeIsEmpty() requires (ValueTraits::hasIsWeakNullValueFunction)
{
    return m_impl.computeIsEmpty();
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::removeWeakNullEntries() requires (ValueTraits::hasIsWeakNullValueFunction)
{
    m_impl.removeWeakNullEntries();
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::begin() const LIFETIME_BOUND -> iterator
{
    return m_impl.begin();
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::end() const LIFETIME_BOUND -> iterator
{
    return m_impl.end();
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::find(const ValueType& value) const LIFETIME_BOUND -> iterator
{
    return m_impl.find(value);
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::contains(const ValueType& value) const
{
    return m_impl.contains(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename HashTranslator, typename T>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::find(const T& value) const LIFETIME_BOUND -> iterator
{
    return m_impl.template find<HashSetTranslatorAdapter<HashTranslator>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename HashTranslator, typename T>
inline bool HashSet<Value, HashFunctions, Traits, TableTraits>::contains(const T& value) const
{
    return m_impl.template contains<HashSetTranslatorAdapter<HashTranslator>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename HashTranslator, typename T>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::ensure(T&& key, NOESCAPE const Invocable<ValueType()> auto& functor) LIFETIME_BOUND -> AddResult
{
    return m_impl.template add<HashSetEnsureTranslatorAdaptor<Traits, HashTranslator>>(std::forward<T>(key), functor);
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::add(const ValueType& value) LIFETIME_BOUND -> AddResult
{
    return m_impl.add(value);
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::add(ValueType&& value) LIFETIME_BOUND -> AddResult
{
    return m_impl.add(WTF::move(value));
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::addVoid(const ValueType& value)
{
    m_impl.add(value);
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::addVoid(ValueType&& value)
{
    m_impl.add(WTF::move(value));
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename HashTranslator>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::add(const auto& value) LIFETIME_BOUND -> AddResult
{
    return m_impl.template addPassingHashCode<HashSetTranslatorAdapter<HashTranslator>>(value, [&] ALWAYS_INLINE_LAMBDA { return value; });
}

template<typename T, typename U, typename V, typename W>
template<typename ContainerType>
inline bool HashSet<T, U, V, W>::addAll(ContainerType&& container)
{
    bool changed = false;
    for (auto&& item : std::forward<ContainerType>(container))
        changed |= add(std::forward<decltype(item)>(item)).isNewEntry;
    return changed;
}

template<typename T, typename U, typename V, typename W>
template<typename ContainerType>
inline bool HashSet<T, U, V, W>::removeAll(const ContainerType& container)
{
    bool changed = false;
    for (auto& item : container)
        changed |= remove(item);
    return changed;
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::remove(iterator it)
{
    if (it.m_impl == m_impl.end())
        return false;
    m_impl.internalCheckTableConsistency();
    m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    return true;
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::remove(const ValueType& value)
{
    return remove(find(value));
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::removeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto& functor)
{
    return m_impl.removeIf(functor);
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::clear()
{
    m_impl.clear(); 
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::take(iterator it) -> TakeType
{
    if (it == end())
        return ValueTraits::take(ValueTraits::emptyValue());

    m_impl.internalCheckTableConsistency();
    auto result = ValueTraits::take(WTF::move(const_cast<ValueType&>(*it)));
    m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    return result;
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::take(const ValueType& value) -> TakeType
{
    return take(find(value));
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::takeAny() -> TakeType
{
    return take(begin());
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline auto HashSet<T, U, V, W>::unionWith(const OtherCollection& other) const -> HashSet<T, U, V, W>
{
    auto copy = *this;
    copy.addAll(other);
    return copy;
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline auto HashSet<T, U, V, W>::intersectionWith(const OtherCollection& other) const -> HashSet<T, U, V, W>
{
    HashSet result;
    for (auto& value : *this) {
        if (other.contains(value))
            result.addVoid(value);
    }
    return result;
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline auto HashSet<T, U, V, W>::differenceWith(const OtherCollection& other) const -> HashSet<T, U, V, W>
{
    HashSet result;
    for (const auto& value : *this) {
        if (!other.contains(value))
            result.add(value);
    }
    return result;
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline auto HashSet<T, U, V, W>::symmetricDifferenceWith(const OtherCollection& other) const -> HashSet<T, U, V, W>
{
    auto copy = *this;
    copy.formSymmetricDifference(other);
    return copy;
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline void HashSet<T, U, V, W>::formIntersection(const OtherCollection& other)
{
    *this = intersectionWith(other);
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline void HashSet<T, U, V, W>::formDifference(const OtherCollection& other)
{
    *this = differenceWith(other);
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline void HashSet<T, U, V, W>::formSymmetricDifference(const OtherCollection& other)
{
    for (auto& value : other) {
        if (!remove(value))
            addVoid(value);
    }
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline bool HashSet<T, U, V, W>::isSubset(const OtherCollection& other)
{
    return intersectionWith(other).size() == size();
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<SmartPtr V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const LIFETIME_BOUND -> iterator
{
    return m_impl.template find<HashSetTranslator<Traits, HashFunctions>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<SmartPtr V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const -> bool
{
    return m_impl.template contains<HashSetTranslator<Traits, HashFunctions>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<SmartPtr V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) -> bool
{
    return remove(find(value));
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<SmartPtr V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) -> TakeType
{
    return take(find(value));
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline bool HashSet<T, U, V, W>::operator==(const OtherCollection& otherCollection) const
{
    if (size() != otherCollection.size())
        return false;
    for (const auto& other : otherCollection) {
        if (!contains(other))
            return false;
    }
    return true;
}

template<typename T, typename U, typename V, typename W>
void HashSet<T, U, V, W>::add(std::initializer_list<std::reference_wrapper<const ValueType>> list)
{
    for (auto& value : list)
        add(value);
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::checkConsistency() const
{
    m_impl.checkTableConsistency();
}

} // namespace WTF

using WTF::HashSet;
