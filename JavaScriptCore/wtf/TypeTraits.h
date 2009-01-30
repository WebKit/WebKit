 /*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef TypeTraits_h
#define TypeTraits_h

#include "Assertions.h"

namespace WTF {

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
    COMPILE_ASSERT(!IsInteger<const char*>::value, WTF_IsInteger_const_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<volatile char*>::value, WTF_IsInteger_volatile_char_pointer_false);
    COMPILE_ASSERT(!IsInteger<double>::value, WTF_IsInteger_double_false);
    COMPILE_ASSERT(!IsInteger<float>::value, WTF_IsInteger_float_false);

    template <typename T> struct IsPod           { static const bool value = false; };
    template <> struct IsPod<bool>               { static const bool value = true; };
    template <> struct IsPod<char>               { static const bool value = true; };
    template <> struct IsPod<signed char>        { static const bool value = true; };
    template <> struct IsPod<unsigned char>      { static const bool value = true; };
    template <> struct IsPod<short>              { static const bool value = true; };
    template <> struct IsPod<unsigned short>     { static const bool value = true; };
    template <> struct IsPod<int>                { static const bool value = true; };
    template <> struct IsPod<unsigned int>       { static const bool value = true; };
    template <> struct IsPod<long>               { static const bool value = true; };
    template <> struct IsPod<unsigned long>      { static const bool value = true; };
    template <> struct IsPod<long long>          { static const bool value = true; };
    template <> struct IsPod<unsigned long long> { static const bool value = true; };
    template <> struct IsPod<float>              { static const bool value = true; };
    template <> struct IsPod<double>             { static const bool value = true; };
    template <> struct IsPod<long double>        { static const bool value = true; };
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
    template<> struct  IsPod<wchar_t>            { static const bool value = true; };
#endif
    template <typename P> struct IsPod<P*>       { static const bool value = true; };

    COMPILE_ASSERT(IsPod<bool>::value, WTF_IsPod_bool_true);
    COMPILE_ASSERT(IsPod<char>::value, WTF_IsPod_char_true);
    COMPILE_ASSERT(IsPod<signed char>::value, WTF_IsPod_signed_char_true);
    COMPILE_ASSERT(IsPod<unsigned char>::value, WTF_IsPod_unsigned_char_true);
    COMPILE_ASSERT(IsPod<short>::value, WTF_IsPod_short_true);
    COMPILE_ASSERT(IsPod<unsigned short>::value, WTF_IsPod_unsigned_short_true);
    COMPILE_ASSERT(IsPod<int>::value, WTF_IsPod_int_true);
    COMPILE_ASSERT(IsPod<unsigned int>::value, WTF_IsPod_unsigned_int_true);
    COMPILE_ASSERT(IsPod<long>::value, WTF_IsPod_long_true);
    COMPILE_ASSERT(IsPod<unsigned long>::value, WTF_IsPod_unsigned_long_true);
    COMPILE_ASSERT(IsPod<long long>::value, WTF_IsPod_long_long_true);
    COMPILE_ASSERT(IsPod<unsigned long long>::value, WTF_IsPod_unsigned_long_long_true);
#if !COMPILER(MSVC) || defined(_NATIVE_WCHAR_T_DEFINED)
    COMPILE_ASSERT(IsPod<wchar_t>::value, WTF_IsPod_wchar_t_true);
#endif
    COMPILE_ASSERT(IsPod<char*>::value, WTF_IsPod_char_pointer_true);
    COMPILE_ASSERT(IsPod<const char*>::value, WTF_IsPod_const_char_pointer_true);
    COMPILE_ASSERT(IsPod<volatile char*>::value, WTF_IsPod_volatile_char_pointer_true);
    COMPILE_ASSERT(IsPod<double>::value, WTF_IsPod_double_true);
    COMPILE_ASSERT(IsPod<long double>::value, WTF_IsPod_long_double_true);
    COMPILE_ASSERT(IsPod<float>::value, WTF_IsPod_float_true);
    COMPILE_ASSERT(!IsPod<IsPod<bool> >::value, WTF_IsPod_struct_false);

    template <typename T> struct RemovePointer {
        typedef T Type;
    };

    template <typename T> struct RemovePointer<T*> {
        typedef T Type;
    };

} // namespace WTF

#endif // TypeTraits_h
