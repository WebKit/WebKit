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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <initializer_list>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashTable.h>

namespace WTF {

template<typename ValueArg, typename HashArg, typename TraitsArg, typename TableTraitsArg>
class HashSet final {
    WTF_MAKE_FAST_ALLOCATED;
private:
    using HashFunctions = HashArg;
    using ValueTraits = TraitsArg;
    using TakeType = typename ValueTraits::TakeType;

public:
    using ValueType = typename ValueTraits::TraitType;

private:
    using HashTableType = typename TableTraitsArg::template TableType<ValueType, ValueType, IdentityExtractor, HashFunctions, ValueTraits, ValueTraits>;

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
        for (const auto& value : initializerList)
            add(value);
    }

    void swap(HashSet&);

    unsigned size() const;
    unsigned capacity() const;
    unsigned memoryUse() const;
    bool isEmpty() const;

    void reserveInitialCapacity(unsigned keyCount) { m_impl.reserveInitialCapacity(keyCount); }

    iterator begin() const;
    iterator end() const;

    iterator random() const { return m_impl.random(); }

    iterator find(const ValueType&) const;
    bool contains(const ValueType&) const;

    // An alternate version of find() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion. HashTranslator
    // must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator, typename T> iterator find(const T&) const;
    template<typename HashTranslator, typename T> bool contains(const T&) const;

    ALWAYS_INLINE bool isNullStorage() const { return m_impl.isNullStorage(); }

    // The return value includes both an iterator to the added value's location,
    // and an isNewEntry bool that indicates if it is a new or existing entry in the set.
    AddResult add(const ValueType&);
    AddResult add(ValueType&&);
    void add(std::initializer_list<std::reference_wrapper<const ValueType>>);

    void addVoid(const ValueType&);
    void addVoid(ValueType&&);

    // An alternate version of add() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table. HashTranslator must have the following function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    //   static translate(ValueType&, const T&, unsigned hashCode);
    template<typename HashTranslator, typename T> AddResult add(const T&);
    
    // An alternate version of translated add(), ensure() will still do translation
    // by hashing and comparing with some other type, to avoid the cost of type
    // conversion if the object is already in the table, but rather than a static
    // translate() function, uses the passed in functor to perform lazy creation of
    // the value only if it is not already there. HashTranslator must have the following
    // function members:
    //   static unsigned hash(const T&);
    //   static bool equal(const ValueType&, const T&);
    template<typename HashTranslator> AddResult ensure(auto&&, const Invocable<ValueType()> auto&);

    // Attempts to add a list of things to the set. Returns true if any of
    // them are new to the set. Returns false if the set is unchanged.
    template<typename IteratorType>
    bool add(IteratorType begin, IteratorType end);
    template<typename IteratorType>
    bool remove(IteratorType begin, IteratorType end);

    bool remove(const ValueType&);
    bool remove(iterator);
    bool removeIf(const Invocable<bool(const ValueType&)> auto&);
    void clear();

    TakeType take(const ValueType&);
    TakeType take(iterator);
    TakeType takeAny();

    template<size_t inlineCapacity = 0>
    Vector<TakeType, inlineCapacity> takeIf(const Invocable<bool(const ValueType&)> auto& functor) { return m_impl.template takeIf<inlineCapacity>(functor); }

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

    // Returns a new set with the elements that are either in this set or
    // in the given collection, but not in both. (a.k.a. XOR).
    template<typename OtherCollection>
    HashSet symmetricDifferenceWith(const OtherCollection&) const;

    // Adds the elements of the given collection to the set (a.k.a. OR).
    template<typename OtherCollection>
    void formUnion(const OtherCollection&);

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
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value, iterator>::type find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const;
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value, bool>::type contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*) const;
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value, bool>::type remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*);
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value, TakeType>::type take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>*);

    // Overloads for non-nullable smart pointer values that take the raw reference type as the parameter.
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, iterator>::type find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>&) const;
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, bool>::type contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>&) const;
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, bool>::type remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>&);
    template<typename V = ValueType> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, TakeType>::type take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>&);

    static bool isValidValue(const ValueType&);

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
    static void translate(auto& location, auto&&, const Invocable<typename ValueTraits::TraitType()> auto& functor)
    { 
        ValueTraits::assignToEmpty(location, functor());
    }
};

template<typename Translator>
struct HashSetTranslatorAdapter {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(auto& location, const auto& key, const auto&, unsigned hashCode)
    {
        Translator::translate(location, key, hashCode);
    }
};

