/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPositionTryRule.h"

#include "CSSPositionTryDescriptors.h"
#include "CSSSerializationContext.h"

namespace WebCore {

Ref<StyleRulePositionTry> StyleRulePositionTry::create(AtomString&& name, Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRulePositionTry(WTF::move(name), WTF::move(properties)));
}

StyleRulePositionTry::StyleRulePositionTry(AtomString&& name, Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::PositionTry)
    , m_name(name)
    , m_properties(properties)
{
}

StyleRulePositionTry::StyleRulePositionTry(const StyleRulePositionTry& o)
    : StyleRuleBase(o)
    , m_name(o.m_name)
    , m_properties(protect(o.properties())->mutableCopy())
{
}

MutableStyleProperties& StyleRulePositionTry::mutableProperties()
{
    Ref properties = m_properties;

    if (!is<MutableStyleProperties>(properties))
        m_properties = properties->mutableCopy();

    return downcast<MutableStyleProperties>(m_properties.get());
}

Ref<CSSPositionTryRule> CSSPositionTryRule::create(StyleRulePositionTry& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSPositionTryRule(rule, parent));
}

CSSPositionTryRule::CSSPositionTryRule(StyleRulePositionTry& rule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_positionTryRule(rule)
    , m_propertiesCSSOMWrapper(nullptr)
{
}

CSSPositionTryRule::~CSSPositionTryRule() = default;

String CSSPositionTryRule::cssText() const
{
    StringBuilder builder;
    builder.append("@position-try "_s, name(), " {"_s);

    Ref properties = m_positionTryRule->properties();

    if (auto declarations = properties->asText(CSS::defaultSerializationContext()); !declarations.isEmpty())
        builder.append(' ', declarations, ' ');
    else
        builder.append(' ');

    builder.append('}');

    return builder.toString();
}

void CSSPositionTryRule::reattach(StyleRuleBase& rule)
{
    m_positionTryRule = downcast<StyleRulePositionTry>(rule);
    if (RefPtr propertiesCSSOMWrapper = m_propertiesCSSOMWrapper)
        propertiesCSSOMWrapper->reattach(protect(protect(positionTryRule())->mutableProperties()));
}

AtomString CSSPositionTryRule::name() const
{
    return m_positionTryRule->name();
}

CSSPositionTryDescriptors& CSSPositionTryRule::style()
{
    Ref mutableProperties = protect(positionTryRule())->mutableProperties();

    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = CSSPositionTryDescriptors::create(mutableProperties.get(), *this);

    return *m_propertiesCSSOMWrapper;
}

} // namespace WebCore
