/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSPropertyIdentifierValue.h"

#include "CSSPropertyNames.h"

namespace WebCore {

Ref<CSSPropertyIdentifierValue> CSSPropertyIdentifierValue::create(CSS::PropertyIdentifier propertyIdentifier)
{
    return adoptRef(*new CSSPropertyIdentifierValue(WTF::move(propertyIdentifier)));
}

CSSPropertyIdentifierValue::CSSPropertyIdentifierValue(CSS::PropertyIdentifier propertyIdentifier)
    : CSSValue(ClassType::PropertyIdentifier)
    , m_propertyIdentifier(WTF::move(propertyIdentifier))
{
}

String CSSPropertyIdentifierValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_propertyIdentifier);
}

bool CSSPropertyIdentifierValue::equals(const CSSPropertyIdentifierValue& other) const
{
    return m_propertyIdentifier == other.m_propertyIdentifier;
}

IterationStatus CSSPropertyIdentifierValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_propertyIdentifier);
}

String CSSPropertyIdentifierValue::stringValue() const
{
    return nameString(m_propertyIdentifier.value);
}

} // namespace WebCore
