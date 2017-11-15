/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibleNode.h"

#include "AXObjectCache.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

using ARIAAttributeMap = HashMap<AXPropertyName, QualifiedName, WTF::IntHash<AXPropertyName>, AXPropertyHashTraits>;

static ARIAAttributeMap& ariaAttributeMap()
{
    static NeverDestroyed<ARIAAttributeMap> ariaAttributeMap = [] {
        const struct AttributeEntry {
            AXPropertyName name;
            QualifiedName ariaAttribute;
        } attributes[] = {
            { AXPropertyName::Label, aria_labelAttr },
            { AXPropertyName::Role, roleAttr }
        };
        ARIAAttributeMap map;
        for (auto& attribute : attributes)
            map.set(attribute.name, attribute.ariaAttribute);
        return map;
    }();
    return ariaAttributeMap;
}

static bool isPropertyValueString(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::Label:
    case AXPropertyName::Role:
        return true;
    default:
        return false;
    }
}

bool AccessibleNode::hasProperty(Element& element, AXPropertyName propertyName)
{
    if (auto* accessibleNode = element.existingAccessibleNode()) {
        if (accessibleNode->m_propertyMap.contains(propertyName))
            return true;
    }

    if (ariaAttributeMap().contains(propertyName))
        return element.hasAttributeWithoutSynchronization(ariaAttributeMap().get(propertyName));

    return false;
}

const PropertyValueVariant AccessibleNode::valueForProperty(Element& element, AXPropertyName propertyName)
{
    if (auto* accessibleNode = element.existingAccessibleNode()) {
        if (accessibleNode->m_propertyMap.contains(propertyName))
            return accessibleNode->m_propertyMap.get(propertyName);
    }

    return nullptr;
}

const String AccessibleNode::effectiveStringValueForElement(Element& element, AXPropertyName propertyName)
{
    const String& value = stringValueForProperty(element, propertyName);
    if (!value.isEmpty())
        return value;

    if (ariaAttributeMap().contains(propertyName) && isPropertyValueString(propertyName))
        return element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName));

    return nullAtom();
}

const String AccessibleNode::stringValueForProperty(Element& element, AXPropertyName propertyName)
{
    const PropertyValueVariant&& variant = AccessibleNode::valueForProperty(element, propertyName);
    if (WTF::holds_alternative<String>(variant))
        return WTF::get<String>(variant);
    return nullAtom();
}

void AccessibleNode::setStringProperty(const String& value, AXPropertyName propertyName)
{
    if (value.isEmpty()) {
        m_propertyMap.remove(propertyName);
        return;
    }
    m_propertyMap.set(propertyName, value);
}

String AccessibleNode::role() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Role);
}

void AccessibleNode::setRole(const String& role)
{
    setStringProperty(role, AXPropertyName::Role);
    if (AXObjectCache* cache = m_ownerElement.document().axObjectCache())
        cache->handleAttributeChanged(roleAttr, &m_ownerElement);
}

String AccessibleNode::label() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Label);
}

void AccessibleNode::setLabel(const String& label)
{
    setStringProperty(label, AXPropertyName::Label);
    if (AXObjectCache* cache = m_ownerElement.document().axObjectCache())
        cache->handleAttributeChanged(aria_labelAttr, &m_ownerElement);
}

} // namespace WebCore
