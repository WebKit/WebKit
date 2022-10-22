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

#include "CSSNumericValue.h"
#include "CSSTransformComponent.h"

namespace WebCore {

class CSSFunctionValue;

template<typename> class ExceptionOr;

class CSSRotate : public CSSTransformComponent {
    WTF_MAKE_ISO_ALLOCATED(CSSRotate);
public:
    static ExceptionOr<Ref<CSSRotate>> create(CSSNumberish, CSSNumberish, CSSNumberish, Ref<CSSNumericValue>);
    static ExceptionOr<Ref<CSSRotate>> create(Ref<CSSNumericValue>);
    static ExceptionOr<Ref<CSSRotate>> create(CSSFunctionValue&);

    CSSNumberish x() { return { m_x.ptr() }; }
    CSSNumberish y() { return { m_y.ptr() }; }
    CSSNumberish z() { return { m_z.ptr() }; }
    const CSSNumericValue& angle() { return m_angle.get(); }

    ExceptionOr<void> setX(CSSNumberish);
    ExceptionOr<void> setY(CSSNumberish);
    ExceptionOr<void> setZ(CSSNumberish);
    ExceptionOr<void> setAngle(Ref<CSSNumericValue>);

    void serialize(StringBuilder&) const final;
    ExceptionOr<Ref<DOMMatrix>> toMatrix() final;
    
    CSSTransformType getType() const final { return CSSTransformType::Rotate; }
    
private:
    CSSRotate(CSSTransformComponent::Is2D, Ref<CSSNumericValue>, Ref<CSSNumericValue>, Ref<CSSNumericValue>, Ref<CSSNumericValue>);
    
    Ref<CSSNumericValue> m_x;
    Ref<CSSNumericValue> m_y;
    Ref<CSSNumericValue> m_z;
    Ref<CSSNumericValue> m_angle;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSRotate)
    static bool isType(const WebCore::CSSTransformComponent& transform) { return transform.getType() == WebCore::CSSTransformType::Rotate; }
SPECIALIZE_TYPE_TRAITS_END()
