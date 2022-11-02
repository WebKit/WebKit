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
#include "CSSLab.h"

#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSLab);

ExceptionOr<Ref<CSSLab>> CSSLab::create(CSSColorPercent&& lightness, CSSColorNumber&& a, CSSColorNumber&& b, CSSColorPercent&& alpha)
{
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    auto rectifiedA = rectifyCSSColorNumber(WTFMove(a));
    if (rectifiedA.hasException())
        return rectifiedA.releaseException();
    auto rectifiedB = rectifyCSSColorNumber(WTFMove(b));
    if (rectifiedB.hasException())
        return rectifiedB.releaseException();
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();

    return adoptRef(*new CSSLab(rectifiedLightness.releaseReturnValue(), rectifiedA.releaseReturnValue(), rectifiedB.releaseReturnValue(), rectifiedAlpha.releaseReturnValue()));
}

CSSLab::CSSLab(RectifiedCSSColorPercent&& lightness, RectifiedCSSColorNumber&& a, RectifiedCSSColorNumber&& b, RectifiedCSSColorPercent&& alpha)
    : m_lightness(WTFMove(lightness))
    , m_a(WTFMove(a))
    , m_b(WTFMove(b))
    , m_alpha(WTFMove(alpha))
{
}

CSSColorPercent CSSLab::l() const
{
    return toCSSColorPercent(m_lightness);
}

ExceptionOr<void> CSSLab::setL(CSSColorPercent&& lightness)
{
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    m_lightness = rectifiedLightness.releaseReturnValue();
    return { };
}

CSSColorNumber CSSLab::a() const
{
    return toCSSColorNumber(m_a);
}

ExceptionOr<void> CSSLab::setA(CSSColorNumber&& a)
{
    auto rectifiedA = rectifyCSSColorNumber(WTFMove(a));
    if (rectifiedA.hasException())
        return rectifiedA.releaseException();
    m_a = rectifiedA.releaseReturnValue();
    return { };
}

CSSColorNumber CSSLab::b() const
{
    return toCSSColorNumber(m_b);
}

ExceptionOr<void> CSSLab::setB(CSSColorNumber&& b)
{
    auto rectifiedB = rectifyCSSColorNumber(WTFMove(b));
    if (rectifiedB.hasException())
        return rectifiedB.releaseException();
    m_b = rectifiedB.releaseReturnValue();
    return { };
}

CSSColorPercent CSSLab::alpha() const
{
    return toCSSColorPercent(m_alpha);
}

ExceptionOr<void> CSSLab::setAlpha(CSSColorPercent&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = rectifiedAlpha.releaseReturnValue();
    return { };
}

} // namespace WebCore
