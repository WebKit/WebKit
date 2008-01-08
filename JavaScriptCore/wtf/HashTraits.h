// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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

#include "Assertions.h"
#include "HashFunctions.h"
#include <utility>
#include <limits>

namespace WTF {

    using std::pair;
    using std::make_pair;

    template<typename T> struct IsInteger           { static const bool value = false; };
    template<> struct IsInteger<bool>               { static const bool value = true; };
    template<> struct IsInteger<char>               { static const bool value = true; };
    template<> struct IsInteger<signed char>        { static const bool value = true; };
    template<> struct IsInteger<unsigned char>      { static const bool value = true; };
    template<> struct IsInteger<short>              { static const bool value = true; };
    template<> struct IsInteger<unsigned short>     { static const bool value = true; };
    template<> struct IsInteger<int>                { static const bool value = true; };
    template<> struct IsInteger<unsigned int>       { static const bool value = true; };
    template<> struct IsInteger<long>               { static const bool value = true; };
    template<> struct IsInteger<unsigned long>      { static const bool value = true; };
    template<> struct IsInteger<long long>          { static const bool value = true; };
    template<> struct IsInteger<unsigned long long> { static const bool value = true; };

    COMPILE_ASSERT(IsInteger<bool>::value, WTF_IsInteger_bool_true);
    COMPILE_ASSERT(IsInteger<char>::value, WTF_IsInteger_char_true);
    COMPILE_ASSERT(IsInteger<signed char>::value, WTF_IsInteger_signed_char_true);
    COMPILE_ASSERT(IsInteger<unsigned char>::value, WTF_IsInteger_unsigned_char_true);
    COMPILE_ASSERT(IsInteger<short>::value, WTF_IsInteger_short_true);
    COMPILE_ASSERT(IsInteger<unsigned short>::value, WTF_IsInteger_unsigned_short_true);
    COMPILE_ASSERT(IsInteger<int>::value, WTF_IsInteger_int_true);
    COMPILE_ASSERT(IsInteger<unsigned int>::value, WTF_IsInteger_unsigned_int_true);
    COMPILE_ASSERT(IsInteger<long>::value, WTF_IsInteger_long_true);
    COMPILE_ASSERT(IsInteger<unsigned long>::value, WTF_IsInteger_unsigned_long_true);
    COMPILE_ASSERT(IsInteger<long long>::value, WTF_IsInteger_long_long_true);
    COMPILE_ASSERT(IsInteger<unsigned long long>::value, WTF_IsInteger_unsigned_long_long_true);

    COMPILE_ASSERT(!IsInteger<char*>::value, WTF_IsInteger_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<const char* >::value, WTF_IsInteger_const_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<volatile char* >::value, WTF_IsInteger_volatile_char_pointer__false);
    COMPILE_ASSERT(!IsInteger<double>::value, WTF_IsInteger_double_false);
    COMPILE_ASSERT(!IsInteger<float>::value, WTF_IsInteger_float_false);

    template<typename T> struct HashTraits;

    template<bool isInteger, typename T> struct GenericHashTraitsBase;
    template<typename T> struct GenericHashTraitsBase<true, T> {
        typedef T TraitType;
        typedef HashTraits<typename IntTypes<sizeof(T)>::SignedType> StorageTraits;
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
    };
    template<typename T> struct GenericHashTraitsBase<false, T> {
        typedef T TraitType;
        typedef HashTraits<T> StorageTraits;
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = true;
    };

    template<typename T> struct GenericHashTraits : GenericHashTraitsBase<IsInteger<T>::value, T> {
        static T emptyValue() { return T(); }
        static const bool needsRef = false;
    };

    template<typename T> struct HashTraits : GenericHashTraits<T> { };

    // signed integer traits may not be appropriate for all uses since they disallow 0 and -1 as keys
    template<> struct HashTraits<signed char> : GenericHashTraits<int> {
        static signed char deletedValue() { return -1; }
    };
    template<> struct HashTraits<short> : GenericHashTraits<int> {
        static short deletedValue() { return -1; }
    };
    template<> struct HashTraits<int> : GenericHashTraits<int> {
        static int deletedValue() { return -1; }
    };
    template<> struct HashTraits<unsigned int> : GenericHashTraits<unsigned int> {
        static unsigned int deletedValue() { return static_cast<unsigned int>(-1); }
    };
    template<> struct HashTraits<long> : GenericHashTraits<long> {
        static long deletedValue() { return -1; }
    };
    template<> struct HashTraits<unsigned long> : GenericHashTraits<unsigned long> {
        static unsigned long deletedValue() { return static_cast<unsigned long>(-1); }
    };
    template<> struct HashTraits<long long> : GenericHashTraits<long long> {
        static long long deletedValue() { return -1; }
    };
    template<> struct HashTraits<unsigned long long> : GenericHashTraits<unsigned long long> {
        static unsigned long long deletedValue() { return static_cast<unsigned long long>(-1); }
    };
    
