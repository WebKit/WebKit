/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WTF_HashTraits_h
#define WTF_HashTraits_h

#include <wtf/HashFunctions.h>
#include <wtf/StdLibExtras.h>
#include <utility>
#include <limits>

namespace WTF {

class String;

template<typename T> class OwnPtr;
template<typename T> class PassOwnPtr;

template<typename T> struct HashTraits;

template<bool isInteger, typename T> struct GenericHashTraitsBase;

template<typename T> struct GenericHashTraitsBase<false, T> {
    // The emptyValueIsZero flag is used to optimize allocation of empty hash tables with zeroed memory.
    static const bool emptyValueIsZero = false;
    
    // The hasIsEmptyValueFunction flag allows the hash table to automatically generate code to check
    // for the empty value when it can be done with the equality operator, but allows custom functions
    // for cases like String that need them.
    static const bool hasIsEmptyValueFunction = false;

    // The starting table size. Can be overridden when we know beforehand that
    // a hash table will have at least N entries.
    static const int minimumTableSize = 8;
};

// Default integer traits disallow both 0 and -1 as keys (max value instead of -1 for unsigned).
template<typename T> struct GenericHashTraitsBase<true, T> : GenericHashTraitsBase<false, T> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(T& slot) { slot = static_cast<T>(-1); }
    static bool isDeletedValue(T value) { return value == static_cast<T>(-1); }
};

template<typename T> struct GenericHashTraits : GenericHashTraitsBase<std::is_integral<T>::value, T> {
    typedef T TraitType;
    typedef T EmptyValueType;

    static T emptyValue() { return T(); }

    // Type for return value of functions that do not transfer ownership, such as get.
    typedef T PeekType;
    template<typename U> static U&& peek(U&& value) { return std::forward<U>(value); }
};

template<typename T> struct HashTraits : GenericHashTraits<T> { };

template<typename T> struct FloatHashTraits : GenericHashTraits<T> {
    static T emptyValue() { return std::numeric_limits<T>::infinity(); }
    static void constructDeletedValue(T& slot) { slot = -std::numeric_limits<T>::infinity(); }
    static bool isDeletedValue(T value) { return value == -std::numeric_limits<T>::infinity(); }
};

template<> struct HashTraits<float> : FloatHashTraits<float> { };
template<> struct HashTraits<double> : FloatHashTraits<double> { };

// Default unsigned traits disallow both 0 and max as keys -- use these traits to allow zero and disallow max - 1.
template<typename T> struct UnsignedWithZeroKeyHashTraits : GenericHashTraits<T> {
    static const bool emptyValueIsZero = false;
    static T emptyValue() { return std::numeric_limits<T>::max(); }
    static void constructDeletedValue(T& slot) { slot = std::numeric_limits<T>::max() - 1; }
    static bool isDeletedValue(T value) { return value == std::numeric_limits<T>::max() - 1; }
};

// Can be used with strong enums, allows zero as key.
template<typename T> struct StrongEnumHashTraits : GenericHashTraits<T> {
    using UnderlyingType = typename std::underlying_type<T>::type;
    static const bool emptyValueIsZero = false;
    static T emptyValue() { return static_cast<T>(std::numeric_limits<UnderlyingType>::max()); }
    static void constructDeletedValue(T& slot) { slot = static_cast<T>(std::numeric_limits<UnderlyingType>::max() - 1); }
    static bool isDeletedValue(T value) { return value == static_cast<T>(std::numeric_limits<UnderlyingType>::max() - 1); }
};

template<typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(P*& slot) { slot = reinterpret_cast<P*>(-1); }
    static bool isDeletedValue(P* value) { return value == reinterpret_cast<P*>(-1); }
};

template<typename T> struct SimpleClassHashTraits : GenericHashTraits<T> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(T& slot) { new (NotNull, &slot) T(HashTableDeletedValue); }
    static bool isDeletedValue(const T& value) { return value.isHashTableDeletedValue(); }
};

template<typename T, typename Deleter> struct HashTraits<std::unique_ptr<T, Deleter>> : SimpleClassHashTraits<std::unique_ptr<T, Deleter>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    typedef T* PeekType;
    static T* peek(const std::unique_ptr<T, Deleter>& value) { return value.get(); }
    static T* peek(std::nullptr_t) { return nullptr; }
};

template<typename T> struct HashTraits<OwnPtr<T>> : SimpleClassHashTraits<OwnPtr<T>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    typedef T* PeekType;
    static T* peek(const OwnPtr<T>& value) { return value.get(); }
    static T* peek(std::nullptr_t) { return nullptr; }
};

template<typename P> struct HashTraits<RefPtr<P>> : SimpleClassHashTraits<RefPtr<P>> {
    static P* emptyValue() { return 0; }

    typedef P* PeekType;
    static PeekType peek(const RefPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }
};

