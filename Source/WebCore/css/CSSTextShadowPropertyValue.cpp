/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSTextShadowPropertyValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSValuePool.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMTextShadowValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

Ref<CSSTextShadowPropertyValue> CSSTextShadowPropertyValue::create(CSS::TextShadowProperty shadow)
{
    return adoptRef(*new CSSTextShadowPropertyValue(WTFMove(shadow)));
}

CSSTextShadowPropertyValue::CSSTextShadowPropertyValue(CSS::TextShadowProperty&& shadow)
    : CSSValue(ClassType::TextShadowProperty)
    , m_shadow(WTFMove(shadow))
{
}

String CSSTextShadowPropertyValue::customCSSText() const
{
    return CSS::serializationForCSS(m_shadow);
}

bool CSSTextShadowPropertyValue::equals(const CSSTextShadowPropertyValue& other) const
{
    return m_shadow == other.m_shadow;
}

IterationStatus CSSTextShadowPropertyValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_shadow);
}

Ref<DeprecatedCSSOMValue> CSSTextShadowPropertyValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return WTF::switchOn(m_shadow,
        [&](CSS::Keyword::None) -> Ref<DeprecatedCSSOMValue> {
            return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(CSSValueNone), owner);
        },
        [&](const auto& list) -> Ref<DeprecatedCSSOMValue> {
            auto values = list.value.template map<Vector<Ref<DeprecatedCSSOMValue>, 4>>([&](const auto& value) {
                return DeprecatedCSSOMTextShadowValue::create(value, owner);
            });

            return DeprecatedCSSOMValueList::create(WTFMove(values), CSSValue::CommaSeparator, owner);
        }
    );
}

} // namespace WebCore
