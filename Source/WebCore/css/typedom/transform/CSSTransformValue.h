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

#if ENABLE(CSS_TYPED_OM)

#include "CSSStyleValue.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSTransformComponent;
class DOMMatrix;
template<typename> class ExceptionOr;

class CSSTransformValue : public CSSStyleValue {
    WTF_MAKE_ISO_ALLOCATED(CSSTransformValue);
public:
    static Ref<CSSTransformValue> create(Vector<RefPtr<CSSTransformComponent>>&& transforms);

    size_t length() const { return m_components.size(); }
    ExceptionOr<RefPtr<CSSTransformComponent>> item(size_t);
    ExceptionOr<RefPtr<CSSTransformComponent>> setItem(size_t, Ref<CSSTransformComponent>&&);
    
    bool is2D() const;
    void setIs2D(bool);
    
    ExceptionOr<Ref<DOMMatrix>> toMatrix();
    
    CSSStyleValueType getType() const override { return CSSStyleValueType::CSSTransformValue; }
private:
    CSSTransformValue(Vector<RefPtr<CSSTransformComponent>>&&);

    bool m_is2D { false };
    Vector<RefPtr<CSSTransformComponent>> m_components;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSTransformValue)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSTransformValue; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
