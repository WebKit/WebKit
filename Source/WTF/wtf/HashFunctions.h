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

#include <stdint.h>
#include <tuple>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

namespace WTF {

    template<size_t size> struct IntTypes;
    template<> struct IntTypes<1> { typedef int8_t SignedType; typedef uint8_t UnsignedType; };
    template<> struct IntTypes<2> { typedef int16_t SignedType; typedef uint16_t UnsignedType; };
    template<> struct IntTypes<4> { typedef int32_t SignedType; typedef uint32_t UnsignedType; };
    template<> struct IntTypes<8> { typedef int64_t SignedType; typedef uint64_t UnsignedType; };

    // integer hash function

    // Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
    inline unsigned intHash(uint8_t key8)
    {
        unsigned key = key8;
        key += ~(key << 15);
        key ^= (key >> 10);
        key += (key << 3);
        key ^= (key >> 6);
        key += ~(key << 11);
        key ^= (key >> 16);
        return key;
    }

    // Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
    inline unsigned intHash(uint16_t key16)
    {
        unsigned key = key16;
        key += ~(key << 15);
        key ^= (key >> 10);
        key += (key << 3);
        key ^= (key >> 6);
        key += ~(key << 11);
        key ^= (key >> 16);
        return key;
    }

    // Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
    inline unsigned intHash(uint32_t key) 
    {
        key += ~(key << 15);
        key ^= (key >> 10);
        key += (key << 3);
        key ^= (key >> 6);
        key += ~(key << 11);
        key ^= (key >> 16);
        return key;
    }
    
    // Thomas Wang's 64 bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
    inline unsigned intHash(uint64_t key)
    {
        key += ~(key << 32);
        key ^= (key >> 22);
        key += ~(key << 13);
        key ^= (key >> 8);
        key += (key << 3);
        key ^= (key >> 15);
        key += ~(key << 27);
        key ^= (key >> 31);
        return static_cast<unsigned>(key);
    }

    // Compound integer hash method: http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
    inline unsigned pairIntHash(unsigned key1, unsigned key2)
    {
        unsigned shortRandom1 = 277951225; // A random 32-bit value.
        unsigned shortRandom2 = 95187966; // A random 32-bit value.
        uint64_t longRandom = 19248658165952622LL; // A random 64-bit value.

        uint64_t product = longRandom * (shortRandom1 * key1 + shortRandom2 * key2);
        unsigned highBits = static_cast<unsigned>(product >> (sizeof(uint64_t) - sizeof(unsigned)));
        return highBits;
    }

    template<typename T> struct IntHash {
        static unsigned hash(T key) { return intHash(static_cast<typename IntTypes<sizeof(T)>::UnsignedType>(key)); }
        static bool equal(T a, T b) { return a == b; }
        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };

    template<typename T> struct FloatHash {
        typedef typename IntTypes<sizeof(T)>::UnsignedType Bits;
        static unsigned hash(T key)
        {
            return intHash(bitwise_cast<Bits>(key));
        }
        static bool equal(T a, T b)
        {
            return bitwise_cast<Bits>(a) == bitwise_cast<Bits>(b);
        }
        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };

    // pointer identity hash function

    template<typename T, bool isSmartPointer> 
    struct PtrHashBase;

    template <typename T>
    struct PtrHashBase<T, false /* isSmartPtr */> {
        typedef T PtrType; 

        static unsigned hash(PtrType key) { return IntHash<uintptr_t>::hash(reinterpret_cast<uintptr_t>(key)); }
        static bool equal(PtrType a, PtrType b) { return a == b; }
        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };

    template <typename T>
    struct PtrHashBase<T, true /* isSmartPtr */> {
        typedef typename GetPtrHelper<T>::PtrType PtrType; 

        static unsigned hash(PtrType key) { return IntHash<uintptr_t>::hash(reinterpret_cast<uintptr_t>(key)); }
        static bool equal(PtrType a, PtrType b) { return a == b; }
        static constexpr bool safeToCompareToEmptyOrDeleted = true;

