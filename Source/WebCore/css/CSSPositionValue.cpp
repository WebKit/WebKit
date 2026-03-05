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
#include "CSSPositionValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

Ref<CSSPositionValue> CSSPositionValue::create(CSS::Position&& position)
{
    return adoptRef(*new CSSPositionValue(WTF::move(position)));
}

CSSPositionValue::CSSPositionValue(CSS::Position&& position)
    : CSSValue(ClassType::Position)
    , m_position(WTF::move(position))
{
}

String CSSPositionValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_position);
}

bool CSSPositionValue::equals(const CSSPositionValue& other) const
{
    return m_position == other.m_position;
}

IterationStatus CSSPositionValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_position);
}

// MARK: PositionX

Ref<CSSPositionXValue> CSSPositionXValue::create(CSS::PositionX&& position)
{
    return adoptRef(*new CSSPositionXValue(WTF::move(position)));
}

CSSPositionXValue::CSSPositionXValue(CSS::PositionX&& position)
    : CSSValue(ClassType::PositionX)
    , m_position(WTF::move(position))
{
}

String CSSPositionXValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_position);
}

bool CSSPositionXValue::equals(const CSSPositionXValue& other) const
{
    return m_position == other.m_position;
}

IterationStatus CSSPositionXValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_position);
}

// MARK: PositionY

Ref<CSSPositionYValue> CSSPositionYValue::create(CSS::PositionY&& position)
{
    return adoptRef(*new CSSPositionYValue(WTF::move(position)));
}

CSSPositionYValue::CSSPositionYValue(CSS::PositionY&& position)
    : CSSValue(ClassType::PositionY)
    , m_position(WTF::move(position))
{
}

String CSSPositionYValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_position);
}

bool CSSPositionYValue::equals(const CSSPositionYValue& other) const
{
    return m_position == other.m_position;
}

IterationStatus CSSPositionYValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_position);
}

} // namespace WebCore
