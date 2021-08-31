/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename T> class ExceptionOr;

enum class CSSStyleValueType : uint8_t {
    CSSStyleValue,
    CSSStyleImageValue,
    CSSTransformValue,
    CSSNumericValue,
    CSSMathValue,
    CSSUnitValue,
    CSSUnparsedValue,
    CSSKeywordValue
};

class CSSStyleValue : public RefCounted<CSSStyleValue>, public ScriptWrappable {
    WTF_MAKE_ISO_ALLOCATED(CSSStyleValue);
public:

    virtual String toString() const;
    virtual ~CSSStyleValue() = default;
    
    virtual CSSStyleValueType getType() const { return CSSStyleValueType::CSSStyleValue; }
        
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseStyleValue(const String&, const String&, bool);
    static ExceptionOr<Ref<CSSStyleValue>> parse(const String&, const String&);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseAll(const String&, const String&);
    
    static RefPtr<CSSStyleValue> reifyValue(CSSPropertyID, RefPtr<CSSValue>&&);
    
    static Ref<CSSStyleValue> create(RefPtr<CSSValue>&&, String&& = String());
    static Ref<CSSStyleValue> create();

protected:
    CSSStyleValue(RefPtr<CSSValue>&&, String&& = String());
    CSSStyleValue() = default;
    
    String m_customPropertyName;
    RefPtr<CSSValue> m_propertyValue;
};

} // namespace WebCore

#endif
