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

class CSSHWB : public CSSColorValue {
    WTF_MAKE_ISO_ALLOCATED(CSSHWB);
public:
    template<typename... Args> static Ref<CSSHWB> create(Args&&... args) { return adoptRef(*new CSSHWB(std::forward<Args>(args)...)); }

    const Ref<CSSNumericValue>& h() const { return m_hue; }
    void setH(Ref<CSSNumericValue> hue) { m_hue = WTFMove(hue); }
    const CSSNumberish& w() const { return m_whiteness; }
    void setW(CSSNumberish whiteness) { m_whiteness = WTFMove(whiteness); }
    const CSSNumberish& b() const { return m_blackness; }
    void setB(CSSNumberish blackness) { m_blackness = WTFMove(blackness); }
    const CSSNumberish& alpha() const { return m_alpha; }
    void setAlpha(CSSNumberish alpha) { m_alpha = WTFMove(alpha); }

private:
    CSSHWB(Ref<CSSNumericValue>, CSSNumberish, CSSNumberish, CSSNumberish);

    Ref<CSSNumericValue> m_hue;
    CSSNumberish m_whiteness;
    CSSNumberish m_blackness;
    CSSNumberish m_alpha;
};
    
} // namespace WebCore
