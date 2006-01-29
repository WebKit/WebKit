// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef KXMLCORE_VECTOR_TRAITS_H
#define KXMLCORE_VECTOR_TRAITS_H

#include "RefPtr.h"
#include <utility>

using std::pair;

namespace KXMLCore {

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
    template <typename P> struct IsPod<P *>      { static const bool value = true; };

    template<bool isPod, typename T>
    class VectorTraitsBase;

    template<typename T>
    struct VectorTraitsBase<false, T>
    {
        static const bool needsDestruction = true;
        static const bool needsInitialization = true;
        static const bool canInitializeWithMemset = true;
        static const bool canMoveWithMemcpy = false;
        static const bool canCopyWithMemcpy = false;
        static const bool canFillWithMemset = false;
    };

    template<typename T>
    struct VectorTraitsBase<true, T>
    {
        static const bool needsDestruction = false;
        static const bool needsInitialization = false;
        static const bool canInitializeWithMemset = false;
        static const bool canMoveWithMemcpy = true;
        static const bool canCopyWithMemcpy = true;
        static const bool canFillWithMemset = sizeof(T) == sizeof(char);
    };

    template<typename T>
    struct VectorTraits : VectorTraitsBase<IsPod<T>::value, T> { };

    struct SimpleClassVectorTraits
    {
        static const bool needsDestruction = false;
        static const bool needsInitialization = true;
        static const bool canInitializeWithMemset = true;
        static const bool canMoveWithMemcpy = true;
        static const bool canCopyWithMemcpy = false;
        static const bool canFillWithMemset = false;
    };

    // we know RefPtr is simple enough that initializing to 0 and moving with memcpy
    // (and then not destructing the original) will totally work
    template<typename P>
    struct VectorTraits<RefPtr<P> > : SimpleClassVectorTraits { };
    
    template<typename First, typename Second>
    struct VectorTraits<pair<First, Second> >
    {
        typedef VectorTraits<First> FirstTraits;
        typedef VectorTraits<Second> SecondTraits;

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;
        static const bool needsInitialization = FirstTraits::needsInitialization || SecondTraits::needsInitialization;
        static const bool canInitializeWithMemset = FirstTraits::canInitializeWithMemset && SecondTraits::canInitializeWithMemset;
        static const bool canMoveWithMemcpy = FirstTraits::canMoveWithMemcpy && SecondTraits::canMoveWithMemcpy;
        static const bool canCopyWithMemcpy = FirstTraits::canCopyWithMemcpy && SecondTraits::canCopyWithMemcpy;
        static const bool canFillWithMemset = false;
    };

} // namespace KXMLCore

using KXMLCore::VectorTraits;
using KXMLCore::SimpleClassVectorTraits;

#endif // KXMLCORE_VECTOR_TRAITS_H
