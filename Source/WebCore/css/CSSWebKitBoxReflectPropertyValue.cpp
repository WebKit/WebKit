/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSWebKitBoxReflectPropertyValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

CSSWebKitBoxReflectPropertyValue::CSSWebKitBoxReflectPropertyValue(CSS::WebKitBoxReflectProperty&& property)
    : CSSValue(ClassType::WebKitBoxReflectProperty)
    , m_property(WTFMove(property))
{
}

CSSWebKitBoxReflectPropertyValue::~CSSWebKitBoxReflectPropertyValue() = default;

Ref<CSSWebKitBoxReflectPropertyValue> CSSWebKitBoxReflectPropertyValue::create(CSS::WebKitBoxReflectProperty&& property)
{
    return adoptRef(*new CSSWebKitBoxReflectPropertyValue(WTFMove(property)));
}

String CSSWebKitBoxReflectPropertyValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_property);
}

bool CSSWebKitBoxReflectPropertyValue::equals(const CSSWebKitBoxReflectPropertyValue& other) const
{
    return m_property == other.m_property;
}

IterationStatus CSSWebKitBoxReflectPropertyValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_property);
}

} // namespace WebCore
