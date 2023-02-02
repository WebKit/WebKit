/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "DeclaredStylePropertyMap.h"

#include "CSSCustomPropertyValue.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSUnparsedValue.h"
#include "Document.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"

namespace WebCore {

Ref<DeclaredStylePropertyMap> DeclaredStylePropertyMap::create(CSSStyleRule& ownerRule)
{
    return adoptRef(*new DeclaredStylePropertyMap(ownerRule));
}

DeclaredStylePropertyMap::DeclaredStylePropertyMap(CSSStyleRule& ownerRule)
    : m_ownerRule(ownerRule)
{
}

unsigned DeclaredStylePropertyMap::size() const
{
    if (auto* styleRule = this->styleRule())
        return styleRule->properties().propertyCount();
    return 0;
}

auto DeclaredStylePropertyMap::entries(ScriptExecutionContext* context) const -> Vector<StylePropertyMapEntry>
{
    if (!context)
        return { };

    auto* styleRule = this->styleRule();
    if (!styleRule)
        return { };

    auto& document = downcast<Document>(*context);
    return map(styleRule->properties(), [&] (auto propertyReference) {
        return StylePropertyMapEntry { propertyReference.cssName(), reifyValueToVector(RefPtr<CSSValue> { propertyReference.value() }, propertyReference.id(), document) };
    });
}

RefPtr<CSSValue> DeclaredStylePropertyMap::propertyValue(CSSPropertyID propertyID) const
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return nullptr;
    return styleRule->properties().getPropertyCSSValue(propertyID);
}

String DeclaredStylePropertyMap::shorthandPropertySerialization(CSSPropertyID propertyID) const
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return { };
    return styleRule->properties().getPropertyValue(propertyID);
}

RefPtr<CSSValue> DeclaredStylePropertyMap::customPropertyValue(const AtomString& propertyName) const
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return nullptr;
    return styleRule->properties().getCustomPropertyCSSValue(propertyName.string());
}

bool DeclaredStylePropertyMap::setShorthandProperty(CSSPropertyID propertyID, const String& value)
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return false;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    bool didFailParsing = false;
    bool important = false;
    styleRule->mutableProperties().setProperty(propertyID, value, important, &didFailParsing);
    return !didFailParsing;
}

bool DeclaredStylePropertyMap::setProperty(CSSPropertyID propertyID, Ref<CSSValue>&& value)
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return false;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    bool didFailParsing = false;
    bool important = false;
    styleRule->mutableProperties().setProperty(propertyID, value->cssText(), important, &didFailParsing);
    return !didFailParsing;
}

bool DeclaredStylePropertyMap::setCustomProperty(Document&, const AtomString& property, Ref<CSSVariableReferenceValue>&& value)
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return false;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    bool important = false;
    auto customPropertyValue = CSSCustomPropertyValue::createUnresolved(property, WTFMove(value));
    styleRule->mutableProperties().addParsedProperty(CSSProperty(CSSPropertyCustom, WTFMove(customPropertyValue), important));
    return true;
}

void DeclaredStylePropertyMap::removeProperty(CSSPropertyID propertyID)
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    styleRule->mutableProperties().removeProperty(propertyID);
}

void DeclaredStylePropertyMap::removeCustomProperty(const AtomString& property)
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    styleRule->mutableProperties().removeCustomProperty(property.string());
}

StyleRule* DeclaredStylePropertyMap::styleRule() const
{
    return m_ownerRule ? &m_ownerRule->styleRule() : nullptr;
}

void DeclaredStylePropertyMap::clear()
{
    auto* styleRule = this->styleRule();
    if (!styleRule)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(m_ownerRule.get());
    styleRule->mutableProperties().clear();
}

} // namespace WebCore
