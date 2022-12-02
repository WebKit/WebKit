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
#include "CSSHWB.h"

#include "Exception.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSHWB);

ExceptionOr<Ref<CSSHWB>> CSSHWB::create(Ref<CSSNumericValue>&& hue, CSSNumberish&& whiteness, CSSNumberish&& blackness, CSSNumberish&& alpha)
{
    auto rectifiedHue = rectifyCSSColorAngle(RefPtr { WTFMove(hue) });
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    auto rectifiedWhiteness = rectifyCSSColorPercent(toCSSColorPercent(whiteness));
    if (rectifiedWhiteness.hasException())
        return rectifiedWhiteness.releaseException();
    auto rectifiedBlackness = rectifyCSSColorPercent(toCSSColorPercent(blackness));
    if (rectifiedBlackness.hasException())
        return rectifiedBlackness.releaseException();
    auto rectifiedAlpha = rectifyCSSColorPercent(toCSSColorPercent(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    return adoptRef(*new CSSHWB(std::get<RefPtr<CSSNumericValue>>(rectifiedHue.releaseReturnValue()).releaseNonNull()
        , std::get<RefPtr<CSSNumericValue>>(rectifiedWhiteness.releaseReturnValue()).releaseNonNull()
        , std::get<RefPtr<CSSNumericValue>>(rectifiedBlackness.releaseReturnValue()).releaseNonNull()
        , std::get<RefPtr<CSSNumericValue>>(rectifiedAlpha.releaseReturnValue()).releaseNonNull()));
}

CSSHWB::CSSHWB(Ref<CSSNumericValue>&& hue, Ref<CSSNumericValue>&& whiteness, Ref<CSSNumericValue>&& blackness, Ref<CSSNumericValue>&& alpha)
    : m_hue(WTFMove(hue))
    , m_whiteness(WTFMove(whiteness))
    , m_blackness(WTFMove(blackness))
    , m_alpha(WTFMove(alpha))
{
}

CSSNumericValue& CSSHWB::h() const
{
    return m_hue;
}

ExceptionOr<void> CSSHWB::setH(Ref<CSSNumericValue>&& hue)
{
    auto rectifiedHue = rectifyCSSColorAngle(RefPtr { WTFMove(hue) });
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    m_hue = std::get<RefPtr<CSSNumericValue>>(rectifiedHue.releaseReturnValue()).releaseNonNull();
    return { };
}

CSSNumberish CSSHWB::w() const
{
    return RefPtr { m_whiteness.copyRef() };
}

ExceptionOr<void> CSSHWB::setW(CSSNumberish&& whiteness)
{
    auto rectifiedWhiteness = rectifyCSSColorPercent(toCSSColorPercent(whiteness));
    if (rectifiedWhiteness.hasException())
        return rectifiedWhiteness.releaseException();
    m_whiteness = std::get<RefPtr<CSSNumericValue>>(rectifiedWhiteness.releaseReturnValue()).releaseNonNull();
    return { };
}

CSSNumberish CSSHWB::b() const
{
    return RefPtr { m_blackness.copyRef() };
}

ExceptionOr<void> CSSHWB::setB(CSSNumberish&& blackness)
{
    auto rectifiedBlackness = rectifyCSSColorPercent(toCSSColorPercent(blackness));
    if (rectifiedBlackness.hasException())
        return rectifiedBlackness.releaseException();
    m_blackness = std::get<RefPtr<CSSNumericValue>>(rectifiedBlackness.releaseReturnValue()).releaseNonNull();
    return { };
}

CSSNumberish CSSHWB::alpha() const
{
    return RefPtr { m_alpha.copyRef() };
}

ExceptionOr<void> CSSHWB::setAlpha(CSSNumberish&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(toCSSColorPercent(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = std::get<RefPtr<CSSNumericValue>>(rectifiedAlpha.releaseReturnValue()).releaseNonNull();
    return { };
}

} // namespace WebCore
