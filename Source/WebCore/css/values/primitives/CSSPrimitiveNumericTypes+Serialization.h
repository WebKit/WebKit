/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// MARK: - Serialization

// Type-erased helper to allow for shared code.
void rawNumericSerialization(StringBuilder&, double, CSSUnitType);

template<RawNumeric RawType> struct Serialize<RawType> {
    void operator()(StringBuilder& builder, const RawType& value)
    {
        rawNumericSerialization(builder, value.value, value.type);
    }
};

template<RawNumeric RawType> struct Serialize<PrimitiveNumeric<RawType>> {
    void operator()(StringBuilder& builder, const PrimitiveNumeric<RawType>& value)
    {
        WTF::switchOn(value, [&](const auto& value) { serializationForCSS(builder, value); });
    }
};

template<> struct Serialize<NumberOrPercentageResolvedToNumber> { void operator()(StringBuilder&, const NumberOrPercentageResolvedToNumber&); };
template<> struct Serialize<SymbolRaw> { void operator()(StringBuilder&, const SymbolRaw&); };
template<> struct Serialize<Symbol> { void operator()(StringBuilder&, const Symbol&); };

} // namespace CSS
} // namespace WebCore
