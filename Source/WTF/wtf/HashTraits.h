/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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

#include <limits>
#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/KeyValuePair.h>
#include <wtf/StdLibExtras.h>

#ifdef __OBJC__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WTF {

template<bool isInteger, typename T> struct GenericHashTraitsBase;

template<typename T> struct GenericHashTraitsBase<false, T> {
    // The emptyValueIsZero flag is used to optimize allocation of empty hash tables with zeroed memory.
    static constexpr bool emptyValueIsZero = false;

    // The hasIsEmptyValueFunction flag allows the hash table to automatically generate code to check
    // for the empty value when it can be done with the equality operator, but allows custom functions
    // for cases like String that need them.
    static constexpr bool hasIsEmptyValueFunction = false;

    // Used by WeakPtr to indicate that the value may become deleted without being explicitly removed.
    static constexpr bool hasIsReleasedWeakValueFunction = false;

    // The starting table size. Can be overridden when we know beforehand that
    // a hash table will have at least N entries.
    static constexpr unsigned minimumTableSize = 8;
};

// Default integer traits disallow both 0 and -1 as keys (max value instead of -1 for unsigned).
template<typename T> struct GenericHashTraitsBase<true, T> : GenericHashTraitsBase<false, T> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(T& slot) { slot = static_cast<T>(-1); }
    static bool isDeletedValue(T value) { return value == static_cast<T>(-1); }
};

template<typename T> struct GenericHashTraits : GenericHashTraitsBase<std::is_integral<T>::value, T> {
    typedef T TraitType;
    typedef T EmptyValueType;

    static T emptyValue() { return T(); }

    template<typename U, typename V> 
    static void assignToEmpty(U& emptyValue, V&& value)
    { 
        emptyValue = std::forward<V>(value);
    }

    template <typename Traits>
    static void constructEmptyValue(T& slot)
    {
        new (NotNull, std::addressof(slot)) T(Traits::emptyValue());
    }

    // Type for return value of functions that do not transfer ownership, such as get.
    typedef T PeekType;
    template<typename U> static U&& peek(U&& value) { return std::forward<U>(value); }

    typedef T TakeType;
    template<typename U> static TakeType take(U&& value) { return std::forward<U>(value); }
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
    static constexpr bool emptyValueIsZero = false;
    static T emptyValue() { return std::numeric_limits<T>::max(); }
    static void constructDeletedValue(T& slot) { slot = std::numeric_limits<T>::max() - 1; }
    static bool isDeletedValue(T value) { return value == std::numeric_limits<T>::max() - 1; }
};

template<typename T> struct SignedWithZeroKeyHashTraits : GenericHashTraits<T> {
    static constexpr bool emptyValueIsZero = false;
    static T emptyValue() { return std::numeric_limits<T>::min(); }
    static void constructDeletedValue(T& slot) { slot = std::numeric_limits<T>::max(); }
    static bool isDeletedValue(T value) { return value == std::numeric_limits<T>::max(); }
};

// Can be used with strong enums, allows zero as key.
template<typename T> struct StrongEnumHashTraits : GenericHashTraits<T> {
    using UnderlyingType = typename std::underlying_type<T>::type;
    static constexpr bool emptyValueIsZero = false;
    static T emptyValue() { return static_cast<T>(std::numeric_limits<UnderlyingType>::max()); }
    static void constructDeletedValue(T& slot) { slot = static_cast<T>(std::numeric_limits<UnderlyingType>::max() - 1); }
    static bool isDeletedValue(T value) { return value == static_cast<T>(std::numeric_limits<UnderlyingType>::max() - 1); }
};

template<typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(P*& slot) { slot = reinterpret_cast<P*>(-1); }
    static bool isDeletedValue(P* value) { return value == reinterpret_cast<P*>(-1); }
};

#ifdef __OBJC__

template<> struct HashTraits<__unsafe_unretained id> : GenericHashTraits<__unsafe_unretained id> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(__unsafe_unretained id& slot) { slot = (__bridge __unsafe_unretained id)reinterpret_cast<CFTypeRef>(-1); }
    static bool isDeletedValue(__unsafe_unretained id value) { return (__bridge CFTypeRef)value == reinterpret_cast<CFTypeRef>(-1); }
};

#endif