        static unsigned hash(const T& key) { return hash(getPtr(key)); }
        static bool equal(const T& a, const T& b) { return getPtr(a) == getPtr(b); }
        static bool equal(PtrType a, const T& b) { return a == getPtr(b); }
        static bool equal(const T& a, PtrType b) { return getPtr(a) == b; }
    };

    template<typename T> struct PtrHash : PtrHashBase<T, IsSmartPtr<T>::value> {
    };

    template<typename P> struct PtrHash<Ref<P>> : PtrHashBase<Ref<P>, IsSmartPtr<Ref<P>>::value> {
        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };

    // default hash function for each type

    template<typename T> struct DefaultHash;

    template<typename T, typename U> struct PairHash {
        static unsigned hash(const std::pair<T, U>& p)
        {
            return pairIntHash(DefaultHash<T>::hash(p.first), DefaultHash<U>::hash(p.second));
        }
        static bool equal(const std::pair<T, U>& a, const std::pair<T, U>& b)
        {
            return DefaultHash<T>::equal(a.first, b.first) && DefaultHash<U>::equal(a.second, b.second);
        }
        static constexpr bool safeToCompareToEmptyOrDeleted = DefaultHash<T>::safeToCompareToEmptyOrDeleted && DefaultHash<U>::safeToCompareToEmptyOrDeleted;
    };

    template<typename T, typename U> struct IntPairHash {
        static unsigned hash(const std::pair<T, U>& p) { return pairIntHash(p.first, p.second); }
        static bool equal(const std::pair<T, U>& a, const std::pair<T, U>& b) { return PairHash<T, T>::equal(a, b); }
        static constexpr bool safeToCompareToEmptyOrDeleted = PairHash<T, U>::safeToCompareToEmptyOrDeleted;
    };

    template<typename... Types>
    struct TupleHash {
        template<size_t I = 0>
        static typename std::enable_if<I < sizeof...(Types) - 1, unsigned>::type hash(const std::tuple<Types...>& t)
        {
            using IthTupleElementType = typename std::tuple_element<I, typename std::tuple<Types...>>::type;
            return pairIntHash(DefaultHash<IthTupleElementType>::hash(std::get<I>(t)), hash<I + 1>(t));
        }

        template<size_t I = 0>
        static typename std::enable_if<I == sizeof...(Types) - 1, unsigned>::type hash(const std::tuple<Types...>& t)
        {
            using IthTupleElementType = typename std::tuple_element<I, typename std::tuple<Types...>>::type;
            return DefaultHash<IthTupleElementType>::hash(std::get<I>(t));
        }

        template<size_t I = 0>
        static typename std::enable_if<I < sizeof...(Types) - 1, bool>::type equal(const std::tuple<Types...>& a, const std::tuple<Types...>& b)
        {
            using IthTupleElementType = typename std::tuple_element<I, typename std::tuple<Types...>>::type;
            return DefaultHash<IthTupleElementType>::equal(std::get<I>(a), std::get<I>(b)) && equal<I + 1>(a, b);
        }

        template<size_t I = 0>
        static typename std::enable_if<I == sizeof...(Types) - 1, bool>::type equal(const std::tuple<Types...>& a, const std::tuple<Types...>& b)
        {
            using IthTupleElementType = typename std::tuple_element<I, typename std::tuple<Types...>>::type;
            return DefaultHash<IthTupleElementType>::equal(std::get<I>(a), std::get<I>(b));
        }

        // We should use safeToCompareToEmptyOrDeleted = DefaultHash<Types>::safeToCompareToEmptyOrDeleted &&... whenever
        // we switch to C++17. We can't do anything better here right now because GCC can't do C++.
        template<typename BoolType>
        static constexpr bool allTrue(BoolType value) { return value; }
        template<typename BoolType, typename... BoolTypes>
        static constexpr bool allTrue(BoolType value, BoolTypes... values) { return value && allTrue(values...); }
        static constexpr bool safeToCompareToEmptyOrDeleted = allTrue(DefaultHash<Types>::safeToCompareToEmptyOrDeleted...);
    };

    // make IntHash the default hash function for many integer types

