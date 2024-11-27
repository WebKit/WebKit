/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"

namespace WebCore {

Ref<StyleRulePositionTry> StyleRulePositionTry::create(AtomString&& name, Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRulePositionTry(WTFMove(name), WTFMove(properties)));
}

StyleRulePositionTry::StyleRulePositionTry(AtomString&& name, Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::PositionTry)
    , m_name(name)
    , m_properties(properties)
{

}

MutableStyleProperties& StyleRulePositionTry::mutableProperties()
{
    if (auto* mutableProperties = dynamicDowncast<MutableStyleProperties>(m_properties.get()))
        return *mutableProperties;

    // m_properties is StyleProperties, so returning directly would require another downcast.
    // Hence why we copy into a temporary ref first before moving it into m_properties, so we
    // have a ref of MutableStyleProperties.
    auto mutableProperties = m_properties->mutableCopy();
    auto& mutablePropertiesRef = mutableProperties.get();
    m_properties = WTFMove(mutableProperties);
    return mutablePropertiesRef;
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

    if (auto declarations = m_positionTryRule->properties().asText(); !declarations.isEmpty())
        builder.append(' ', declarations, ' ');
    else
        builder.append(' ');

    builder.append('}');

    return builder.toString();
}

void CSSPositionTryRule::reattach(StyleRuleBase&)
{
}

AtomString CSSPositionTryRule::name() const
{
    return m_positionTryRule->name();
}

CSSStyleDeclaration& CSSPositionTryRule::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_positionTryRule->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

} // namespace WebCore