    template<typename T> struct FloatHashTraits {
        typedef T TraitType;
        typedef HashTraits<T> StorageTraits;
        static T emptyValue() { return std::numeric_limits<T>::infinity(); }
        static T deletedValue() { return -std::numeric_limits<T>::infinity(); }
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
        static const bool needsRef = false;
    };
    template<> struct HashTraits<float> : FloatHashTraits<float> {
    };
    template<> struct HashTraits<double> : FloatHashTraits<double> {
    };

    template<typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
        typedef HashTraits<typename IntTypes<sizeof(P*)>::SignedType> StorageTraits;
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static P* deletedValue() { return reinterpret_cast<P*>(-1); }
    };

    template<typename P> struct HashTraits<RefPtr<P> > : GenericHashTraits<RefPtr<P> > {
        typedef HashTraits<typename IntTypes<sizeof(P*)>::SignedType> StorageTraits;
        typedef typename StorageTraits::TraitType StorageType;
        static const bool emptyValueIsZero = true;
        static const bool needsRef = true;

        typedef union { 
            P* m_p; 
            StorageType m_s; 
        } UnionType;

        static void ref(const StorageType& s) 
        { 
            if (const P* p = reinterpret_cast<const UnionType*>(&s)->m_p) 
                const_cast<P*>(p)->ref(); 
        }
        static void deref(const StorageType& s) 
        { 
            if (const P* p = reinterpret_cast<const UnionType*>(&s)->m_p) 
                const_cast<P*>(p)->deref(); 
        }
    };

    // template to set deleted values

    template<typename Traits> struct DeletedValueAssigner {
        static void assignDeletedValue(typename Traits::TraitType& location) { location = Traits::deletedValue(); }
    };

    template<typename T, typename Traits> inline void assignDeleted(T& location)
    {
        DeletedValueAssigner<Traits>::assignDeletedValue(location);
    }

    // special traits for pairs, helpful for their use in HashMap implementation

    template<typename FirstTraits, typename SecondTraits> struct PairHashTraits;

    template<typename FirstTraitsArg, typename SecondTraitsArg>
    struct PairBaseHashTraits : GenericHashTraits<pair<typename FirstTraitsArg::TraitType, typename SecondTraitsArg::TraitType> > {
        typedef FirstTraitsArg FirstTraits;
        typedef SecondTraitsArg SecondTraits;
        typedef pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;

        typedef PairHashTraits<typename FirstTraits::StorageTraits, typename SecondTraits::StorageTraits> StorageTraits;

        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;

        static TraitType emptyValue()
        {
            return make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue());
        }
    };

    template<typename FirstTraits, typename SecondTraits>
    struct PairHashTraits : PairBaseHashTraits<FirstTraits, SecondTraits> {
        typedef pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;

        static TraitType deletedValue()
        {
            return TraitType(FirstTraits::deletedValue(), SecondTraits::emptyValue());
        }

        static void assignDeletedValue(TraitType& location)
        {
            assignDeleted<typename FirstTraits::TraitType, FirstTraits>(location.first);
            location.second = SecondTraits::emptyValue();
        }
    };

    template<typename First, typename Second>
    struct HashTraits<pair<First, Second> > : public PairHashTraits<HashTraits<First>, HashTraits<Second> > { };

    template<typename FirstTraits, typename SecondTraits>
    struct DeletedValueAssigner<PairHashTraits<FirstTraits, SecondTraits> >
    {
        static void assignDeletedValue(pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType>& location)
        {
            PairHashTraits<FirstTraits, SecondTraits>::assignDeletedValue(location);
        }
    };

    template<typename First, typename Second>
    struct DeletedValueAssigner<HashTraits<pair<First, Second> > >
    {
        static void assignDeletedValue(pair<First, Second>& location)
        {
            HashTraits<pair<First, Second> >::assignDeletedValue(location);
        }
    };

    // hash functions and traits that are equivalent (for code sharing)

    template<typename HashArg, typename TraitsArg> struct HashKeyStorageTraits {
        typedef HashArg Hash;
        typedef TraitsArg Traits;
    };
    template<typename P> struct HashKeyStorageTraits<PtrHash<P*>, HashTraits<P*> > {
        typedef typename IntTypes<sizeof(P*)>::SignedType IntType;
        typedef IntHash<IntType> Hash;
        typedef HashTraits<IntType> Traits;
    };
    template<typename P> struct HashKeyStorageTraits<PtrHash<RefPtr<P> >, HashTraits<RefPtr<P> > > {
        typedef typename IntTypes<sizeof(P*)>::SignedType IntType;
        typedef IntHash<IntType> Hash;
        typedef HashTraits<IntType> Traits;
    };

} // namespace WTF

using WTF::HashTraits;
using WTF::PairHashTraits;

#endif // WTF_HashTraits_h
