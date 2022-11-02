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
#include "CSSHSL.h"

#include "Exception.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSHSL);

ExceptionOr<Ref<CSSHSL>> CSSHSL::create(CSSColorAngle&& hue, CSSColorPercent&& saturation, CSSColorPercent&& lightness, CSSColorPercent&& alpha)
{
    auto rectifiedHue = rectifyCSSColorAngle(WTFMove(hue));
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    auto rectifiedSaturation = rectifyCSSColorPercent(WTFMove(saturation));
    if (rectifiedSaturation.hasException())
        return rectifiedSaturation.releaseException();
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    return adoptRef(*new CSSHSL(rectifiedHue.releaseReturnValue(), rectifiedSaturation.releaseReturnValue(), rectifiedLightness.releaseReturnValue(), rectifiedAlpha.releaseReturnValue()));
}

CSSHSL::CSSHSL(RectifiedCSSColorAngle&& hue, RectifiedCSSColorPercent&& saturation, RectifiedCSSColorPercent&& lightness, RectifiedCSSColorPercent&& alpha)
    : m_hue(WTFMove(hue))
    , m_saturation(WTFMove(saturation))
    , m_lightness(WTFMove(lightness))
    , m_alpha(WTFMove(alpha))
{
}

CSSColorAngle CSSHSL::h() const
{
    return toCSSColorAngle(m_hue);
}

ExceptionOr<void> CSSHSL::setH(CSSColorAngle&& hue)
{
    auto rectifiedHue = rectifyCSSColorAngle(WTFMove(hue));
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    m_hue = rectifiedHue.releaseReturnValue();
    return { };
}

CSSColorPercent CSSHSL::s() const
{
    return toCSSColorPercent(m_saturation);
}

ExceptionOr<void> CSSHSL::setS(CSSColorPercent&& saturation)
{
    auto rectifiedSaturation = rectifyCSSColorPercent(WTFMove(saturation));
    if (rectifiedSaturation.hasException())
        return rectifiedSaturation.releaseException();
    m_saturation = rectifiedSaturation.releaseReturnValue();
    return { };
}

CSSColorPercent CSSHSL::l() const
{
    return toCSSColorPercent(m_lightness);
}

ExceptionOr<void> CSSHSL::setL(CSSColorPercent&& lightness)
{
    auto rectifiedLightness = rectifyCSSColorPercent(WTFMove(lightness));
    if (rectifiedLightness.hasException())
        return rectifiedLightness.releaseException();
    m_lightness = rectifiedLightness.releaseReturnValue();
    return { };
}

CSSColorPercent CSSHSL::alpha() const
{
    return toCSSColorPercent(m_alpha);
}

ExceptionOr<void> CSSHSL::setAlpha(CSSColorPercent&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(WTFMove(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = rectifiedAlpha.releaseReturnValue();
    return { };
}

} // namespace WebCore
