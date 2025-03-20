/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

enum class ListOptimization : bool { None, SingleValue };

struct ListBounds {
    unsigned min;
    unsigned max;

    constexpr static ListBounds minimumOf(unsigned min)
    {
        return ListBounds { min, std::numeric_limits<unsigned>::max() };
    }

    constexpr static ListBounds exactly(unsigned value)
    {
        return ListBounds { value, value };
    }
};

inline constexpr auto ZeroOrMore = ListBounds::minimumOf(0);
inline constexpr auto OneOrMore = ListBounds::minimumOf(1);

template<char separator, ListBounds bounds, typename SubConsumer, typename... Args>
auto consumeListSeparatedByIntoBuilder(CSSParserTokenRange& range, SubConsumer&& subConsumer, Args&&... args) -> std::optional<CSSValueListBuilder>
{
    auto consumeSeparator = [](auto& range) {
        if constexpr (separator == ',')
            return consumeCommaIncludingWhitespace(range);
        else if constexpr (separator == '/')
            return consumeSlashIncludingWhitespace(range);
        else if constexpr (separator == ' ')
            return !range.atEnd();
    };

    CSSValueListBuilder list;
    do {
        auto value = std::invoke(subConsumer, range, args...);
        if (!value) {
            if constexpr (separator == ',')
                return { };
            else if constexpr (separator == '/')
                return { };
            else if constexpr (separator == ' ')
                break;
        }
        list.append(value.releaseNonNull());
    } while (consumeSeparator(range));

    if constexpr (bounds.min > 0) {
        if (list.size() < bounds.min)
            return { };
    }
    if constexpr (bounds.max < std::numeric_limits<unsigned>::max()) {
        if (list.size() > bounds.max)
            return { };
    }

    return { WTFMove(list) };
}

template<char separator, ListBounds bounds, ListOptimization optimization = ListOptimization::None, typename ListType = CSSValueList, typename SubConsumer, typename... Args>
auto consumeListSeparatedBy(CSSParserTokenRange& range, SubConsumer&& subConsumer, Args&&... args) -> std::conditional_t<optimization == ListOptimization::None, RefPtr<ListType>, RefPtr<CSSValue>>
{
    auto list = consumeListSeparatedByIntoBuilder<separator, bounds>(range, std::forward<SubConsumer>(subConsumer), std::forward<Args>(args)...);
    if (!list)
        return nullptr;

    if constexpr (optimization == ListOptimization::SingleValue) {
        if (list->size() == 1)
            return WTFMove((*list)[0]);
    }

    if constexpr (std::is_same_v<ListType, CSSValueList>) {
        if constexpr (separator == ',')
            return CSSValueList::createCommaSeparated(WTFMove(*list));
        else if constexpr (separator == '/')
            return CSSValueList::createSlashSeparated(WTFMove(*list));
        else if constexpr (separator == ' ')
            return CSSValueList::createSpaceSeparated(WTFMove(*list));
    } else {
        return ListType::create(WTFMove(*list));
    }
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
