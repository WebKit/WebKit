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

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserOptions.h"
#include <optional>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolTable;
struct CSSParserContext;

namespace CSSPropertyParserHelpers {

template<typename R, typename Base, typename T, typename... Ts>
struct MetaResolver : Base {
    using ResultType = R;

    static ResultType resolve(std::variant<T, Ts...>&& consumeResult, CSSPropertyParserOptions options) requires (sizeof...(Ts) > 0)
    {
        return WTF::switchOn(WTFMove(consumeResult), [&](auto&& value) -> ResultType {
            return Base::resolve(WTFMove(value), options);
        });
    }

    static ResultType resolve(T&& consumeResult, CSSPropertyParserOptions options) requires (sizeof...(Ts) == 0)
    {
        return Base::resolve(WTFMove(consumeResult), options);
    }

    static ResultType consumeAndResolve(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyParserOptions options)
    {
        auto result = MetaConsumer<T, Ts...>::consume(range, context, { }, options);
        if (!result)
            return { };
        return resolve(WTFMove(*result), options);
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
