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
#include "CSSLCH.h"

#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSLCH);

ExceptionOr<Ref<CSSLCH>> CSSLCH::create(CSSColorPercent&& lightness, CSSColorPercent&& chroma, CSSColorAngle&& hue, CSSColorPercent&& alpha)
{
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    auto rectifiedChroma = rectifyCSSColorPercent(WTFMove(chroma));
    if (rectifiedChroma.hasException())
        return rectifiedChroma.releaseException();
    auto rectifiedHue = rectifyCSSColorAngle(WTFMove(hue));
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();

    return adoptRef(*new CSSLCH(rectifiedLightness.releaseReturnValue(), rectifiedChroma.releaseReturnValue(), rectifiedHue.releaseReturnValue(), rectifiedAlpha.releaseReturnValue()));

}

CSSLCH::CSSLCH(RectifiedCSSColorPercent&& lightness, RectifiedCSSColorPercent&& chroma, RectifiedCSSColorAngle&& hue, RectifiedCSSColorPercent&& alpha)
    : m_lightness(WTFMove(lightness))
    , m_chroma(WTFMove(chroma))
    , m_hue(WTFMove(hue))
    , m_alpha(WTFMove(alpha))
{
}

CSSColorPercent CSSLCH::l() const
{
    return toCSSColorAngle(m_lightness);
}

ExceptionOr<void> CSSLCH::setL(CSSColorPercent&& lightness)
{
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    m_lightness = rectifiedLightness.releaseReturnValue();
    return { };
}

CSSColorPercent CSSLCH::c() const
{
    return toCSSColorAngle(m_chroma);
}

ExceptionOr<void> CSSLCH::setC(CSSColorPercent&& chroma)
{
    auto rectifiedChroma = rectifyCSSColorPercent(WTFMove(chroma));
    if (rectifiedChroma.hasException())
        return rectifiedChroma.releaseException();
    m_chroma = rectifiedChroma.releaseReturnValue();
    return { };
}

CSSColorAngle CSSLCH::h() const
{
    return toCSSColorAngle(m_hue);
}

ExceptionOr<void> CSSLCH::setH(CSSColorAngle&& hue)
{
    auto rectifiedHue = rectifyCSSColorAngle(WTFMove(hue));
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    m_hue = rectifiedHue.releaseReturnValue();
    return { };
}

CSSColorPercent CSSLCH::alpha() const
{
    return toCSSColorPercent(m_alpha);
}

ExceptionOr<void> CSSLCH::setAlpha(CSSColorPercent&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = rectifiedAlpha.releaseReturnValue();
    return { };
}

} // namespace WebCore
