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
#include "CSSOMColorValue.h"

#include "CSSKeywordValue.h"
#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "ExceptionOr.h"

namespace WebCore {

RefPtr<CSSKeywordValue> CSSOMColorValue::colorSpace()
{
    // FIXME: implement this.
    return nullptr;
}

RefPtr<CSSOMColorValue> CSSOMColorValue::to(CSSKeywordish)
{
    // FIXME: implement this.
    return nullptr;
}

std::optional<Variant<Ref<CSSOMColorValue>, Ref<CSSStyleValue>>> CSSOMColorValue::parse(const String&)
{
    // FIXME: implement this.
    return std::nullopt;
}

// https://drafts.css-houdini.org/css-typed-om-1/#rectify-a-csscolorpercent
ExceptionOr<RectifiedCSSColorPercent> CSSOMColorValue::rectifyCSSColorPercent(CSSColorPercent&& colorPercent)
{
    return switchOn(WTF::move(colorPercent),
        [](double value) -> ExceptionOr<RectifiedCSSColorPercent> {
            return { Ref<CSSNumericValue> { CSSUnitValue::create(value * 100, CSSUnitType::CSS_PERCENTAGE) } };
        },
        [](Ref<CSSNumericValue>&& numericValue) -> ExceptionOr<RectifiedCSSColorPercent> {
            if (numericValue->type().matches<CSSNumericBaseType::Percent>())
                return { WTF::move(numericValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorPercent"_s };
        },
        [](String&& string) -> ExceptionOr<RectifiedCSSColorPercent> {
            return { CSSKeywordValue::rectifyKeywordish(WTF::move(string)) };
        },
        [](Ref<CSSKeywordValue>&& keywordValue) -> ExceptionOr<RectifiedCSSColorPercent> {
            if (equalIgnoringASCIICase(keywordValue->value(), "none"_s))
                return { WTF::move(keywordValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorPercent"_s };
        }
    );
}

// https://drafts.css-houdini.org/css-typed-om/#rectify-a-csscolorangle
ExceptionOr<RectifiedCSSColorAngle> CSSOMColorValue::rectifyCSSColorAngle(CSSColorAngle&& colorAngle)
{
    return switchOn(WTF::move(colorAngle),
        [](double value) -> ExceptionOr<RectifiedCSSColorAngle> {
            return { Ref<CSSNumericValue> { CSSUnitValue::create(value, CSSUnitType::CSS_DEG) } };
        },
        [](Ref<CSSNumericValue>&& numericValue) -> ExceptionOr<RectifiedCSSColorAngle> {
            if (numericValue->type().matches<CSSNumericBaseType::Angle>())
                return { WTF::move(numericValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorAngle"_s };
        },
        [](String&& string) -> ExceptionOr<RectifiedCSSColorAngle> {
            return { CSSKeywordValue::rectifyKeywordish(WTF::move(string)) };
        },
        [](Ref<CSSKeywordValue>&& keywordValue) -> ExceptionOr<RectifiedCSSColorAngle> {
            if (equalIgnoringASCIICase(keywordValue->value(), "none"_s))
                return { WTF::move(keywordValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorAngle"_s };
        }
    );
}

// https://drafts.css-houdini.org/css-typed-om/#rectify-a-csscolornumber
ExceptionOr<RectifiedCSSColorNumber> CSSOMColorValue::rectifyCSSColorNumber(CSSColorNumber&& colorNumber)
{
    return switchOn(WTF::move(colorNumber),
        [](double value) -> ExceptionOr<RectifiedCSSColorNumber> {
            return { Ref<CSSNumericValue> { CSSUnitValue::create(value, CSSUnitType::CSS_NUMBER) } };
        },
        [](Ref<CSSNumericValue>&& numericValue) -> ExceptionOr<RectifiedCSSColorNumber> {
            if (numericValue->type().matchesNumber())
                return { WTF::move(numericValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorNumber"_s };
        },
        [](String&& string) -> ExceptionOr<RectifiedCSSColorNumber> {
            return { CSSKeywordValue::rectifyKeywordish(WTF::move(string)) };
        },
        [](Ref<CSSKeywordValue>&& keywordValue) -> ExceptionOr<RectifiedCSSColorNumber> {
            if (equalIgnoringASCIICase(keywordValue->value(), "none"_s))
                return { WTF::move(keywordValue) };
            return Exception { ExceptionCode::SyntaxError, "Invalid CSSColorNumber"_s };
        }
    );
}

CSSColorPercent CSSOMColorValue::toCSSColorPercent(const RectifiedCSSColorPercent& component)
{
    return switchOn(component,
        [](const Ref<CSSKeywordValue>& keywordValue) -> CSSColorPercent {
            return keywordValue;
        },
        [](const Ref<CSSNumericValue>& numericValue) -> CSSColorPercent {
            return numericValue;
        }
    );
}

CSSColorPercent CSSOMColorValue::toCSSColorPercent(const CSSNumberish& numberish)
{
    return switchOn(numberish,
        [](double number) -> CSSColorPercent {
            return number;
        },
        [](const Ref<CSSNumericValue>& numericValue) -> CSSColorPercent {
            return numericValue;
        }
    );
}

CSSColorAngle CSSOMColorValue::toCSSColorAngle(const RectifiedCSSColorAngle& angle)
{
    return switchOn(angle,
        [](const Ref<CSSKeywordValue>& keywordValue) -> CSSColorAngle {
            return keywordValue;
        },
        [](const Ref<CSSNumericValue>& numericValue) -> CSSColorAngle {
            return numericValue;
        }
    );
}

CSSColorNumber CSSOMColorValue::toCSSColorNumber(const RectifiedCSSColorNumber& number)
{
    return switchOn(number,
        [](const Ref<CSSKeywordValue>& keywordValue) -> CSSColorNumber {
            return keywordValue;
        },
        [](const Ref<CSSNumericValue>& numericValue) -> CSSColorNumber {
            return numericValue;
        }
    );
}

RefPtr<CSSValue> CSSOMColorValue::toCSSValue() const
{
    // FIXME: Implement this.
    return nullptr;
}

} // namespace WebCore
