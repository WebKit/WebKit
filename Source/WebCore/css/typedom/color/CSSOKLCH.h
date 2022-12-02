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

class CSSOKLCH final : public CSSColorValue {
    WTF_MAKE_ISO_ALLOCATED(CSSOKLCH);
public:
    static ExceptionOr<Ref<CSSOKLCH>> create(CSSColorPercent&& lightness, CSSColorPercent&& chroma, CSSColorAngle&& hue, CSSColorPercent&& alpha);

    CSSColorPercent l() const;
    ExceptionOr<void> setL(CSSColorPercent&&);
    CSSColorPercent c() const;
    ExceptionOr<void> setC(CSSColorPercent&&);
    CSSColorAngle h() const;
    ExceptionOr<void> setH(CSSColorAngle&&);
    CSSColorPercent alpha() const;
    ExceptionOr<void> setAlpha(CSSColorPercent&&);

private:
    CSSOKLCH(RectifiedCSSColorPercent&& lightness, RectifiedCSSColorPercent&& chroma, RectifiedCSSColorAngle&& hue, RectifiedCSSColorPercent&& alpha);

    RectifiedCSSColorPercent m_lightness;
    RectifiedCSSColorPercent m_chroma;
    RectifiedCSSColorAngle m_hue;
    RectifiedCSSColorPercent m_alpha;
};
    
} // namespace WebCore
