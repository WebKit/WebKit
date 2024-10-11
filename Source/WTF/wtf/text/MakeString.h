/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include <wtf/CheckedArithmetic.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringView.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

template<typename StringTypeAdapter> constexpr bool makeStringSlowPathRequiredForAdapter = requires(const StringTypeAdapter& adapter) {
    { adapter.writeUsing(std::declval<StringBuilder&>) } -> std::same_as<void>;
};
template<typename... StringTypeAdapters> constexpr bool makeStringSlowPathRequired = (... || makeStringSlowPathRequiredForAdapter<StringTypeAdapters>);

template<typename... StringTypeAdapters>
RefPtr<StringImpl> tryMakeStringImplFromAdaptersInternal(unsigned length, bool areAllAdapters8Bit, StringTypeAdapters... adapters)
{
    ASSERT(length <= String::MaxLength);
    if (areAllAdapters8Bit) {
        LChar* buffer;
        RefPtr result = StringImpl::tryCreateUninitialized(length, buffer);
        if (!result)
            return nullptr;

        if (buffer)
            stringTypeAdapterAccumulator(buffer, adapters...);

        return result;
    }

    UChar* buffer;
    RefPtr result = StringImpl::tryCreateUninitialized(length, buffer);
    if (!result)
        return nullptr;

    if (buffer)
        stringTypeAdapterAccumulator(buffer, adapters...);

    return result;
}

template<typename... StringTypeAdapters>
String tryMakeStringFromAdapters(StringTypeAdapters&&... adapters)
{
    static_assert(String::MaxLength == std::numeric_limits<int32_t>::max());

    if constexpr (makeStringSlowPathRequired<StringTypeAdapters...>) {
        StringBuilder builder;
        builder.appendFromAdapters(std::forward<StringTypeAdapters>(adapters)...);
        return builder.toString();
    } else {
        auto sum = checkedSum<int32_t>(adapters.length()...);
        if (sum.hasOverflowed())
            return String();

        bool areAllAdapters8Bit = are8Bit(adapters...);
        return tryMakeStringImplFromAdaptersInternal(sum, areAllAdapters8Bit, adapters...);
    }
}

template<StringTypeAdaptable... StringTypes>
String tryMakeString(const StringTypes& ...strings)
{
    return tryMakeStringFromAdapters(StringTypeAdapter<StringTypes>(strings)...);
}

template<StringTypeAdaptable... StringTypes>
String makeString(StringTypes... strings)
{
    auto result = tryMakeString(strings...);
    if (!result)
        CRASH();
    return result;
}

template<typename... StringTypeAdapters>
AtomString tryMakeAtomStringFromAdapters(StringTypeAdapters ...adapters)
{
    static_assert(String::MaxLength == std::numeric_limits<int32_t>::max());

    if constexpr (makeStringSlowPathRequired<StringTypeAdapters...>) {
        StringBuilder builder;
        builder.appendFromAdapters(adapters...);
        return builder.toAtomString();
    } else {
        auto sum = checkedSum<int32_t>(adapters.length()...);
        if (sum.hasOverflowed())
            return AtomString();

        unsigned length = sum;
        ASSERT(length <= String::MaxLength);

        bool areAllAdapters8Bit = are8Bit(adapters...);
        constexpr size_t maxLengthToUseStackVariable = 64;
        if (length < maxLengthToUseStackVariable) {
            if (areAllAdapters8Bit) {
                LChar buffer[maxLengthToUseStackVariable];
                stringTypeAdapterAccumulator(buffer, adapters...);
                return std::span<const LChar> { buffer, length };
            }
            UChar buffer[maxLengthToUseStackVariable];
            stringTypeAdapterAccumulator(buffer, adapters...);
            return std::span<const UChar> { buffer, length };
        }
        return tryMakeStringImplFromAdaptersInternal(length, areAllAdapters8Bit, adapters...).get();
    }
}

template<StringTypeAdaptable... StringTypes>
AtomString tryMakeAtomString(StringTypes... strings)
{
    return tryMakeAtomStringFromAdapters(StringTypeAdapter<StringTypes>(strings)...);
}

template<StringTypeAdaptable... StringTypes>
AtomString makeAtomString(StringTypes... strings)
{
    auto result = tryMakeAtomString(strings...);
    if (result.isNull())
        CRASH();
    return result;
}

inline String WARN_UNUSED_RETURN makeStringByInserting(StringView originalString, StringView stringToInsert, unsigned position)
{
    return makeString(originalString.left(position), stringToInsert, originalString.substring(position));
}

// Helper functor useful in generic contexts where both makeString() and StringBuilder are being used.
struct SerializeUsingMakeString {
    using Result = String;
    template<typename... T> String operator()(T&&... args)
    {
        return makeString(std::forward<T>(args)...);
    }
};

} // namespace WTF

using WTF::makeAtomString;
using WTF::makeString;
using WTF::makeStringByInserting;
using WTF::tryMakeAtomString;
using WTF::tryMakeString;
using WTF::SerializeUsingMakeString;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
