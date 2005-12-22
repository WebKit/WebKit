// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KXMLCORE_HASH_TRAITS_H
#define KXMLCORE_HASH_TRAITS_H

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
        
    template<typename T>
    struct HashTraits {
        typedef T traitType;
        static const bool emptyValueIsZero = IsInteger<T>::value;
        
        static traitType emptyValue() {
            return traitType();
        }
    };

    // may not be appropriate for all uses since it would disallow 0 and -1 as keys
    template<>
    struct HashTraits<int> {
        typedef int traitType;
        static const bool emptyValueIsZero = true;
        
        static traitType emptyValue() {
            return 0;
        }
        static traitType deletedValue() {
            return -1;
        }
    };

    template<typename P>
    struct HashTraits<P *> {
        typedef P *traitType;
        static const bool emptyValueIsZero = true;
        
        static traitType emptyValue() {
            return reinterpret_cast<P *>(0);
        }
        static traitType deletedValue() {
            return reinterpret_cast<P *>(-1);
        }
    };

    template<typename FirstTraits, typename SecondTraits>
    struct PairHashTraits {
    private:
        typedef typename FirstTraits::traitType FirstType;
        typedef typename SecondTraits::traitType SecondType;
    public:
        typedef pair<FirstType, SecondType> traitType;
        
        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
        
        static traitType emptyValue() {
            return traitType(FirstTraits::emptyValue(), SecondTraits::emptyValue());
        }
        static traitType deletedValue() {
            return traitType(FirstTraits::deletedValue(), SecondTraits::emptyValue());
        }
    };
    
    template<typename First, typename Second>
    struct HashTraits<pair<First, Second> > : public PairHashTraits<HashTraits<First>, HashTraits<Second> > {
    };

} // namespace KXMLCore

using KXMLCore::HashTraits;
using KXMLCore::PairHashTraits;

#endif // KXMLCORE_HASH_TRAITS_H
