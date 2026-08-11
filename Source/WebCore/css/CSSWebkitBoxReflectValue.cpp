/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "CSSWebkitBoxReflectValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

CSSWebkitBoxReflectValue::CSSWebkitBoxReflectValue(CSS::WebkitBoxReflect&& reflect)
    : CSSValue(ClassType::WebkitBoxReflect)
    , m_reflect(WTF::move(reflect))
{
}

Ref<CSSWebkitBoxReflectValue> CSSWebkitBoxReflectValue::create(CSS::WebkitBoxReflect&& reflect)
{
    return adoptRef(*new CSSWebkitBoxReflectValue(WTF::move(reflect)));
}

String CSSWebkitBoxReflectValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_reflect);
}

IterationStatus CSSWebkitBoxReflectValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& function) const
{
    return CSS::visitCSSValueChildren(function, m_reflect);
}

bool CSSWebkitBoxReflectValue::equals(const CSSWebkitBoxReflectValue& other) const
{
    return m_reflect == other.m_reflect;
}

} // namespace WebCore
