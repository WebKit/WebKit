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

// MARK: - Hashing

template<RawNumeric RawType> struct Hash<RawType> {
    inline void operator()(Hasher& hasher, const RawType& value)
    {
        WTF::add(hasher, value.type);
        WTF::add(hasher, value.value);
    }
};

template<RawNumeric RawType> struct Hash<UnevaluatedCalc<RawType>> {
    void operator()(Hasher& hasher, const UnevaluatedCalc<RawType>& calc)
    {
        WTF::add(hasher, calc.protectedCalc().ptr());
    }
};

template<RawNumeric RawType> struct Hash<PrimitiveNumeric<RawType>> {
    inline void operator()(Hasher& hasher, const PrimitiveNumeric<RawType>& value)
    {
        WTF::switchOn(value, [&](const auto& value) { hash(hasher, value); });
    }
};

template<> struct Hash<NumberOrPercentageResolvedToNumber> {
    void operator()(Hasher& hasher, const NumberOrPercentageResolvedToNumber& value)
    {
        hash(hasher, value.value);
    }
};
template<> struct Hash<SymbolRaw> {
    void operator()(Hasher& hasher, const SymbolRaw& value)
    {
        WTF::add(hasher, value.value);
    }
};
template<> struct Hash<Symbol> {
    void operator()(Hasher& hasher, const Symbol& value)
    {
        WTF::add(hasher, value.value);
    }
};

} // namespace CSS
} // namespace WebCore
