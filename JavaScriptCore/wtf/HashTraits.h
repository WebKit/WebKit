/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
    template<> struct IsInteger<wchar_t>            { static const bool value = true; };
#endif

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

#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
    COMPILE_ASSERT(IsInteger<wchar_t>::value, WTF_IsInteger_wchar_t_true);
#endif

    COMPILE_ASSERT(!IsInteger<char*>::value, WTF_IsInteger_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<const char* >::value, WTF_IsInteger_const_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<volatile char* >::value, WTF_IsInteger_volatile_char_pointer__false);
    COMPILE_ASSERT(!IsInteger<double>::value, WTF_IsInteger_double_false);
    COMPILE_ASSERT(!IsInteger<float>::value, WTF_IsInteger_float_false);

    template<typename T> struct HashTraits;

    template<bool isInteger, typename T> struct GenericHashTraitsBase;

    template<typename T> struct GenericHashTraitsBase<false, T> {
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = true;
    };

    // Default integer traits disallow both 0 and -1 as keys (max value instead of -1 for unsigned).
    template<typename T> struct GenericHashTraitsBase<true, T> {
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static void constructDeletedValue(T& slot) { slot = static_cast<T>(-1); }
        static bool isDeletedValue(T value) { return value == static_cast<T>(-1); }
    };

    template<typename T> struct GenericHashTraits : GenericHashTraitsBase<IsInteger<T>::value, T> {
        typedef T TraitType;
        static T emptyValue() { return T(); }
    };

    template<typename T> struct HashTraits : GenericHashTraits<T> { };

    template<typename T> struct FloatHashTraits : GenericHashTraits<T> {
        static const bool needsDestruction = false;
        static T emptyValue() { return std::numeric_limits<T>::infinity(); }
        static void constructDeletedValue(T& slot) { slot = -std::numeric_limits<T>::infinity(); }
        static bool isDeletedValue(T value) { return value == -std::numeric_limits<T>::infinity(); }
    };

    template<> struct HashTraits<float> : FloatHashTraits<float> { };
    template<> struct HashTraits<double> : FloatHashTraits<double> { };

    // Default unsigned traits disallow both 0 and max as keys -- use these traits to allow zero and disallow max - 1.
    template<typename T> struct UnsignedWithZeroKeyHashTraits : GenericHashTraits<T> {
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
        static T emptyValue() { return std::numeric_limits<T>::max(); }
        static void constructDeletedValue(T& slot) { slot = std::numeric_limits<T>::max() - 1; }
        static bool isDeletedValue(T value) { return value == std::numeric_limits<T>::max() - 1; }
    };

    template<typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static void constructDeletedValue(P*& slot) { slot = reinterpret_cast<P*>(-1); }
        static bool isDeletedValue(P* value) { return value == reinterpret_cast<P*>(-1); }
    };

    template<typename P> struct HashTraits<RefPtr<P> > : GenericHashTraits<RefPtr<P> > {
        static const bool emptyValueIsZero = true;
        static void constructDeletedValue(RefPtr<P>& slot) { new (&slot) RefPtr<P>(HashTableDeletedValue); }
        static bool isDeletedValue(const RefPtr<P>& value) { return value.isHashTableDeletedValue(); }
    };

    // special traits for pairs, helpful for their use in HashMap implementation

    template<typename FirstTraitsArg, typename SecondTraitsArg>
    struct PairHashTraits : GenericHashTraits<pair<typename FirstTraitsArg::TraitType, typename SecondTraitsArg::TraitType> > {
        typedef FirstTraitsArg FirstTraits;
        typedef SecondTraitsArg SecondTraits;
        typedef pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> TraitType;

        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
        static TraitType emptyValue() { return make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;

        static void constructDeletedValue(TraitType& slot) { FirstTraits::constructDeletedValue(slot.first); }
        static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
    };

    template<typename First, typename Second>
    struct HashTraits<pair<First, Second> > : public PairHashTraits<HashTraits<First>, HashTraits<Second> > { };

} // namespace WTF

using WTF::HashTraits;
using WTF::PairHashTraits;

#endif // WTF_HashTraits_h
