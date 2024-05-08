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

#include "CSSPropertyParserConsumer+RawTypes.h"
#include <optional>
#include <variant>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - Meta Transformers

template<typename Transformer, typename RawType>
typename Transformer::Result transformRaw(RawType value)
{
    return Transformer::transform(value);
}

template<typename Parent, typename ResultType>
struct RawVariantTransformerBase {
    using Result = ResultType;

    template<typename... Types>
    static Result transform(const std::variant<Types...>& value)
    {
        return WTF::switchOn(value, [] (auto value) { return Parent::transform(value); });
    }

    static Result transform(Result value)
    {
        return value;
    }

    template<typename T>
    static Result transform(T value)
    {
        return Parent::transform(value);
    }
};

struct PercentOrNumberDividedBy100Transformer : RawVariantTransformerBase<PercentOrNumberDividedBy100Transformer, double> {
    using RawVariantTransformerBase<PercentOrNumberDividedBy100Transformer, double>::transform;

    static double transform(NumberRaw value)
    {
        return value.value;
    }

    static double transform(PercentRaw value)
    {
        return value.value / 100.0;
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