    template<> struct DefaultHash<bool> : IntHash<uint8_t> { };
    template<> struct DefaultHash<uint8_t> : IntHash<uint8_t> { };
    template<> struct DefaultHash<short> : IntHash<unsigned> { };
    template<> struct DefaultHash<unsigned short> : IntHash<unsigned> { };
    template<> struct DefaultHash<int> : IntHash<unsigned> { };
    template<> struct DefaultHash<unsigned> : IntHash<unsigned> { };
    template<> struct DefaultHash<long> : IntHash<unsigned long> { };
    template<> struct DefaultHash<unsigned long> : IntHash<unsigned long> { };
    template<> struct DefaultHash<long long> : IntHash<unsigned long long> { };
    template<> struct DefaultHash<unsigned long long> : IntHash<unsigned long long> { };

#if defined(_NATIVE_WCHAR_T_DEFINED)
    template<> struct DefaultHash<wchar_t> : IntHash<wchar_t> { };
#endif

    template<> struct DefaultHash<float> : FloatHash<float> { };
    template<> struct DefaultHash<double> : FloatHash<double> { };

    // make PtrHash the default hash function for pointer types that don't specialize

    template<typename P> struct DefaultHash<P*> : PtrHash<P*> { };
    template<typename P> struct DefaultHash<RefPtr<P>> : PtrHash<RefPtr<P>> { };
    template<typename P> struct DefaultHash<Ref<P>> : PtrHash<Ref<P>> { };

    template<typename P, typename Deleter> struct DefaultHash<std::unique_ptr<P, Deleter>> : PtrHash<std::unique_ptr<P, Deleter>> { };

#ifdef __OBJC__
    template<> struct DefaultHash<__unsafe_unretained id> : PtrHash<__unsafe_unretained id> { };
#endif

    // make IntPairHash the default hash function for pairs of (at most) 32-bit integers.

    template<> struct DefaultHash<std::pair<short, short>> : IntPairHash<short, short> { };
    template<> struct DefaultHash<std::pair<short, unsigned short>> : IntPairHash<short, unsigned short> { };
    template<> struct DefaultHash<std::pair<short, int>> : IntPairHash<short, int> { };
    template<> struct DefaultHash<std::pair<short, unsigned>> : IntPairHash<short, unsigned> { };
    template<> struct DefaultHash<std::pair<unsigned short, short>> : IntPairHash<unsigned short, short> { };
    template<> struct DefaultHash<std::pair<unsigned short, unsigned short>> : IntPairHash<unsigned short, unsigned short> { };
    template<> struct DefaultHash<std::pair<unsigned short, int>> : IntPairHash<unsigned short, int> { };
    template<> struct DefaultHash<std::pair<unsigned short, unsigned>> : IntPairHash<unsigned short, unsigned> { };
    template<> struct DefaultHash<std::pair<int, short>> : IntPairHash<int, short> { };
    template<> struct DefaultHash<std::pair<int, unsigned short>> : IntPairHash<int, unsigned short> { };
    template<> struct DefaultHash<std::pair<int, int>> : IntPairHash<int, int> { };
    template<> struct DefaultHash<std::pair<int, unsigned>> : IntPairHash<unsigned, unsigned> { };
    template<> struct DefaultHash<std::pair<unsigned, short>> : IntPairHash<unsigned, short> { };
    template<> struct DefaultHash<std::pair<unsigned, unsigned short>> : IntPairHash<unsigned, unsigned short> { };
    template<> struct DefaultHash<std::pair<unsigned, int>> : IntPairHash<unsigned, int> { };
    template<> struct DefaultHash<std::pair<unsigned, unsigned>> : IntPairHash<unsigned, unsigned> { };

    // make PairHash the default hash function for pairs of arbitrary values.

    template<typename T, typename U> struct DefaultHash<std::pair<T, U>> : PairHash<T, U> { };
    template<typename... Types> struct DefaultHash<std::tuple<Types...>> : TupleHash<Types...> { };

} // namespace WTF

using WTF::DefaultHash;
using WTF::IntHash;
using WTF::PtrHash;