template<typename T> struct SimpleClassHashTraits : GenericHashTraits<T> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(T& slot) { new (NotNull, std::addressof(slot)) T(HashTableDeletedValue); }
    static bool isDeletedValue(const T& value) { return value.isHashTableDeletedValue(); }
};

template<typename T, typename Deleter> struct HashTraits<std::unique_ptr<T, Deleter>> : SimpleClassHashTraits<std::unique_ptr<T, Deleter>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    static void constructDeletedValue(std::unique_ptr<T, Deleter>& slot) { new (NotNull, std::addressof(slot)) std::unique_ptr<T, Deleter> { reinterpret_cast<T*>(-1) }; }
    static bool isDeletedValue(const std::unique_ptr<T, Deleter>& value) { return value.get() == reinterpret_cast<T*>(-1); }

    typedef T* PeekType;
    static T* peek(const std::unique_ptr<T, Deleter>& value) { return value.get(); }
    static T* peek(std::nullptr_t) { return nullptr; }

    static void customDeleteBucket(std::unique_ptr<T, Deleter>& value)
    {
        // The custom delete function exists to avoid a dead store before the value is destructed.
        // The normal destruction sequence of a bucket would be:
        // 1) Call the destructor of unique_ptr.
        // 2) unique_ptr store a zero for its internal pointer.
        // 3) unique_ptr destroys its value.
        // 4) Call constructDeletedValue() to set the bucket as destructed.
        //
        // The problem is the call in (3) prevents the compile from eliminating the dead store in (2)
        // becase a side effect of free() could be observing the value.
        //
        // This version of deleteBucket() ensures the dead 2 stores changing "value"
        // are on the same side of the function call.
        ASSERT(!isDeletedValue(value));
        T* pointer = value.release();
        constructDeletedValue(value);

        // The null case happens if a caller uses std::move() to remove the pointer before calling remove()
        // with an iterator. This is very uncommon.
        if (LIKELY(pointer))
            Deleter()(pointer);
    }
};

template<typename T> struct HashTraits<UniqueRef<T>> : SimpleClassHashTraits<UniqueRef<T>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    template <typename>
    static void constructEmptyValue(UniqueRef<T>& slot)
    {
        new (NotNull, std::addressof(slot)) UniqueRef<T>(HashTableEmptyValue);
    }

    static void constructDeletedValue(UniqueRef<T>& slot) { new (NotNull, std::addressof(slot)) UniqueRef<T> { reinterpret_cast<T*>(-1) }; }
    static bool isDeletedValue(const UniqueRef<T>& value) { return value.get() == reinterpret_cast<T*>(-1); }

    typedef T* PeekType;
    static const T* peek(const UniqueRef<T>& value) { return &value.get(); }
    static T* peek(UniqueRef<T>& value) { return &value.get(); }
    static T* peek(std::nullptr_t) { return nullptr; }

    using TakeType = std::unique_ptr<T>;
    static TakeType take(UniqueRef<T>&& value) { return value.moveToUniquePtr(); }
    static TakeType take(std::nullptr_t) { return nullptr; }
};

template<typename P> struct HashTraits<RefPtr<P>> : SimpleClassHashTraits<RefPtr<P>> {
    static P* emptyValue() { return nullptr; }

    typedef P* PeekType;
    static PeekType peek(const RefPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }

    static void customDeleteBucket(RefPtr<P>& value)
    {
        // See unique_ptr's customDeleteBucket() for an explanation.
        ASSERT(!SimpleClassHashTraits<RefPtr<P>>::isDeletedValue(value));
        auto valueToBeDestroyed = WTFMove(value);
        SimpleClassHashTraits<RefPtr<P>>::constructDeletedValue(value);
    }
};

template<typename P> struct RefHashTraits : SimpleClassHashTraits<Ref<P>> {
    static constexpr bool emptyValueIsZero = true;
    static Ref<P> emptyValue() { return HashTableEmptyValue; }

    template <typename>
    static void constructEmptyValue(Ref<P>& slot)
    {
        new (NotNull, std::addressof(slot)) Ref<P>(HashTableEmptyValue);
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const Ref<P>& value) { return value.isHashTableEmptyValue(); }

    using PeekType = P*;
    static PeekType peek(const Ref<P>& value) { return const_cast<PeekType>(value.ptrAllowingHashTableEmptyValue()); }
    static PeekType peek(P* value) { return value; }

    using TakeType = RefPtr<P>;
    static TakeType take(Ref<P>&& value) { return isEmptyValue(value) ? nullptr : RefPtr<P>(WTFMove(value)); }
};

