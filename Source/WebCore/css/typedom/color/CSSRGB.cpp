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
#include "CSSRGB.h"

#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSRGB);

static CSSColorRGBComp toCSSColorRGBComp(const RectifiedCSSColorRGBComp& component)
{
    return switchOn(component, [](const RefPtr<CSSKeywordValue>& keywordValue) -> CSSColorRGBComp {
        return keywordValue;
    }, [](const RefPtr<CSSNumericValue>& numericValue) -> CSSColorRGBComp {
        return numericValue;
    });
}

ExceptionOr<Ref<CSSRGB>> CSSRGB::create(CSSColorRGBComp&& red, CSSColorRGBComp&& green, CSSColorRGBComp&& blue, CSSColorPercent&& alpha)
{
    auto rectifiedRed = rectifyCSSColorRGBComp(WTFMove(red));
    if (rectifiedRed.hasException())
        return rectifiedRed.releaseException();
    auto rectifiedGreen = rectifyCSSColorRGBComp(WTFMove(green));
    if (rectifiedGreen.hasException())
        return rectifiedGreen.releaseException();
    auto rectifiedBlue = rectifyCSSColorRGBComp(WTFMove(blue));
    if (rectifiedBlue.hasException())
        return rectifiedBlue.releaseException();
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();

    return adoptRef(*new CSSRGB(rectifiedRed.releaseReturnValue(), rectifiedGreen.releaseReturnValue(), rectifiedBlue.releaseReturnValue(), rectifiedAlpha.releaseReturnValue()));
}

CSSRGB::CSSRGB(RectifiedCSSColorRGBComp&& red, RectifiedCSSColorRGBComp&& green, RectifiedCSSColorRGBComp&& blue, RectifiedCSSColorPercent&& alpha)
    : m_red(WTFMove(red))
    , m_green(WTFMove(green))
    , m_blue(WTFMove(blue))
    , m_alpha(WTFMove(alpha))
{
}

CSSColorRGBComp CSSRGB::r() const
{
    return toCSSColorRGBComp(m_red);
}

ExceptionOr<void> CSSRGB::setR(CSSColorRGBComp&& red)
{
    auto rectifiedRed = rectifyCSSColorRGBComp(WTFMove(red));
    if (rectifiedRed.hasException())
        return rectifiedRed.releaseException();
    m_red = rectifiedRed.releaseReturnValue();
    return { };
}

CSSColorRGBComp CSSRGB::g() const
{
    return toCSSColorRGBComp(m_green);
}

ExceptionOr<void> CSSRGB::setG(CSSColorRGBComp&& green)
{
    auto rectifiedGreen = rectifyCSSColorRGBComp(WTFMove(green));
    if (rectifiedGreen.hasException())
        return rectifiedGreen.releaseException();
    m_green = rectifiedGreen.releaseReturnValue();
    return { };
}

CSSColorRGBComp CSSRGB::b() const
{
    return toCSSColorRGBComp(m_blue);
}

ExceptionOr<void> CSSRGB::setB(CSSColorRGBComp&& blue)
{
    auto rectifiedBlue = rectifyCSSColorRGBComp(WTFMove(blue));
    if (rectifiedBlue.hasException())
        return rectifiedBlue.releaseException();
    m_blue = rectifiedBlue.releaseReturnValue();
    return { };
}

CSSColorPercent CSSRGB::alpha() const
{
    return toCSSColorPercent(m_alpha);
}

ExceptionOr<void> CSSRGB::setAlpha(CSSColorPercent&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = rectifiedAlpha.releaseReturnValue();
    return { };
}

// https://drafts.css-houdini.org/css-typed-om-1/#rectify-a-csscolorrgbcomp
ExceptionOr<RectifiedCSSColorRGBComp> CSSRGB::rectifyCSSColorRGBComp(CSSColorRGBComp&& component)
{
    return switchOn(WTFMove(component), [](double value) -> ExceptionOr<RectifiedCSSColorRGBComp> {
        return { RefPtr<CSSNumericValue> { CSSUnitValue::create(value * 100, CSSUnitType::CSS_PERCENTAGE) } };
    }, [](RefPtr<CSSNumericValue>&& numericValue) -> ExceptionOr<RectifiedCSSColorRGBComp> {
        if (numericValue->type().matchesNumber() || numericValue->type().matches<CSSNumericBaseType::Percent>())
            return { WTFMove(numericValue) };
        return Exception { SyntaxError, "Invalid CSSColorRGBComp"_s };
    }, [](String&& string) -> ExceptionOr<RectifiedCSSColorRGBComp> {
        return { RefPtr<CSSKeywordValue> { CSSKeywordValue::rectifyKeywordish(WTFMove(string)) } };
    }, [](RefPtr<CSSKeywordValue>&& keywordValue) -> ExceptionOr<RectifiedCSSColorRGBComp> {
        if (equalIgnoringASCIICase(keywordValue->value(), "none"_s))
            return { WTFMove(keywordValue) };
        return Exception { SyntaxError, "Invalid CSSColorRGBComp"_s };
    });
}

} // namespace WebCore
