/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "InlineStylePropertyMap.h"

#include "StyledElement.h"

namespace WebCore {

Ref<InlineStylePropertyMap> InlineStylePropertyMap::create(StyledElement& element)
{
    return adoptRef(*new InlineStylePropertyMap(element));
}

InlineStylePropertyMap::InlineStylePropertyMap(StyledElement& element)
    : m_element(&element)
{
}

RefPtr<CSSValue> InlineStylePropertyMap::propertyValue(CSSPropertyID propertyID) const
{
    if (auto* inlineStyle = m_element ? m_element->inlineStyle() : nullptr)
        return inlineStyle->getPropertyCSSValue(propertyID);
    return nullptr;
}

String InlineStylePropertyMap::shorthandPropertySerialization(CSSPropertyID propertyID) const
{
    if (auto* inlineStyle = m_element ? m_element->inlineStyle() : nullptr)
        return inlineStyle->getPropertyValue(propertyID);
    return String();
}

RefPtr<CSSValue> InlineStylePropertyMap::customPropertyValue(const AtomString& property) const
{
    if (auto* inlineStyle = m_element ? m_element->inlineStyle() : nullptr)
        return inlineStyle->getCustomPropertyCSSValue(property.string());
    return nullptr;
}

unsigned InlineStylePropertyMap::size() const
{
    auto* inlineStyle = m_element ? m_element->inlineStyle() : nullptr;
    return inlineStyle ? inlineStyle->propertyCount() : 0;
}

auto InlineStylePropertyMap::entries(ScriptExecutionContext* context) const -> Vector<StylePropertyMapEntry>
{
    if (!m_element || !context)
        return { };

    auto* inlineStyle = m_element->inlineStyle();
    if (!inlineStyle)
        return { };

    auto& document = downcast<Document>(*context);
    return map(*inlineStyle, [&document] (auto property) {
        return StylePropertyMapEntry(property.cssName(), reifyValueToVector(RefPtr<CSSValue> { property.value() }, document));
    });
}

void InlineStylePropertyMap::removeProperty(CSSPropertyID propertyID)
{
    if (m_element)
        m_element->removeInlineStyleProperty(propertyID);
}

bool InlineStylePropertyMap::setShorthandProperty(CSSPropertyID propertyID, const String& value)
{
    if (!m_element)
        return false;
    bool didFailParsing = false;
    bool important = false;
    m_element->setInlineStyleProperty(propertyID, value, important, &didFailParsing);
    return !didFailParsing;
}

bool InlineStylePropertyMap::setProperty(CSSPropertyID propertyID, Ref<CSSValue>&& value)
{
    if (!m_element)
        return false;
    bool didFailParsing = false;
    bool important = false;
    m_element->setInlineStyleProperty(propertyID, value->cssText(), important, &didFailParsing);
    return !didFailParsing;
}

bool InlineStylePropertyMap::setCustomProperty(Document&, const AtomString& property, Ref<CSSVariableReferenceValue>&& value)
{
    if (!m_element)
        return false;

    auto customPropertyValue = CSSCustomPropertyValue::createUnresolved(property, WTFMove(value));
    m_element->setInlineStyleCustomProperty(WTFMove(customPropertyValue));
    return true;
}

void InlineStylePropertyMap::removeCustomProperty(const AtomString& property)
{
    if (m_element)
        m_element->removeInlineStyleCustomProperty(property);
}

void InlineStylePropertyMap::clear()
{
    if (m_element)
        m_element->removeAllInlineStyleProperties();
}

void InlineStylePropertyMap::clearElement()
{
    m_element = nullptr;
}

} // namespace WebCore