template<typename P> struct HashTraits<Ref<P>> : RefHashTraits<P> { };

template<typename P> struct HashTraits<Packed<P*>> : SimpleClassHashTraits<Packed<P*>> {
    static constexpr bool hasIsEmptyValueFunction = true;
    using TargetType = Packed<P*>;
    static_assert(TargetType::alignment < 4 * KB, "The first page is always unmapped since it includes nullptr.");

    static Packed<P*> emptyValue() { return nullptr; }
    static bool isEmptyValue(const TargetType& value) { return value.get() == nullptr; }

    using PeekType = Packed<P*>;
    static PeekType peek(const TargetType& value) { return value; }
    static PeekType peek(P* value) { return value; }
};

template<typename P> struct HashTraits<CompactPtr<P>> : SimpleClassHashTraits<CompactPtr<P>> {
    static constexpr bool hasIsEmptyValueFunction = true;
    using TargetType = CompactPtr<P>;

    static CompactPtr<P> emptyValue() { return nullptr; }
    static bool isEmptyValue(const TargetType& value) { return !value; }

    using PeekType = CompactPtr<P>;
    static PeekType peek(const TargetType& value) { return value; }
    static PeekType peek(P* value) { return value; }
};

template<> struct HashTraits<String> : SimpleClassHashTraits<String> {
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const String&);

    static void customDeleteBucket(String&);
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

template<typename Traits, bool hasIsReleasedWeakValueFunction> struct HashTraitsReleasedWeakValueChecker;
template<typename Traits> struct HashTraitsReleasedWeakValueChecker<Traits, true> {
    template<typename T> static bool isReleasedWeakValue(const T& value) { return Traits::isReleasedWeakValue(value); }
};
template<typename Traits> struct HashTraitsReleasedWeakValueChecker<Traits, false> {
    template<typename T> static bool isReleasedWeakValue(const T&) { return false; }
};
template<typename Traits, typename T> inline bool isHashTraitsReleasedWeakValue(const T& value)
{
    return HashTraitsReleasedWeakValueChecker<Traits, Traits::hasIsReleasedWeakValueFunction>::isReleasedWeakValue(value);
}

template<typename Traits, typename T>
struct HashTraitHasCustomDelete {
    static T& bucketArg;
    template<typename X> static std::true_type TestHasCustomDelete(X*, decltype(X::customDeleteBucket(bucketArg))* = nullptr);
    static std::false_type TestHasCustomDelete(...);
    typedef decltype(TestHasCustomDelete(static_cast<Traits*>(nullptr))) ResultType;
    static constexpr bool value = ResultType::value;
};

template<typename Traits, typename T>
void hashTraitsDeleteBucket(T& value)
{
    if constexpr (HashTraitHasCustomDelete<Traits, T>::value)
        Traits::customDeleteBucket(value);
    else {
        value.~T();
        Traits::constructDeletedValue(value);
    }
}

template<typename FirstTraitsArg, typename SecondTraitsArg>
struct PairHashTraits : GenericHashTraits<std::pair<typename FirstTraitsArg::TraitType, typename SecondTraitsArg::TraitType>> {
    typedef FirstTraitsArg FirstTraits;
    typedef SecondTraitsArg SecondTraits;
    typedef std::pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;
    typedef std::pair<typename FirstTraits::EmptyValueType, typename SecondTraits::EmptyValueType> EmptyValueType;

    static constexpr bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return std::make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

    static constexpr unsigned minimumTableSize = FirstTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot) { FirstTraits::constructDeletedValue(slot.first); }
    static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
};

template<typename First, typename Second>
struct HashTraits<std::pair<First, Second>> : public PairHashTraits<HashTraits<First>, HashTraits<Second>> { };

template<typename FirstTrait, typename... Traits>
struct TupleHashTraits : GenericHashTraits<std::tuple<typename FirstTrait::TraitType, typename Traits::TraitType...>> {
    typedef std::tuple<typename FirstTrait::TraitType, typename Traits::TraitType...> TraitType;
    typedef std::tuple<typename FirstTrait::EmptyValueType, typename Traits::EmptyValueType...> EmptyValueType;

