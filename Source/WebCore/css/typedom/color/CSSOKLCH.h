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

#pragma once

#include "CSSColorValue.h"

namespace WebCore {

class CSSOKLCH : public CSSColorValue {
    WTF_MAKE_ISO_ALLOCATED(CSSOKLCH);
public:
    template<typename... Args> static Ref<CSSOKLCH> create(Args&&... args) { return adoptRef(*new CSSOKLCH(std::forward<Args>(args)...)); }

    const CSSColorPercent& l() const { return m_lightness; }
    void setL(CSSColorPercent lightness) { m_lightness = WTFMove(lightness); }
    const CSSColorPercent& c() const { return m_chroma; }
    void setC(CSSColorPercent chroma) { m_chroma = WTFMove(chroma); }
    const CSSColorAngle& h() const { return m_hue; }
    void setH(CSSColorAngle hue) { m_hue = WTFMove(hue); }
    const CSSColorPercent& alpha() const { return m_alpha; }
    void setAlpha(CSSColorPercent alpha) { m_alpha = WTFMove(alpha); }

private:
    CSSOKLCH(CSSColorPercent l, CSSColorPercent c, CSSColorAngle h, CSSColorPercent alpha);

    CSSColorPercent m_lightness;
    CSSColorPercent m_chroma;
    CSSColorAngle m_hue;
    CSSColorPercent m_alpha;
};
    
} // namespace WebCore
