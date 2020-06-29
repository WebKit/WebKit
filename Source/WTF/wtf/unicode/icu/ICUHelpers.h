/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <tuple>
#include <unicode/utypes.h>
#include <wtf/Forward.h>
#include <wtf/FunctionTraits.h>

namespace WTF {

constexpr bool needsToGrowToProduceBuffer(UErrorCode);
constexpr bool needsToGrowToProduceCString(UErrorCode);

// Use this to call a function from ICU that has the following properties:
// - Takes a buffer pointer and capacity.
// - Returns the length of the buffer needed.
// - Takes a UErrorCode* as its last argument, returning the status, including U_BUFFER_OVERFLOW_ERROR.
// Pass the arguments, but pass a Vector in place of the buffer pointer and capacity, and don't pass a UErrorCode*.
// This will call the function, once or twice as needed, resizing the buffer as needed.
//
// Example:
//
//    Vector<UChar, 32> buffer;
//    auto status = callBufferProducingFunction(ucal_getDefaultTimeZone, buffer);
//
template<typename FunctionType, typename ...ArgumentTypes> UErrorCode callBufferProducingFunction(const FunctionType&, ArgumentTypes&&...);

// Implementations of the functions declared above.

constexpr bool needsToGrowToProduceBuffer(UErrorCode errorCode)
{
    return errorCode == U_BUFFER_OVERFLOW_ERROR;
}

constexpr bool needsToGrowToProduceCString(UErrorCode errorCode)
{
    return needsToGrowToProduceBuffer(errorCode) || errorCode == U_STRING_NOT_TERMINATED_WARNING;
}

namespace CallBufferProducingFunction {

template<typename CharacterType, size_t inlineCapacity, typename ...ArgumentTypes> auto& findVector(Vector<CharacterType, inlineCapacity>& buffer, ArgumentTypes&&...)
{
    return buffer;
}

template<typename FirstArgumentType, typename ...ArgumentTypes> auto& findVector(FirstArgumentType&&, ArgumentTypes&&... arguments)
{
    return findVector(std::forward<ArgumentTypes>(arguments)...);
}

constexpr std::tuple<> argumentTuple() { return { }; }

template<typename FirstArgumentType, typename ...OtherArgumentTypes> auto argumentTuple(FirstArgumentType&&, OtherArgumentTypes&&...);

template<typename CharacterType, size_t inlineCapacity, typename ...OtherArgumentTypes> auto argumentTuple(Vector<CharacterType, inlineCapacity>& buffer, OtherArgumentTypes&&... otherArguments)
{
    return tuple_cat(std::make_tuple(buffer.data(), buffer.size()), argumentTuple(std::forward<OtherArgumentTypes>(otherArguments)...));
}

template<typename FirstArgumentType, typename ...OtherArgumentTypes> auto argumentTuple(FirstArgumentType&& firstArgument, OtherArgumentTypes&&... otherArguments)
{
    // This technique of building a tuple and passing it twice does not work well for complex types, so assert this is a relatively simple one.
    static_assert(std::is_trivial_v<std::remove_reference_t<FirstArgumentType>>);
    return tuple_cat(std::make_tuple(firstArgument), argumentTuple(std::forward<OtherArgumentTypes>(otherArguments)...));
}

}

template<typename FunctionType, typename ...ArgumentTypes> UErrorCode callBufferProducingFunction(const FunctionType& function, ArgumentTypes&&... arguments)
{
    auto& buffer = CallBufferProducingFunction::findVector(std::forward<ArgumentTypes>(arguments)...);
    buffer.grow(buffer.capacity());
    auto status = U_ZERO_ERROR;
    auto resultLength = apply(function, CallBufferProducingFunction::argumentTuple(std::forward<ArgumentTypes>(arguments)..., &status));
    if (U_SUCCESS(status))
        buffer.shrink(resultLength);
    else if (needsToGrowToProduceBuffer(status)) {
        status = U_ZERO_ERROR;
        buffer.grow(resultLength);
        apply(function, CallBufferProducingFunction::argumentTuple(std::forward<ArgumentTypes>(arguments)..., &status));
        ASSERT(U_SUCCESS(status));
    }
    return status;
}

template<auto deleteFunction>
struct ICUDeleter {
    void operator()(typename FunctionTraits<decltype(deleteFunction)>::template ArgumentType<0> pointer)
    {
        if (pointer)
            deleteFunction(pointer);
    }
};

namespace ICU {

WTF_EXPORT_PRIVATE unsigned majorVersion();
WTF_EXPORT_PRIVATE unsigned minorVersion();

} // namespace ICU
} // namespace WTF

using WTF::callBufferProducingFunction;
using WTF::needsToGrowToProduceCString;
using WTF::needsToGrowToProduceBuffer;
using WTF::ICUDeleter;
