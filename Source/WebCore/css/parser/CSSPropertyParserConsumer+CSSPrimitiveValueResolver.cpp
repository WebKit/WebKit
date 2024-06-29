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
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+NoneDefinitions.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+SymbolDefinitions.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(AngleRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, value.type);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(LengthRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, value.type);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(NumberRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, CSSUnitType::CSS_NUMBER);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(PercentRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, CSSUnitType::CSS_PERCENTAGE);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(ResolutionRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, value.type);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(TimeRaw value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(value.value, value.type);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(NoneRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
{
    return CSSPrimitiveValue::create(std::numeric_limits<double>::quiet_NaN(), CSSUnitType::CSS_NUMBER);
}

RefPtr<CSSPrimitiveValue> CSSPrimitiveValueResolverBase::resolve(SymbolRaw value, const CSSCalcSymbolTable& symbolTable, CSSPropertyParserOptions)
{
    if (auto variable = symbolTable.get(value.value))
        return CSSPrimitiveValue::create(variable->value, variable->type);

    // We should only get here if the symbol was previously looked up in the symbol table.
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