template<typename ValueTraits, typename Translator>
struct HashSetEnsureTranslatorAdaptor {
    static unsigned hash(const auto& key) { return Translator::hash(key); }
    static bool equal(const auto& a, const auto& b) { return Translator::equal(a, b); }
    static void translate(auto& location, auto&&, const Invocable<typename ValueTraits::TraitType()> auto& functor)
    {
        ValueTraits::assignToEmpty(location, functor());
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
inline auto HashSet<T, U, V, W>::begin() const -> iterator
{
    return m_impl.begin(); 
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::end() const -> iterator
{
    return m_impl.end(); 
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::find(const ValueType& value) const -> iterator
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
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::find(const T& value) const -> iterator
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
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::ensure(T&& key, const Invocable<ValueType()> auto& functor) -> AddResult
{
    return m_impl.template add<HashSetEnsureTranslatorAdaptor<Traits, HashTranslator>>(std::forward<T>(key), functor);
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::add(const ValueType& value) -> AddResult
{
    return m_impl.add(value);
}

template<typename T, typename U, typename V, typename W>
inline auto HashSet<T, U, V, W>::add(ValueType&& value) -> AddResult
{
    return m_impl.add(WTFMove(value));
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::addVoid(const ValueType& value)
{
    m_impl.add(value);
}

template<typename T, typename U, typename V, typename W>
inline void HashSet<T, U, V, W>::addVoid(ValueType&& value)
{
    m_impl.add(WTFMove(value));
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename HashTranslator>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::add(const auto& value) -> AddResult
{
    return m_impl.template addPassingHashCode<HashSetTranslatorAdapter<HashTranslator>>(value, [&]() ALWAYS_INLINE_LAMBDA { return value; });
}

template<typename T, typename U, typename V, typename W>
template<typename IteratorType>
inline bool HashSet<T, U, V, W>::add(IteratorType begin, IteratorType end)
{
    bool changed = false;
    for (IteratorType iter = begin; iter != end; ++iter)
        changed |= add(*iter).isNewEntry;
    return changed;
}

template<typename T, typename U, typename V, typename W>
template<typename IteratorType>
inline bool HashSet<T, U, V, W>::remove(IteratorType begin, IteratorType end)
{
    bool changed = false;
    for (IteratorType iter = begin; iter != end; ++iter)
        changed |= remove(*iter);
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
inline bool HashSet<T, U, V, W>::removeIf(const Invocable<bool(const ValueType&)> auto& functor)
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

    auto result = ValueTraits::take(WTFMove(const_cast<ValueType&>(*it)));
    remove(it);
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
    copy.add(other.begin(), other.end());
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
inline auto HashSet<T, U, V, W>::symmetricDifferenceWith(const OtherCollection& other) const -> HashSet<T, U, V, W>
{
    auto copy = *this;
    copy.formSymmetricDifference(other);
    return copy;
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline void HashSet<T, U, V, W>::formUnion(const OtherCollection& other)
{
    add(other.begin(), other.end());
}

template<typename T, typename U, typename V, typename W>
template<typename OtherCollection>
inline void HashSet<T, U, V, W>::formIntersection(const OtherCollection& other)
{
    *this = intersectionWith(other);
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
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const -> typename std::enable_if<IsSmartPtr<V>::value, iterator>::type
{
    return m_impl.template find<HashSetTranslator<Traits, HashFunctions>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) const -> typename std::enable_if<IsSmartPtr<V>::value, bool>::type
{
    return m_impl.template contains<HashSetTranslator<Traits, HashFunctions>>(value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) -> typename std::enable_if<IsSmartPtr<V>::value, bool>::type
{
    return remove(find(value));
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>* value) -> typename std::enable_if<IsSmartPtr<V>::value, TakeType>::type
{
    return take(find(value));
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::find(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& value) const -> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, iterator>::type
{
    return find(&value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::contains(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& value) const -> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, bool>::type
{
    return contains(&value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::remove(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& value) -> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, bool>::type
{
    return remove(&value);
}

template<typename Value, typename HashFunctions, typename Traits, typename TableTraits>
template<typename V>
inline auto HashSet<Value, HashFunctions, Traits, TableTraits>::take(std::add_const_t<typename GetPtrHelper<V>::UnderlyingType>& value) -> typename std::enable_if<IsSmartPtr<V>::value && !IsSmartPtr<V>::isNullable, TakeType>::type
{
    return take(&value);
}

template<typename T, typename U, typename V, typename W>
inline bool HashSet<T, U, V, W>::isValidValue(const ValueType& value)
{
    if (ValueTraits::isDeletedValue(value))
        return false;

    if (HashFunctions::safeToCompareToEmptyOrDeleted) {
        if (value == ValueTraits::emptyValue())
            return false;
    } else {
        if (isHashTraitsEmptyValue<ValueTraits>(value))
            return false;
    }

    return true;
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
