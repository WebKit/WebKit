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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSHWB);

ExceptionOr<Ref<CSSHWB>> CSSHWB::create(Ref<CSSNumericValue>&& hue, CSSNumberish&& whiteness, CSSNumberish&& blackness, CSSNumberish&& alpha)
{
    auto rectifiedHue = rectifyCSSColorAngle(WTF::move(hue));
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
    return adoptRef(*new CSSHWB(std::get<Ref<CSSNumericValue>>(rectifiedHue.releaseReturnValue())
        , std::get<Ref<CSSNumericValue>>(rectifiedWhiteness.releaseReturnValue())
        , std::get<Ref<CSSNumericValue>>(rectifiedBlackness.releaseReturnValue())
        , std::get<Ref<CSSNumericValue>>(rectifiedAlpha.releaseReturnValue())));
}

CSSHWB::CSSHWB(Ref<CSSNumericValue>&& hue, Ref<CSSNumericValue>&& whiteness, Ref<CSSNumericValue>&& blackness, Ref<CSSNumericValue>&& alpha)
    : m_hue(WTF::move(hue))
    , m_whiteness(WTF::move(whiteness))
    , m_blackness(WTF::move(blackness))
    , m_alpha(WTF::move(alpha))
{
}

CSSNumericValue& CSSHWB::h() const
{
    return m_hue;
}

ExceptionOr<void> CSSHWB::setH(Ref<CSSNumericValue>&& hue)
{
    auto rectifiedHue = rectifyCSSColorAngle(WTF::move(hue));
    if (rectifiedHue.hasException())
        return rectifiedHue.releaseException();
    m_hue = std::get<Ref<CSSNumericValue>>(rectifiedHue.releaseReturnValue());
    return { };
}

CSSNumberish CSSHWB::w() const
{
    return m_whiteness;
}

ExceptionOr<void> CSSHWB::setW(CSSNumberish&& whiteness)
{
    auto rectifiedWhiteness = rectifyCSSColorPercent(toCSSColorPercent(whiteness));
    if (rectifiedWhiteness.hasException())
        return rectifiedWhiteness.releaseException();
    m_whiteness = std::get<Ref<CSSNumericValue>>(rectifiedWhiteness.releaseReturnValue());
    return { };
}

CSSNumberish CSSHWB::b() const
{
    return m_blackness;
}

ExceptionOr<void> CSSHWB::setB(CSSNumberish&& blackness)
{
    auto rectifiedBlackness = rectifyCSSColorPercent(toCSSColorPercent(blackness));
    if (rectifiedBlackness.hasException())
        return rectifiedBlackness.releaseException();
    m_blackness = std::get<Ref<CSSNumericValue>>(rectifiedBlackness.releaseReturnValue());
    return { };
}

CSSNumberish CSSHWB::alpha() const
{
    return m_alpha;
}

ExceptionOr<void> CSSHWB::setAlpha(CSSNumberish&& alpha)
{
    auto rectifiedAlpha = rectifyCSSColorPercent(toCSSColorPercent(alpha));
    if (rectifiedAlpha.hasException())
        return rectifiedAlpha.releaseException();
    m_alpha = std::get<Ref<CSSNumericValue>>(rectifiedAlpha.releaseReturnValue());
    return { };
}

} // namespace WebCore
