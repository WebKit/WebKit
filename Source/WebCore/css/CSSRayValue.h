/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveValue.h"

namespace WebCore {

// Class containing the value of a ray() function, as used in offset-path:
// https://drafts.fxtf.org/motion-1/#funcdef-offset-path-ray.
class CSSRayValue final : public CSSValue {
public:
    static Ref<CSSRayValue> create(Ref<CSSPrimitiveValue>&& angle, Ref<CSSPrimitiveValue>&& size, bool isContaining)
    {
        return adoptRef(*new CSSRayValue(WTFMove(angle), WTFMove(size), isContaining));
    }

    String customCSSText() const;

    Ref<CSSPrimitiveValue> angle() const { return m_angle; }
    Ref<CSSPrimitiveValue> size() const { return m_size; }
    bool isContaining() const { return m_isContaining; }

    bool equals(const CSSRayValue&) const;

private:
    CSSRayValue(Ref<CSSPrimitiveValue>&& angle, Ref<CSSPrimitiveValue>&& size, bool isContaining)
        : CSSValue(RayClass)
        , m_angle(WTFMove(angle))
        , m_size(WTFMove(size))
        , m_isContaining(isContaining)
    {
        ASSERT(m_angle->isAngle());
        ASSERT(m_size->isValueID());
    }

    Ref<CSSPrimitiveValue> m_angle;
    Ref<CSSPrimitiveValue> m_size;
    bool m_isContaining;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRayValue, isRayValue())
