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

#include "config.h"
#include "CSSPropertyParserConsumer+RawResolver.h"

#include "CSSCalcValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+NoneDefinitions.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<AngleRaw> RawResolverBase::resolve(AngleRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<LengthRaw> RawResolverBase::resolve(LengthRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<NumberRaw> RawResolverBase::resolve(NumberRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<PercentRaw> RawResolverBase::resolve(PercentRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<ResolutionRaw> RawResolverBase::resolve(ResolutionRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<TimeRaw> RawResolverBase::resolve(TimeRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<NoneRaw> RawResolverBase::resolve(NoneRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return value;
}

std::optional<NumberRaw> RawResolverBase::resolve(SymbolRaw value, const CSSCalcSymbolTable& symbolTable, CSSPropertyParserOptions)
{
    if (auto variable = symbolTable.get(value.value))
        return NumberRaw { variable->value };

    // We should only get here if the symbol was previously looked up in the symbol table.
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

std::optional<AngleRaw> RawResolverBase::resolve(UnevaluatedCalc<AngleRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(AngleRaw { value.calc->primitiveType(), value.calc->doubleValue() }, options);
}

std::optional<LengthRaw> RawResolverBase::resolve(UnevaluatedCalc<LengthRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(LengthRaw { value.calc->primitiveType(), value.calc->doubleValue() }, options);
}

std::optional<NumberRaw> RawResolverBase::resolve(UnevaluatedCalc<NumberRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(NumberRaw { value.calc->doubleValue() }, options);
}

std::optional<PercentRaw> RawResolverBase::resolve(UnevaluatedCalc<PercentRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(PercentRaw { value.calc->doubleValue() }, options);
}

std::optional<ResolutionRaw> RawResolverBase::resolve(UnevaluatedCalc<ResolutionRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(ResolutionRaw { value.calc->primitiveType(), value.calc->doubleValue() }, options);
}

std::optional<TimeRaw> RawResolverBase::resolve(UnevaluatedCalc<TimeRaw> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions options)
{
    return validatedRange(TimeRaw { value.calc->primitiveType(), value.calc->doubleValue() }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