    // We should use emptyValueIsZero = Traits::emptyValueIsZero &&... whenever we switch to C++17. We can't do anything
    // better here right now because GCC can't do C++.
    template<typename BoolType>
    static constexpr bool allTrue(BoolType value) { return value; }
    template<typename BoolType, typename... BoolTypes>
    static constexpr bool allTrue(BoolType value, BoolTypes... values) { return value && allTrue(values...); }
    static constexpr bool emptyValueIsZero = allTrue(FirstTrait::emptyValueIsZero, Traits::emptyValueIsZero...);
    static EmptyValueType emptyValue() { return std::make_tuple(FirstTrait::emptyValue(), Traits::emptyValue()...); }

    static constexpr unsigned minimumTableSize = FirstTrait::minimumTableSize;

    static void constructDeletedValue(TraitType& slot) { FirstTrait::constructDeletedValue(std::get<0>(slot)); }
    static bool isDeletedValue(const TraitType& value) { return FirstTrait::isDeletedValue(std::get<0>(value)); }
};

template<typename... Traits>
struct HashTraits<std::tuple<Traits...>> : public TupleHashTraits<HashTraits<Traits>...> { };

template<typename KeyTraitsArg, typename ValueTraitsArg>
struct KeyValuePairHashTraits : GenericHashTraits<KeyValuePair<typename KeyTraitsArg::TraitType, typename ValueTraitsArg::TraitType>> {
    typedef KeyTraitsArg KeyTraits;
    typedef ValueTraitsArg ValueTraits;
    typedef KeyValuePair<typename KeyTraits::TraitType, typename ValueTraits::TraitType> TraitType;
    typedef KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType> EmptyValueType;
    typedef typename ValueTraitsArg::TraitType ValueType;

    static constexpr bool emptyValueIsZero = KeyTraits::emptyValueIsZero && ValueTraits::emptyValueIsZero;
    static EmptyValueType emptyValue() { return KeyValuePair<typename KeyTraits::EmptyValueType, typename ValueTraits::EmptyValueType>(KeyTraits::emptyValue(), ValueTraits::emptyValue()); }

    template <typename>
    static void constructEmptyValue(TraitType& slot)
    {
        KeyTraits::template constructEmptyValue<KeyTraits>(slot.key);
        ValueTraits::template constructEmptyValue<ValueTraits>(slot.value);
    }

    static constexpr unsigned minimumTableSize = KeyTraits::minimumTableSize;

    static void constructDeletedValue(TraitType& slot) { KeyTraits::constructDeletedValue(slot.key); }
    static bool isDeletedValue(const TraitType& value) { return KeyTraits::isDeletedValue(value.key); }

    static void customDeleteBucket(TraitType& value)
    {
        static_assert(std::is_trivially_destructible<KeyValuePair<int, int>>::value,
            "The wrapper itself has to be trivially destructible for customDeleteBucket() to make sense, since we do not destruct the wrapper itself.");

        hashTraitsDeleteBucket<KeyTraits>(value.key);
        value.value.~ValueType();
    }
};

template<typename Key, typename Value>
struct HashTraits<KeyValuePair<Key, Value>> : public KeyValuePairHashTraits<HashTraits<Key>, HashTraits<Value>> { };

template<typename T>
struct NullableHashTraits : public HashTraits<T> {
    static constexpr bool emptyValueIsZero = false;
    static T emptyValue() { return reinterpret_cast<T>(1); }
};

template<typename T, size_t inlineCapacity>
struct HashTraits<Vector<T, inlineCapacity>> : GenericHashTraits<Vector<T, inlineCapacity>> {
    static constexpr bool emptyValueIsZero = !inlineCapacity;

    static void constructDeletedValue(Vector<T, inlineCapacity>& slot) { new (NotNull, std::addressof(slot)) Vector<T, inlineCapacity>(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const Vector<T, inlineCapacity>& value) { return value.isHashTableDeletedValue(); }
};

// Useful for classes that want complete control over what is empty and what is deleted,
// and how to construct both.
template<typename T>
struct CustomHashTraits : public GenericHashTraits<T> {
    static constexpr bool emptyValueIsZero = false;
    static constexpr bool hasIsEmptyValueFunction = true;
    
    static void constructDeletedValue(T& slot)
    {
        new (NotNull, std::addressof(slot)) T(T::DeletedValue);
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
using WTF::KeyValuePair;
using WTF::PairHashTraits;
using WTF::NullableHashTraits;
using WTF::SimpleClassHashTraits;
