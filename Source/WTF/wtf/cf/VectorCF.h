/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#if USE(CF)

#include <wtf/CheckedArithmetic.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template <typename A, typename... B>
struct ParameterTypeTraits {
    using firstParamType = A;
};

// Derive return type and parameters from lamba using decltype(&LambdaType::operator()).
// See: <http://coliru.stacked-crooked.com/a/da415f1f536b1a31>
// Via: <https://stackoverflow.com/questions/33222673/c11-template-programming-delegates-and-lambdas>

template <typename LambdaType>
struct LambdaTypeTraits : LambdaTypeTraits<decltype(&LambdaType::operator())> { };

template <typename R, typename C, typename... Params>
struct LambdaTypeTraits<R(C::*)(Params...)> {
    using firstParamType = typename ParameterTypeTraits<Params...>::firstParamType;
    using returnType = R;

    static constexpr size_t paramCount = sizeof...(Params);
};

template <typename R, typename C, typename... Params>
struct LambdaTypeTraits<R(C::*)(Params...) const> {
    using firstParamType = typename ParameterTypeTraits<Params...>::firstParamType;
    using returnType = R;

    static constexpr size_t paramCount = sizeof...(Params);
};

// Specialize the behavior of these functions by overloading the makeCFArrayElement
// functions and makeVectorElement functions. The makeCFArrayElement function takes
// a const& to a collection element and can return either a RetainPtr<CFTypeRef> or a CFTypeRef
// if the value is autoreleased. The makeVectorElement function takes an ignored
// pointer to the vector element type, making argument-dependent lookup work, and a
// CFTypeRef for the array element, and returns an std::optional<T> of the the vector element,
// allowing us to filter out array elements that are not of the expected type.
//
//    RetainPtr<CFTypeRef> makeCFArrayElement(const CollectionElementType& collectionElement);
//        -or-
//    CFTypeRef makeCFArrayElement(const VectorElementType& vectorElement);
//
//    std::optional<VectorElementType> makeVectorElement(const VectorElementType*, CFTypeRef arrayElement);
//
// Note that a specific CF type may be used in place of the generic CFTypeRef above.

template<typename CollectionType> RetainPtr<CFMutableArrayRef> createCFArray(CollectionType&&);
template<typename VectorElementType, typename CFType> Vector<VectorElementType> makeVector(CFArrayRef);

// Allow for abbreviated specializations like makeVector<String>(CFArrayRef) containing CFStringRef objects.
template<typename VectorElementType> Vector<VectorElementType> makeVector(CFArrayRef);
template<> inline Vector<String> makeVector<String>(CFArrayRef array) { return makeVector<String, CFStringRef>(array); }

// This overload of createCFArray takes a function to map each vector element to an CF object.
// The map function has the same interface as the makeCFArrayElement function above, but can be any
// function including a lambda, a function-like object, or Function<>.
template<typename CollectionType, typename MapFunctionType> RetainPtr<CFMutableArrayRef> createCFArray(CollectionType&&, MapFunctionType&&);

// This overload of makeVector takes a function to map each CF object to a vector element.
// Currently, the map function needs to return a std::optional<T>.
template<typename MapLambdaType> Vector<typename LambdaTypeTraits<MapLambdaType>::returnType::value_type> makeVector(CFArrayRef, MapLambdaType&&);

// Implementation details of the function templates above.

inline void addUnlessNil(CFMutableArrayRef array, CFTypeRef value)
{
    if (value)
        CFArrayAppendValue(array, value);
}

// Conversion function declarations must appear before use. See also WTFString.h.
RetainPtr<CFNumberRef> makeCFArrayElement(const float&);
std::optional<float> makeVectorElement(const float*, CFNumberRef);

template<typename CollectionType> RetainPtr<CFMutableArrayRef> createCFArray(CollectionType&& collection)
{
    auto array = adoptCF(CFArrayCreateMutable(nullptr, Checked<CFIndex>(std::size(collection)), &kCFTypeArrayCallBacks));
    for (auto&& element : collection)
        addUnlessNil(array.get(), getPtr(makeCFArrayElement(std::forward<decltype(element)>(element))));
    return array;
}

template<typename CollectionType, typename MapFunctionType> RetainPtr<CFMutableArrayRef> createCFArray(CollectionType&& collection, MapFunctionType&& function)
{
    auto array = adoptCF(CFArrayCreateMutable(nullptr, Checked<CFIndex>(std::size(collection)), &kCFTypeArrayCallBacks));
    for (auto&& element : collection)
        addUnlessNil(array.get(), getPtr(std::invoke(std::forward<MapFunctionType>(function), std::forward<decltype(element)>(element))));
    return array;
}

template<typename VectorElementType, typename CFType> Vector<VectorElementType> makeVector(CFArrayRef array)
{
    Vector<VectorElementType> vector;
    CFIndex count = CFArrayGetCount(array);
    vector.reserveInitialCapacity(count);
    for (CFIndex i = 0; i < count; ++i) {
        constexpr const VectorElementType* typedNull = nullptr;
        if (CFType element = dynamic_cf_cast<CFType>(CFArrayGetValueAtIndex(array, i))) {
            if (auto vectorElement = makeVectorElement(typedNull, element))
                vector.uncheckedAppend(WTFMove(*vectorElement));
        }
    }
    vector.shrinkToFit();
    return vector;
}

template<typename MapLambdaType> Vector<typename LambdaTypeTraits<MapLambdaType>::returnType::value_type> makeVector(CFArrayRef array, MapLambdaType&& lambda)
{
    using ElementType = typename LambdaTypeTraits<MapLambdaType>::returnType::value_type;
    using CFType = typename LambdaTypeTraits<MapLambdaType>::firstParamType;

    static_assert(std::is_same_v<typename LambdaTypeTraits<MapLambdaType>::returnType, std::optional<ElementType>>, "MapLambdaType returns std::optional<ElementType>");
    static_assert(LambdaTypeTraits<MapLambdaType>::paramCount == 1, "MapLambdaType takes a single CFTypeRef argument");

    Vector<ElementType> vector;
    CFIndex count = CFArrayGetCount(array);
    vector.reserveInitialCapacity(count);
    for (CFIndex i = 0; i < count; ++i) {
        if (CFType element = dynamic_cf_cast<CFType>(CFArrayGetValueAtIndex(array, i))) {
            if (auto vectorElement = std::invoke(std::forward<MapLambdaType>(lambda), element))
                vector.uncheckedAppend(WTFMove(*vectorElement));
        }
    }
    vector.shrinkToFit();
    return vector;
}

inline Vector<uint8_t> vectorFromCFData(CFDataRef data)
{
    return { reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(data)), Checked<size_t>(CFDataGetLength(data)) };
}

// Conversion function implementations. See also StringCF.cpp.

inline RetainPtr<CFNumberRef> makeCFArrayElement(const float& number)
{
    return adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &number));
}

inline std::optional<float> makeVectorElement(const float*, CFNumberRef cfNumber)
{
    float number = 0;
    CFNumberGetValue(cfNumber, kCFNumberFloatType, &number);
    return { number };
}

} // namespace WTF

using WTF::createCFArray;
using WTF::makeVector;
using WTF::vectorFromCFData;

#endif // USE(CF)
