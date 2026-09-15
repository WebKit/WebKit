/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "CSSCalcSizeValue.h"
#include "StyleCalcSizeValue.h"
#include "StyleSizeOrKeyword.h"
#include "StyleUnevaluatedCalcSize.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Rebuilding the CSS side keeps serialization in step with the parser, and lets serialization and
// CSSValue creation share one path.
CSS::CalcSizeFunction toCSSCalcSizeFunction(const CalcSizeValue&);

template<> struct Serialize<UnevaluatedCalcSize> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const ComputedStyle&, const UnevaluatedCalcSize& value)
    {
        CSS::serializationForCSS(builder, context, toCSSCalcSizeFunction(protect(value.calcSize())));
    }
};

template<> struct CSSValueCreation<UnevaluatedCalcSize> {
    Ref<CSSValue> operator()(CSSValuePool&, const ComputedStyle&, const UnevaluatedCalcSize& value)
    {
        return CSSCalcSizeValue::create(toCSSCalcSizeFunction(protect(value.calcSize())));
    }
};

// The computed value keeps the function whatever the basis, but switchOn() reports a keyword basis as
// the keyword for layout's benefit. Intercept the calc-size() alternative before the generic
// variant-like path sees it.
template<SizeOrKeywordDerived StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const ComputedStyle& style, const StyleType& value, Rest&&... rest)
    {
        if (value.isCalcSize()) {
            Serialize<UnevaluatedCalcSize> { }(builder, context, style, value.template get<UnevaluatedCalcSize>());
            return;
        }
        serializationForCSSOnVariantLike(builder, context, style, value, std::forward<Rest>(rest)...);
    }
};

template<SizeOrKeywordDerived StyleType> struct CSSValueCreation<StyleType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const ComputedStyle& style, const StyleType& value, Rest&&... rest)
    {
        if (value.isCalcSize())
            return CSSValueCreation<UnevaluatedCalcSize> { }(pool, style, value.template get<UnevaluatedCalcSize>());
        return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> {
            return createCSSValue(pool, style, alternative, std::forward<Rest>(rest)...);
        });
    }
};

} // namespace Style
} // namespace WebCore
