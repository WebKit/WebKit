/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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

#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <utility>
#include <memory>
#include <type_traits>

namespace WTF {

    class AtomString;

    template<bool isPod, typename T>
    struct VectorTraitsBase;

    template<typename T>
    struct VectorTraitsBase<false, T>
    {
        static constexpr bool needsInitialization = true;
        static constexpr bool canInitializeWithMemset = false;
        static constexpr bool canMoveWithMemcpy = false;
        static constexpr bool canCopyWithMemcpy = false;
        static constexpr bool canFillWithMemset = false;
        static constexpr bool canCompareWithMemcmp = false;
    };

    template<typename T>
    struct VectorTraitsBase<true, T>
    {
        static constexpr bool needsInitialization = false;
        static constexpr bool canInitializeWithMemset = true;
        static constexpr bool canMoveWithMemcpy = true;
        static constexpr bool canCopyWithMemcpy = true;
        static constexpr bool canFillWithMemset = sizeof(T) == sizeof(char) && std::is_integral<T>::value;
        static constexpr bool canCompareWithMemcmp = true;
    };

    template<typename T>
    struct VectorTraits : VectorTraitsBase<std::is_pod<T>::value, T> { };

    struct SimpleClassVectorTraits : VectorTraitsBase<false, void>
    {
        static constexpr bool canInitializeWithMemset = true;
        static constexpr bool canMoveWithMemcpy = true;
        static constexpr bool canCompareWithMemcmp = true;
    };

    // We know smart pointers are simple enough that initializing to 0 and moving with memcpy
    // (and then not destructing the original) will work.

    template<typename P> struct VectorTraits<RefPtr<P>> : SimpleClassVectorTraits { };
    template<typename P> struct VectorTraits<std::unique_ptr<P>> : SimpleClassVectorTraits { };
    template<typename P> struct VectorTraits<Ref<P>> : SimpleClassVectorTraits { };
    template<> struct VectorTraits<AtomString> : SimpleClassVectorTraits { };

    template<typename First, typename Second>
    struct VectorTraits<std::pair<First, Second>>
    {
        typedef VectorTraits<First> FirstTraits;
        typedef VectorTraits<Second> SecondTraits;

        static constexpr bool needsInitialization = FirstTraits::needsInitialization || SecondTraits::needsInitialization;
        static constexpr bool canInitializeWithMemset = FirstTraits::canInitializeWithMemset && SecondTraits::canInitializeWithMemset;
        static constexpr bool canMoveWithMemcpy = FirstTraits::canMoveWithMemcpy && SecondTraits::canMoveWithMemcpy;
        static constexpr bool canCopyWithMemcpy = FirstTraits::canCopyWithMemcpy && SecondTraits::canCopyWithMemcpy;
        static constexpr bool canFillWithMemset = false;
        static constexpr bool canCompareWithMemcmp = FirstTraits::canCompareWithMemcmp && SecondTraits::canCompareWithMemcmp;
    };

} // namespace WTF

using WTF::VectorTraits;
using WTF::SimpleClassVectorTraits;
