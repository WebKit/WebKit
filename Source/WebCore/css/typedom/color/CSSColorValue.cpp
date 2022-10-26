/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSColorValue.h"

#include "CSSKeywordValue.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "ExceptionOr.h"

namespace WebCore {

RefPtr<CSSKeywordValue> CSSColorValue::colorSpace()
{
    // FIXME: implement this.
    return nullptr;
}

RefPtr<CSSColorValue> CSSColorValue::to(CSSKeywordish)
{
    // FIXME: implement this.
    return nullptr;
}

std::variant<RefPtr<CSSColorValue>, RefPtr<CSSStyleValue>> CSSColorValue::parse(const String&)
{
    // FIXME: implement this.
    return RefPtr<CSSColorValue> { nullptr };
}

// https://drafts.css-houdini.org/css-typed-om-1/#rectify-a-csscolorpercent
ExceptionOr<RectifiedCSSColorPercent> CSSColorValue::rectifyCSSColorPercent(CSSColorPercent&& colorPercent)
{
    return switchOn(WTFMove(colorPercent), [](double value) -> ExceptionOr<RectifiedCSSColorPercent> {
        return { RefPtr<CSSNumericValue> { CSSUnitValue::create(value * 100, CSSUnitType::CSS_PERCENTAGE) } };
    }, [](RefPtr<CSSNumericValue>&& numericValue) -> ExceptionOr<RectifiedCSSColorPercent> {
        if (numericValue->type().matches<CSSNumericBaseType::Percent>())
            return { WTFMove(numericValue) };
        return Exception { SyntaxError, "Invalid CSSColorPercent"_s };
    }, [](String&& string) -> ExceptionOr<RectifiedCSSColorPercent> {
        return { RefPtr<CSSKeywordValue> { CSSKeywordValue::rectifyKeywordish(WTFMove(string)) } };
    }, [](RefPtr<CSSKeywordValue>&& keywordValue) -> ExceptionOr<RectifiedCSSColorPercent> {
        if (equalIgnoringASCIICase(keywordValue->value(), "none"_s))
            return { WTFMove(keywordValue) };
        return Exception { SyntaxError, "Invalid CSSColorPercent"_s };
    });
}

CSSColorPercent CSSColorValue::toCSSColorPercent(const RectifiedCSSColorPercent& component)
{
    return switchOn(component, [](const RefPtr<CSSKeywordValue>& keywordValue) -> CSSColorPercent {
        return keywordValue;
    }, [](const RefPtr<CSSNumericValue>& numericValue) -> CSSColorPercent {
        return numericValue;
    });
}

} // namespace WebCore
