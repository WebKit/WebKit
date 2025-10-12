/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSRatioValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

Ref<CSSRatioValue> CSSRatioValue::create(CSS::Ratio&& ratio)
{
    return adoptRef(*new CSSRatioValue(WTFMove(ratio)));
}

Ref<CSSRatioValue> CSSRatioValue::create(const CSS::Ratio& ratio)
{
    return adoptRef(*new CSSRatioValue(ratio));
}

CSSRatioValue::CSSRatioValue(CSS::Ratio&& ratio)
    : CSSValue(ClassType::Ratio)
    , m_ratio(WTFMove(ratio))
{
}

CSSRatioValue::CSSRatioValue(const CSS::Ratio& ratio)
    : CSSValue(ClassType::Ratio)
    , m_ratio(ratio)
{
}

String CSSRatioValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_ratio);
}

bool CSSRatioValue::equals(const CSSRatioValue& other) const
{
    return m_ratio == other.m_ratio;
}

IterationStatus CSSRatioValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_ratio);
}

} // namespace WebCore
