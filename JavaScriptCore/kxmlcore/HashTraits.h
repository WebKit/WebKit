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

#ifndef KXMLCORE_HASH_TRAITS_H
#define KXMLCORE_HASH_TRAITS_H

#include "RefPtr.h"
#include <utility>

namespace KXMLCore {

    using std::pair;

    template <typename T> struct IsInteger           { static const bool value = false; };
    template <> struct IsInteger<bool>               { static const bool value = true; };
    template <> struct IsInteger<char>               { static const bool value = true; };
    template <> struct IsInteger<signed char>        { static const bool value = true; };
    template <> struct IsInteger<unsigned char>      { static const bool value = true; };
    template <> struct IsInteger<short>              { static const bool value = true; };
    template <> struct IsInteger<unsigned short>     { static const bool value = true; };
    template <> struct IsInteger<int>                { static const bool value = true; };
    template <> struct IsInteger<unsigned int>       { static const bool value = true; };
    template <> struct IsInteger<long>               { static const bool value = true; };
    template <> struct IsInteger<unsigned long>      { static const bool value = true; };
    template <> struct IsInteger<long long>          { static const bool value = true; };
    template <> struct IsInteger<unsigned long long> { static const bool value = true; };

    template<bool isInteger, typename T> struct GenericHashTraitsBase;
    template<typename T> struct GenericHashTraitsBase<true, T> {
        typedef T TraitType;
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static TraitType emptyValue() { return 0; }
    };
    template<typename T> struct GenericHashTraitsBase<false, T> {
        typedef T TraitType;
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = true;
        static TraitType emptyValue() { return TraitType(); }
    };

    template<typename T> struct GenericHashTraits : GenericHashTraitsBase<IsInteger<T>::value, T> { };

    template<typename T> struct HashTraits : GenericHashTraits<T> { };

    // may not be appropriate for all uses since it disallows 0 and -1 as keys
    template<> struct HashTraits<int> : GenericHashTraits<int> {
        static TraitType deletedValue() { return -1; }
    };

    template<typename P> struct HashTraits<P*> : GenericHashTraits<P*> {
        typedef P* TraitType;
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
        static TraitType emptyValue() { return 0; }
        static TraitType deletedValue() { return reinterpret_cast<P*>(-1); }
    };

    template<typename P> struct HashTraits<RefPtr<P> > : GenericHashTraits<RefPtr<P> > {
        static const bool emptyValueIsZero = true;
    };

    template<typename T, typename Traits> class DeletedValueAssigner;

    template<typename T, typename Traits> inline void assignDeleted(T& location)
    {
        DeletedValueAssigner<T, Traits>::assignDeletedValue(location);
    }

    template<typename FirstTraits, typename SecondTraits>
    struct PairHashTraits : GenericHashTraits<pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType> > {
    private:
        typedef typename FirstTraits::TraitType FirstType;
        typedef typename SecondTraits::TraitType SecondType;
    public:
        typedef pair<FirstType, SecondType> TraitType;

        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;

        static TraitType emptyValue()
        {
            return TraitType(FirstTraits::emptyValue(), SecondTraits::emptyValue());
        }

        static TraitType deletedValue()
        {
            return TraitType(FirstTraits::deletedValue(), SecondTraits::emptyValue());
        }

        static void assignDeletedValue(TraitType& location)
        {
            assignDeleted<FirstType, FirstTraits>(location.first);
            location.second = SecondTraits::emptyValue();
        }
    };

    template<typename First, typename Second>
    struct HashTraits<pair<First, Second> > : public PairHashTraits<HashTraits<First>, HashTraits<Second> > { };

    template<typename T, typename Traits> struct DeletedValueAssigner {
        static void assignDeletedValue(T& location) { location = Traits::deletedValue(); }
    };

    template<typename FirstTraits, typename SecondTraits>
    struct DeletedValueAssigner<pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType>, PairHashTraits<FirstTraits, SecondTraits> >
    {
        static void assignDeletedValue(pair<typename FirstTraits::TraitType, typename SecondTraits::TraitType>& location)
        {
            PairHashTraits<FirstTraits, SecondTraits>::assignDeletedValue(location);
        }
    };

    template<typename First, typename Second>
    struct DeletedValueAssigner<pair<First, Second>, HashTraits<pair<First, Second> > >
    {
        static void assignDeletedValue(pair<First, Second>& location)
        {
            HashTraits<pair<First, Second> >::assignDeletedValue(location);
        }
    };


} // namespace KXMLCore

using KXMLCore::HashTraits;
using KXMLCore::PairHashTraits;

#endif // KXMLCORE_HASH_TRAITS_H
