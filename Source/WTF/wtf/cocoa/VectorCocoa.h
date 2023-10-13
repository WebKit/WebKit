/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WTF {

// Specialize the behavior of these functions by overloading the makeNSArrayElement
// functions and makeVectorElement functions. The makeNSArrayElement function takes
// a const& to a collection element and can return either a RetainPtr<id> or an id
// if the value is autoreleased. The makeVectorElement function takes an ignored
// pointer to the vector element type, making argument-dependent lookup work, and an
// id for the array element, and returns an std::optional<T> of the the vector element,
// allowing us to filter out array elements that are not of the expected type.
//
//    RetainPtr<id> makeNSArrayElement(const CollectionElementType& collectionElement);
//        -or-
//    id makeNSArrayElement(const VectorElementType& vectorElement);
//
//    std::optional<VectorElementType> makeVectorElement(const VectorElementType*, id arrayElement);

template<typename CollectionType> RetainPtr<NSMutableArray> createNSArray(CollectionType&&);
template<typename VectorElementType> Vector<VectorElementType> makeVector(NSArray *);

// This overload of createNSArray takes a function to map each vector element to an Objective-C object.
// The map function has the same interface as the makeNSArrayElement function above, but can be any
// function including a lambda, a function-like object, or Function<>.
template<typename CollectionType, typename MapFunctionType> RetainPtr<NSMutableArray> createNSArray(CollectionType&&, MapFunctionType&&);

// This overload of makeVector takes a function to map each Objective-C object to a vector element.
// Currently, the map function needs to return an Optional.
template<typename MapFunctionType> Vector<typename std::invoke_result_t<MapFunctionType, id>::value_type> makeVector(NSArray *, MapFunctionType&&);

// Implementation details of the function templates above.

inline void addUnlessNil(NSMutableArray *array, id value)
{
    if (value)
        [array addObject:value];
}

template<typename CollectionType> RetainPtr<NSMutableArray> createNSArray(CollectionType&& collection)
{
    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:std::size(collection)]);
    for (auto&& element : collection)
        addUnlessNil(array.get(), getPtr(makeNSArrayElement(std::forward<decltype(element)>(element))));
    return array;
}

template<typename CollectionType, typename MapFunctionType> RetainPtr<NSMutableArray> createNSArray(CollectionType&& collection, MapFunctionType&& function)
{
    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:std::size(collection)]);
    for (auto&& element : std::forward<CollectionType>(collection)) {
        if constexpr (std::is_rvalue_reference_v<CollectionType&&> && !std::is_const_v<std::remove_reference_t<decltype(element)>>)
            addUnlessNil(array.get(), getPtr(function(WTFMove(element))));
        else
            addUnlessNil(array.get(), getPtr(function(element)));
    }
    return array;
}

template<typename VectorElementType> Vector<VectorElementType> makeVector(NSArray *array)
{
    Vector<VectorElementType> vector;
    vector.reserveInitialCapacity(array.count);
    for (id element in array) {
        constexpr const VectorElementType* typedNull = nullptr;
        if (auto vectorElement = makeVectorElement(typedNull, element))
            vector.append(WTFMove(*vectorElement));
    }
    vector.shrinkToFit();
    return vector;
}

template<typename MapFunctionType> Vector<typename std::invoke_result_t<MapFunctionType, id>::value_type> makeVector(NSArray *array, MapFunctionType&& function)
{
    Vector<typename std::invoke_result_t<MapFunctionType, id>::value_type> vector;
    vector.reserveInitialCapacity(array.count);
    for (id element in array) {
        if (auto vectorElement = std::invoke(std::forward<MapFunctionType>(function), element))
            vector.append(WTFMove(*vectorElement));
    }
    vector.shrinkToFit();
    return vector;
}

inline Vector<uint8_t> vectorFromNSData(NSData* data)
{
    return { reinterpret_cast<const uint8_t*>(data.bytes), data.length };
}

} // namespace WTF

using WTF::createNSArray;
using WTF::makeVector;
using WTF::vectorFromNSData;
