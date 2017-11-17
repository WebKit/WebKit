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
            { AXPropertyName::Autocomplete, aria_autocompleteAttr },
            { AXPropertyName::Checked, aria_checkedAttr },
            { AXPropertyName::Current, aria_currentAttr },
            { AXPropertyName::HasPopUp, aria_haspopupAttr },
            { AXPropertyName::Invalid, aria_invalidAttr },
            { AXPropertyName::KeyShortcuts, aria_keyshortcutsAttr },
            { AXPropertyName::Label, aria_labelAttr },
            { AXPropertyName::Live, aria_liveAttr },
            { AXPropertyName::Orientation, aria_orientationAttr },
            { AXPropertyName::Placeholder, aria_placeholderAttr },
            { AXPropertyName::Pressed, aria_pressedAttr },
            { AXPropertyName::Relevant, aria_relevantAttr },
            { AXPropertyName::Role, roleAttr },
            { AXPropertyName::RoleDescription, aria_roledescriptionAttr },
            { AXPropertyName::Sort, aria_sortAttr },
            { AXPropertyName::ValueText, aria_valuetextAttr }
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
    case AXPropertyName::Autocomplete:
    case AXPropertyName::Checked:
    case AXPropertyName::Current:
    case AXPropertyName::HasPopUp:
    case AXPropertyName::Invalid:
    case AXPropertyName::KeyShortcuts:
    case AXPropertyName::Label:
    case AXPropertyName::Live:
    case AXPropertyName::Orientation:
    case AXPropertyName::Placeholder:
    case AXPropertyName::Pressed:
    case AXPropertyName::Relevant:
    case AXPropertyName::Role:
    case AXPropertyName::RoleDescription:
    case AXPropertyName::Sort:
    case AXPropertyName::ValueText:
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

void AccessibleNode::notifyAttributeChanged(const WebCore::QualifiedName& name)
{
    if (AXObjectCache* cache = m_ownerElement.document().axObjectCache())
        cache->handleAttributeChanged(name, &m_ownerElement);
}

String AccessibleNode::autocomplete() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Autocomplete);
}

void AccessibleNode::setAutocomplete(const String& autocomplete)
{
    setStringProperty(autocomplete, AXPropertyName::Autocomplete);
    notifyAttributeChanged(aria_autocompleteAttr);
}

String AccessibleNode::checked() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Checked);
}

void AccessibleNode::setChecked(const String& checked)
{
    setStringProperty(checked, AXPropertyName::Checked);
    notifyAttributeChanged(aria_checkedAttr);
}

String AccessibleNode::current() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Current);
}

void AccessibleNode::setCurrent(const String& current)
{
    setStringProperty(current, AXPropertyName::Current);
    notifyAttributeChanged(aria_currentAttr);
}

String AccessibleNode::hasPopUp() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::HasPopUp);
}

void AccessibleNode::setHasPopUp(const String& hasPopUp)
{
    setStringProperty(hasPopUp, AXPropertyName::HasPopUp);
    notifyAttributeChanged(aria_haspopupAttr);
}

String AccessibleNode::invalid() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Invalid);
}

void AccessibleNode::setInvalid(const String& invalid)
{
    setStringProperty(invalid, AXPropertyName::Invalid);
    notifyAttributeChanged(aria_invalidAttr);
}

String AccessibleNode::keyShortcuts() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::KeyShortcuts);
}

void AccessibleNode::setKeyShortcuts(const String& keyShortcuts)
{
    setStringProperty(keyShortcuts, AXPropertyName::KeyShortcuts);
    notifyAttributeChanged(aria_keyshortcutsAttr);
}

String AccessibleNode::live() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Live);
}

void AccessibleNode::setLive(const String& live)
{
    setStringProperty(live, AXPropertyName::Live);
    notifyAttributeChanged(aria_liveAttr);
}

String AccessibleNode::label() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Label);
}

void AccessibleNode::setLabel(const String& label)
{
    setStringProperty(label, AXPropertyName::Label);
    notifyAttributeChanged(aria_labelAttr);
}

String AccessibleNode::orientation() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Orientation);
}

void AccessibleNode::setOrientation(const String& orientation)
{
    setStringProperty(orientation, AXPropertyName::Orientation);
    notifyAttributeChanged(aria_orientationAttr);
}

String AccessibleNode::placeholder() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Placeholder);
}

void AccessibleNode::setPlaceholder(const String& placeholder)
{
    setStringProperty(placeholder, AXPropertyName::Placeholder);
    notifyAttributeChanged(aria_placeholderAttr);
}

String AccessibleNode::pressed() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Pressed);
}

void AccessibleNode::setPressed(const String& pressed)
{
    setStringProperty(pressed, AXPropertyName::Pressed);
    notifyAttributeChanged(aria_pressedAttr);
}

String AccessibleNode::relevant() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Relevant);
}

void AccessibleNode::setRelevant(const String& relevant)
{
    setStringProperty(relevant, AXPropertyName::Relevant);
    notifyAttributeChanged(aria_relevantAttr);
}

String AccessibleNode::role() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Role);
}

void AccessibleNode::setRole(const String& role)
{
    setStringProperty(role, AXPropertyName::Role);
    notifyAttributeChanged(roleAttr);
}

String AccessibleNode::roleDescription() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::RoleDescription);
}

void AccessibleNode::setRoleDescription(const String& roleDescription)
{
    setStringProperty(roleDescription, AXPropertyName::RoleDescription);
    notifyAttributeChanged(aria_roledescriptionAttr);
}

String AccessibleNode::sort() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Sort);
}

void AccessibleNode::setSort(const String& sort)
{
    setStringProperty(sort, AXPropertyName::Sort);
    notifyAttributeChanged(aria_sortAttr);
}

String AccessibleNode::valueText() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::ValueText);
}

void AccessibleNode::setValueText(const String& valueText)
{
    setStringProperty(valueText, AXPropertyName::ValueText);
    notifyAttributeChanged(aria_valuetextAttr);
}

} // namespace WebCore
