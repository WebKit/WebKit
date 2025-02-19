/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "CSSBackgroundRepeatValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

CSSBackgroundRepeatValue::CSSBackgroundRepeatValue(CSS::BackgroundRepeatStyle&& repeat)
    : CSSValue(ClassType::BackgroundRepeat)
    , m_repeat(WTFMove(repeat))
{
}

CSSBackgroundRepeatValue::~CSSBackgroundRepeatValue() = default;

Ref<CSSBackgroundRepeatValue> CSSBackgroundRepeatValue::create(CSS::BackgroundRepeatStyle&& repeat)
{
    return adoptRef(*new CSSBackgroundRepeatValue(WTFMove(repeat)));
}

String CSSBackgroundRepeatValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_repeat);
}

bool CSSBackgroundRepeatValue::equals(const CSSBackgroundRepeatValue& other) const
{
    return m_repeat == other.m_repeat;
}

IterationStatus CSSBackgroundRepeatValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_repeat);
}

Ref<DeprecatedCSSOMValue> CSSBackgroundRepeatValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    auto axis = [&](const CSS::BackgroundRepeatStyleSingleAxis& value) -> Ref<DeprecatedCSSOMValue> {
        return WTF::switchOn(value,
            [&]<CSSValueID Id>(const Constant<Id>& constant) {
                return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(constant.value), owner);
            }
        );
    };

    if (m_repeat.x == m_repeat.y)
        return axis(m_repeat.x);
    if (WTF::holdsAlternative<CSS::Keyword::Repeat>(m_repeat.x) && WTF::holdsAlternative<CSS::Keyword::NoRepeat>(m_repeat.y))
        return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(CSSValueRepeatX), owner);
    if (WTF::holdsAlternative<CSS::Keyword::NoRepeat>(m_repeat.x) && WTF::holdsAlternative<CSS::Keyword::Repeat>(m_repeat.y))
        return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(CSSValueRepeatY), owner);
    return DeprecatedCSSOMValueList::create({ axis(m_repeat.x), axis(m_repeat.y) }, CSSValue::SpaceSeparator, owner);
}

} // namespace WebCore
