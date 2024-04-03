/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveValue.h"
#include "CSSValue.h"

namespace WebCore {

class CSSOffsetRotateValue final : public CSSValue {
public:
    static Ref<CSSOffsetRotateValue> create(RefPtr<CSSPrimitiveValue>&& modifier, RefPtr<CSSPrimitiveValue>&& angle)
    {
        return adoptRef(*new CSSOffsetRotateValue(WTFMove(modifier), WTFMove(angle)));
    }

    String customCSSText() const;

    CSSPrimitiveValue* modifier() const { return m_modifier.get(); }
    CSSPrimitiveValue* angle() const { return m_angle.get(); }

    bool isInitialValue() const;

    bool equals(const CSSOffsetRotateValue&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (m_modifier) {
            if (func(*m_modifier) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (m_angle) {
            if (func(*m_angle) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSOffsetRotateValue(RefPtr<CSSPrimitiveValue>&& modifier, RefPtr<CSSPrimitiveValue>&& angle)
        : CSSValue(OffsetRotateClass)
        , m_modifier(WTFMove(modifier))
        , m_angle(WTFMove(angle))
    {
        ASSERT(m_modifier || m_angle);

        if (m_modifier)
            ASSERT(m_modifier->isValueID());

        if (m_angle)
            ASSERT(m_angle->isAngle());
    }

    RefPtr<CSSPrimitiveValue> m_modifier;
    RefPtr<CSSPrimitiveValue> m_angle;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSOffsetRotateValue, isOffsetRotateValue())