template<> struct HashTraits<String> : SimpleClassHashTraits<String> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const String&);
};

// This struct template is an implementation detail of the isHashTraitsEmptyValue function,
// which selects either the emptyValue function or the isEmptyValue function to check for empty values.
template<typename Traits, bool hasEmptyValueFunction> struct HashTraitsEmptyValueChecker;
template<typename Traits> struct HashTraitsEmptyValueChecker<Traits, true> {
    template<typename T> static bool isEmptyValue(const T& value) { return Traits::isEmptyValue(value); }
};
template<typename Traits> struct HashTraitsEmptyValueChecker<Traits, false> {
    template<typename T> static bool isEmptyValue(const T& value) { return value == Traits::emptyValue(); }
};
template<typename Traits, typename T> inline bool isHashTraitsEmptyValue(const T& value)
{
    return HashTraitsEmptyValueChecker<Traits, Traits::hasIsEmptyValueFunction>::isEmptyValue(value);
}

template<typename FirstTraitsArg, typename SecondTraitsArg>
struct PairHashTraits : GenericHashTraits<std::pair<typename FirstTraitsArg::TraitType, typename SecondTraitsArg::TraitType>> {
    typedef FirstTraitsArg FirstTraits;
    typedef SecondTraitsArg SecondTraits;
    typedef std::pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;
    typedef std::pair<typename FirstTraits::EmptyValueType, typename SecondTraits::EmptyValueType> EmptyValueType;

    static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return std::make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

    static const int minimumTableSize = FirstTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot) { FirstTraits::constructDeletedValue(slot.first); }
    static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
};

template<typename First, typename Second>
struct HashTraits<std::pair<First, Second>> : public PairHashTraits<HashTraits<First>, HashTraits<Second>> { };

template<typename KeyTypeArg, typename ValueTypeArg>
struct KeyValuePair {
    typedef KeyTypeArg KeyType;

    KeyValuePair()
    {
    }

    template<typename K, typename V>
    KeyValuePair(K&& key, V&& value)
        : key(std::forward<K>(key))
        , value(std::forward<V>(value))
    {
    }

    template <typename OtherKeyType, typename OtherValueType>
    KeyValuePair(KeyValuePair<OtherKeyType, OtherValueType>&& other)
        : key(std::forward<OtherKeyType>(other.key))
        , value(std::forward<OtherValueType>(other.value))
    {
    }

    KeyTypeArg key;
    ValueTypeArg value;
};

template<typename KeyTraitsArg, typename ValueTraitsArg>
struct KeyValuePairHashTraits : GenericHashTraits<KeyValuePair<typename KeyTraitsArg::TraitType, typename ValueTraitsArg::TraitType>> {
    typedef KeyTraitsArg KeyTraits;
    typedef ValueTraitsArg ValueTraits;
    typedef KeyValuePair<typename KeyTraits::TraitType, typename ValueTraits::TraitType> TraitType;
    typedef KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType> EmptyValueType;

    static const bool emptyValueIsZero = KeyTraits::emptyValueIsZero && ValueTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType>(KeyTraits::emptyValue(), ValueTraits::emptyValue()); }

    static const int minimumTableSize = KeyTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot) { KeyTraits::constructDeletedValue(slot.key); }
    static bool isDeletedValue(const TraitType& value) { return KeyTraits::isDeletedValue(value.key); }
};

template<typename Key, typename Value>
struct HashTraits<KeyValuePair<Key, Value>> : public KeyValuePairHashTraits<HashTraits<Key>, HashTraits<Value>> { };

template<typename T>
struct NullableHashTraits : public HashTraits<T> {
    static const bool emptyValueIsZero = false;
    static T emptyValue() { return reinterpret_cast<T>(1); }
};

// Useful for classes that want complete control over what is empty and what is deleted,
// and how to construct both.
template<typename T>
struct CustomHashTraits : public GenericHashTraits<T> {
    static const bool emptyValueIsZero = false;
    static const bool hasIsEmptyValueFunction = true;
    
    static void constructDeletedValue(T& slot)
    {
        new (NotNull, &slot) T(T::DeletedValue);
    }
    
    static bool isDeletedValue(const T& value)
    {
        return value.isDeletedValue();
    }
    
    static T emptyValue()
    {
        return T(T::EmptyValue);
    }
    
    static bool isEmptyValue(const T& value)
    {
        return value.isEmptyValue();
    }
};

} // namespace WTF

using WTF::HashTraits;
using WTF::PairHashTraits;
using WTF::NullableHashTraits;
using WTF::SimpleClassHashTraits;

#endif // WTF_HashTraits_h
