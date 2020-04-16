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

namespace WTF {

// Specialize the behavior of these functions by overloading the makeNSArrayElement
// functions and makeVectorElement functions. The makeNSArrayElement function takes
// a const& to a vector element and can return either a RetainPtr<id> or an id
// if the value is autoreleased. The makeVectorElement function takes an ignored
// pointer to the vector element type, making argument-dependent lookup work, and an
// id for the array element, and returns an Optional<T> of the the vector element,
// allowing us to filter out array elements that are not of the expected type.
//
//    RetainPtr<id> makeNSArrayElement(const VectorElementType& vectorElement);
//        -or-
//    id makeNSArrayElement(const VectorElementType& vectorElement);
//
//    Optional<VectorElementType> makeVectorElement(const VectorElementType*, id arrayElement);

template<typename VectorType> RetainPtr<NSArray> createNSArray(const VectorType&);
template<typename VectorElementType> Vector<VectorElementType> makeVector(NSArray *);

// This overload of createNSArray takes a function to map each vector element to an Objective-C object.
// The map function has the same interface as the makeNSArrayElement function above, but can be any
// function including a lambda, a function-like object, or Function<>.
template<typename VectorType, typename MapFunctionType> RetainPtr<NSArray> createNSArray(const VectorType&, const MapFunctionType&);

// Implementation details of the function templates above.

template<typename VectorType> RetainPtr<NSArray> createNSArray(const VectorType& vector)
{
    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:vector.size()]);
    for (auto& element : vector)
        [array addObject:getPtr(makeNSArrayElement(element))];
    return array;
}

template<typename VectorType, typename MapFunctionType> RetainPtr<NSArray> createNSArray(const VectorType& vector, const MapFunctionType& function)
{
    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:vector.size()]);
    for (auto& element : vector)
        [array addObject:getPtr(function(element))];
    return array;
}

template<typename VectorElementType> Vector<VectorElementType> makeVector(NSArray *array)
{
    Vector<VectorElementType> vector;
    vector.reserveInitialCapacity(array.count);
    for (id element in array) {
        constexpr const VectorElementType* typedNull = nullptr;
        if (auto vectorElement = makeVectorElement(typedNull, element))
            vector.uncheckedAppend(WTFMove(*vectorElement));
    }
    vector.shrinkToFit();
    return vector;
}

} // namespace WTF

using WTF::makeVector;
