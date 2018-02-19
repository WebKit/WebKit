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
            { AXPropertyName::ActiveDescendant, aria_activedescendantAttr },
            { AXPropertyName::Atomic, aria_atomicAttr },
            { AXPropertyName::Autocomplete, aria_autocompleteAttr },
            { AXPropertyName::Busy, aria_busyAttr },
            { AXPropertyName::Checked, aria_checkedAttr },
            { AXPropertyName::ColCount, aria_colcountAttr },
            { AXPropertyName::ColIndex, aria_colindexAttr },
            { AXPropertyName::ColSpan, aria_colspanAttr },
            { AXPropertyName::Current, aria_currentAttr },
            { AXPropertyName::Details, aria_detailsAttr },
            { AXPropertyName::Disabled, aria_disabledAttr },
            { AXPropertyName::ErrorMessage, aria_errormessageAttr },
            { AXPropertyName::Expanded, aria_expandedAttr },
            { AXPropertyName::HasPopUp, aria_haspopupAttr },
            { AXPropertyName::Hidden, aria_hiddenAttr },
            { AXPropertyName::Invalid, aria_invalidAttr },
            { AXPropertyName::KeyShortcuts, aria_keyshortcutsAttr },
            { AXPropertyName::Label, aria_labelAttr },
            { AXPropertyName::Level, aria_levelAttr },
            { AXPropertyName::Live, aria_liveAttr },
            { AXPropertyName::Modal, aria_modalAttr },
            { AXPropertyName::Multiline, aria_multilineAttr },
            { AXPropertyName::Multiselectable, aria_multiselectableAttr },
            { AXPropertyName::Orientation, aria_orientationAttr },
            { AXPropertyName::Placeholder, aria_placeholderAttr },
            { AXPropertyName::PosInSet, aria_posinsetAttr },
            { AXPropertyName::Pressed, aria_pressedAttr },
            { AXPropertyName::ReadOnly, aria_readonlyAttr },
            { AXPropertyName::Relevant, aria_relevantAttr },
            { AXPropertyName::Required, aria_requiredAttr },
            { AXPropertyName::Role, roleAttr },
            { AXPropertyName::RoleDescription, aria_roledescriptionAttr },
            { AXPropertyName::RowCount, aria_rowcountAttr },
            { AXPropertyName::RowIndex, aria_rowindexAttr },
            { AXPropertyName::RowSpan, aria_rowspanAttr },
            { AXPropertyName::Selected, aria_selectedAttr },
            { AXPropertyName::SetSize, aria_setsizeAttr },
            { AXPropertyName::Sort, aria_sortAttr },
            { AXPropertyName::ValueMax, aria_valuemaxAttr },
            { AXPropertyName::ValueMin, aria_valueminAttr },
            { AXPropertyName::ValueNow, aria_valuenowAttr },
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

static bool isPropertyValueBoolean(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::Atomic:
    case AXPropertyName::Busy:
    case AXPropertyName::Disabled:
    case AXPropertyName::Expanded:
    case AXPropertyName::Hidden:
    case AXPropertyName::Modal:
    case AXPropertyName::Multiline:
    case AXPropertyName::Multiselectable:
    case AXPropertyName::ReadOnly:
    case AXPropertyName::Required:
    case AXPropertyName::Selected:
        return true;
    default:
        return false;
    }
}

static bool isPropertyValueInt(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::ColCount:
    case AXPropertyName::RowCount:
    case AXPropertyName::SetSize:
        return true;
    default:
        return false;
    }
}

static bool isPropertyValueUnsigned(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::ColIndex:
    case AXPropertyName::ColSpan:
    case AXPropertyName::Level:
    case AXPropertyName::PosInSet:
    case AXPropertyName::RowIndex:
    case AXPropertyName::RowSpan:
        return true;
    default:
        return false;
    }
}

static bool isPropertyValueFloat(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::ValueMax:
    case AXPropertyName::ValueMin:
    case AXPropertyName::ValueNow:
        return true;
    default:
        return false;
    }
}

static bool isPropertyValueRelation(AXPropertyName propertyName)
{
    switch (propertyName) {
    case AXPropertyName::ActiveDescendant:
    case AXPropertyName::Details:
    case AXPropertyName::ErrorMessage:
        return true;
    default:
        return false;
    }
}

QualifiedName AccessibleNode::attributeFromAXPropertyName(AXPropertyName propertyName)
{
    return ariaAttributeMap().get(propertyName);
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

void AccessibleNode::setProperty(AXPropertyName propertyName, PropertyValueVariant&& value, bool shouldRemove)
{
    if (shouldRemove) {
        m_propertyMap.remove(propertyName);
        return;
    }
    m_propertyMap.set(propertyName, value);
}

template<typename T>
void AccessibleNode::setOptionalProperty(AXPropertyName propertyName, std::optional<T> optional)
{
    setProperty(propertyName, optional.value(), !optional.has_value());
}

void AccessibleNode::setStringProperty(AXPropertyName propertyName, const String& value)
{
    setProperty(propertyName, value, value.isEmpty());
}

void AccessibleNode::setRelationProperty(AXPropertyName propertyName, AccessibleNode* value)
{
    Vector<RefPtr<AccessibleNode>> accessibleNodes;
    if (value)
        accessibleNodes.append(value);
    setProperty(propertyName, accessibleNodes, !value);
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

template<typename T>
std::optional<T> AccessibleNode::optionalValueForProperty(Element& element, AXPropertyName propertyName)
{
    const PropertyValueVariant&& variant = AccessibleNode::valueForProperty(element, propertyName);
    if (WTF::holds_alternative<std::nullptr_t>(variant))
        return std::nullopt;
    if (WTF::holds_alternative<T>(variant))
        return WTF::get<T>(variant);
    return std::nullopt;
}

std::optional<bool> AccessibleNode::effectiveBoolValueForElement(Element& element, AXPropertyName propertyName)
{
    auto value = optionalValueForProperty<bool>(element, propertyName);
    if (value)
        return *value;

    AtomicString attributeValue;
    if (ariaAttributeMap().contains(propertyName) && isPropertyValueBoolean(propertyName))
        attributeValue = element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName));

    if (equalIgnoringASCIICase(attributeValue, "true"))
        return true;
    if (equalIgnoringASCIICase(attributeValue, "false"))
        return false;
    return std::nullopt;
}

int AccessibleNode::effectiveIntValueForElement(Element& element, AXPropertyName propertyName)
{
    auto value = optionalValueForProperty<int>(element, propertyName);
    if (value)
        return *value;

    if (ariaAttributeMap().contains(propertyName) && isPropertyValueInt(propertyName))
        return element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName)).toInt();

    return 0;
}

unsigned AccessibleNode::effectiveUnsignedValueForElement(Element& element, AXPropertyName propertyName)
{
    auto value = optionalValueForProperty<unsigned>(element, propertyName);
    if (value)
        return *value;

    if (ariaAttributeMap().contains(propertyName) && isPropertyValueUnsigned(propertyName)) {
        const String& value = element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName));
        return value.toUInt();
    }

    return 0;
}

double AccessibleNode::effectiveDoubleValueForElement(Element& element, AXPropertyName propertyName)
{
    auto value = optionalValueForProperty<double>(element, propertyName);
    if (value)
        return *value;

    if (ariaAttributeMap().contains(propertyName) && isPropertyValueFloat(propertyName))
        return element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName)).toDouble();

    return 0.0;
}

RefPtr<AccessibleNode> AccessibleNode::singleRelationValueForProperty(Element& element, AXPropertyName propertyName)
{
    Vector<RefPtr<AccessibleNode>> accessibleNodes = relationsValueForProperty(element, propertyName);
    size_t size = accessibleNodes.size();
    ASSERT(!size || size == 1);
    if (size)
        return accessibleNodes.first();
    return nullptr;
}

Vector<RefPtr<AccessibleNode>> AccessibleNode::relationsValueForProperty(Element& element, AXPropertyName propertyName)
{
    const PropertyValueVariant&& variant = AccessibleNode::valueForProperty(element, propertyName);
    if (WTF::holds_alternative<Vector<RefPtr<AccessibleNode>>>(variant))
        return WTF::get<Vector<RefPtr<AccessibleNode>>>(variant);
    return Vector<RefPtr<AccessibleNode>> { };
}

Vector<RefPtr<Element>> AccessibleNode::effectiveElementsValueForElement(Element& element, AXPropertyName propertyName)
{
    Vector<RefPtr<Element>> elements;
    auto value = relationsValueForProperty(element, propertyName);
    if (value.size()) {
        for (auto accessibleNode : value)
            elements.append(&accessibleNode->m_ownerElement);
        return elements;
    }

    if (ariaAttributeMap().contains(propertyName) && isPropertyValueRelation(propertyName)) {
        const AtomicString& attrStr = element.attributeWithoutSynchronization(ariaAttributeMap().get(propertyName));
        if (attrStr.isNull() || attrStr.isEmpty())
            return elements;
        auto spaceSplitString = SpaceSplitString(attrStr, false);
        size_t length = spaceSplitString.size();
        for (size_t i = 0; i < length; ++i) {
            if (auto* idElement = element.treeScope().getElementById(spaceSplitString[i]))
                elements.append(idElement);
        }
    }
    return elements;
}

void AccessibleNode::notifyAttributeChanged(const WebCore::QualifiedName& name)
{
    if (AXObjectCache* cache = m_ownerElement.document().axObjectCache())
        cache->deferAttributeChangeIfNeeded(name, &m_ownerElement);
}

RefPtr<AccessibleNode> AccessibleNode::activeDescendant() const
{
    return singleRelationValueForProperty(m_ownerElement, AXPropertyName::ActiveDescendant);
}

void AccessibleNode::setActiveDescendant(AccessibleNode* value)
{
    setRelationProperty(AXPropertyName::ActiveDescendant, value);
    notifyAttributeChanged(aria_activedescendantAttr);
}

std::optional<bool> AccessibleNode::atomic() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Atomic);
}

void AccessibleNode::setAtomic(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Atomic, value);
    notifyAttributeChanged(aria_atomicAttr);
}

String AccessibleNode::autocomplete() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Autocomplete);
}

void AccessibleNode::setAutocomplete(const String& autocomplete)
{
    setStringProperty(AXPropertyName::Autocomplete, autocomplete);
    notifyAttributeChanged(aria_autocompleteAttr);
}

std::optional<bool> AccessibleNode::busy() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Busy);
}

void AccessibleNode::setBusy(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Busy, value);
    notifyAttributeChanged(aria_busyAttr);
}

String AccessibleNode::checked() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Checked);
}

void AccessibleNode::setChecked(const String& checked)
{
    setStringProperty(AXPropertyName::Checked, checked);
    notifyAttributeChanged(aria_checkedAttr);
}

std::optional<int> AccessibleNode::colCount() const
{
    return optionalValueForProperty<int>(m_ownerElement, AXPropertyName::ColCount);
}

void AccessibleNode::setColCount(std::optional<int> value)
{
    setOptionalProperty<int>(AXPropertyName::ColCount, value);
    notifyAttributeChanged(aria_colcountAttr);
}

std::optional<unsigned> AccessibleNode::colIndex() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::ColCount);
}

void AccessibleNode::setColIndex(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::ColIndex, value);
    notifyAttributeChanged(aria_colindexAttr);
}

std::optional<unsigned> AccessibleNode::colSpan() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::ColSpan);
}

void AccessibleNode::setColSpan(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::ColSpan, value);
    notifyAttributeChanged(aria_colspanAttr);
}

String AccessibleNode::current() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Current);
}

void AccessibleNode::setCurrent(const String& current)
{
    setStringProperty(AXPropertyName::Current, current);
    notifyAttributeChanged(aria_currentAttr);
}

RefPtr<AccessibleNode> AccessibleNode::details() const
{
    return singleRelationValueForProperty(m_ownerElement, AXPropertyName::Details);
}

void AccessibleNode::setDetails(AccessibleNode* value)
{
    setRelationProperty(AXPropertyName::Details, value);
    notifyAttributeChanged(aria_detailsAttr);
}

std::optional<bool> AccessibleNode::disabled() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Disabled);
}

RefPtr<AccessibleNode> AccessibleNode::errorMessage() const
{
    return singleRelationValueForProperty(m_ownerElement, AXPropertyName::ErrorMessage);
}

void AccessibleNode::setErrorMessage(AccessibleNode* value)
{
    setRelationProperty(AXPropertyName::ErrorMessage, value);
    notifyAttributeChanged(aria_errormessageAttr);
}


void AccessibleNode::setDisabled(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Disabled, value);
    notifyAttributeChanged(aria_disabledAttr);
}

std::optional<bool> AccessibleNode::expanded() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Expanded);
}

void AccessibleNode::setExpanded(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Expanded, value);
    notifyAttributeChanged(aria_expandedAttr);
}

String AccessibleNode::hasPopUp() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::HasPopUp);
}

void AccessibleNode::setHasPopUp(const String& hasPopUp)
{
    setStringProperty(AXPropertyName::HasPopUp, hasPopUp);
    notifyAttributeChanged(aria_haspopupAttr);
}

std::optional<bool> AccessibleNode::hidden() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Hidden);
}

void AccessibleNode::setHidden(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Hidden, value);
    notifyAttributeChanged(aria_hiddenAttr);
}

String AccessibleNode::invalid() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Invalid);
}

void AccessibleNode::setInvalid(const String& invalid)
{
    setStringProperty(AXPropertyName::Invalid, invalid);
    notifyAttributeChanged(aria_invalidAttr);
}

String AccessibleNode::keyShortcuts() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::KeyShortcuts);
}

void AccessibleNode::setKeyShortcuts(const String& keyShortcuts)
{
    setStringProperty(AXPropertyName::KeyShortcuts, keyShortcuts);
    notifyAttributeChanged(aria_keyshortcutsAttr);
}

String AccessibleNode::label() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Label);
}

void AccessibleNode::setLabel(const String& label)
{
    setStringProperty(AXPropertyName::Label, label);
    notifyAttributeChanged(aria_labelAttr);
}

std::optional<unsigned> AccessibleNode::level() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::Level);
}

void AccessibleNode::setLevel(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::Level, value);
    notifyAttributeChanged(aria_levelAttr);
}

String AccessibleNode::live() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Live);
}

void AccessibleNode::setLive(const String& live)
{
    setStringProperty(AXPropertyName::Live, live);
    notifyAttributeChanged(aria_liveAttr);
}

std::optional<bool> AccessibleNode::modal() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Modal);
}

void AccessibleNode::setModal(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Modal, value);
    notifyAttributeChanged(aria_modalAttr);
}

std::optional<bool> AccessibleNode::multiline() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Multiline);
}

void AccessibleNode::setMultiline(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Multiline, value);
    notifyAttributeChanged(aria_multilineAttr);
}

std::optional<bool> AccessibleNode::multiselectable() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Multiselectable);
}

void AccessibleNode::setMultiselectable(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Multiselectable, value);
    notifyAttributeChanged(aria_multiselectableAttr);
}

String AccessibleNode::orientation() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Orientation);
}

void AccessibleNode::setOrientation(const String& orientation)
{
    setStringProperty(AXPropertyName::Orientation, orientation);
    notifyAttributeChanged(aria_orientationAttr);
}

String AccessibleNode::placeholder() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Placeholder);
}

void AccessibleNode::setPlaceholder(const String& placeholder)
{
    setStringProperty(AXPropertyName::Placeholder, placeholder);
    notifyAttributeChanged(aria_placeholderAttr);
}

std::optional<unsigned> AccessibleNode::posInSet() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::PosInSet);
}

void AccessibleNode::setPosInSet(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::PosInSet, value);
    notifyAttributeChanged(aria_posinsetAttr);
}

String AccessibleNode::pressed() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Pressed);
}

void AccessibleNode::setPressed(const String& pressed)
{
    setStringProperty(AXPropertyName::Pressed, pressed);
    notifyAttributeChanged(aria_pressedAttr);
}

std::optional<bool> AccessibleNode::readOnly() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::ReadOnly);
}

void AccessibleNode::setReadOnly(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::ReadOnly, value);
    notifyAttributeChanged(aria_readonlyAttr);
}

String AccessibleNode::relevant() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Relevant);
}

void AccessibleNode::setRelevant(const String& relevant)
{
    setStringProperty(AXPropertyName::Relevant, relevant);
    notifyAttributeChanged(aria_relevantAttr);
}

std::optional<bool> AccessibleNode::required() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Required);
}

void AccessibleNode::setRequired(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Required, value);
    notifyAttributeChanged(aria_requiredAttr);
}

String AccessibleNode::role() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Role);
}

void AccessibleNode::setRole(const String& role)
{
    setStringProperty(AXPropertyName::Role, role);
    notifyAttributeChanged(roleAttr);
}

String AccessibleNode::roleDescription() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::RoleDescription);
}

void AccessibleNode::setRoleDescription(const String& roleDescription)
{
    setStringProperty(AXPropertyName::RoleDescription, roleDescription);
    notifyAttributeChanged(aria_roledescriptionAttr);
}

std::optional<int> AccessibleNode::rowCount() const
{
    return optionalValueForProperty<int>(m_ownerElement, AXPropertyName::RowCount);
}

void AccessibleNode::setRowCount(std::optional<int> value)
{
    setOptionalProperty<int>(AXPropertyName::RowCount, value);
    notifyAttributeChanged(aria_rowcountAttr);
}

std::optional<unsigned> AccessibleNode::rowIndex() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::RowCount);
}

void AccessibleNode::setRowIndex(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::RowIndex, value);
    notifyAttributeChanged(aria_rowindexAttr);
}

std::optional<unsigned> AccessibleNode::rowSpan() const
{
    return optionalValueForProperty<unsigned>(m_ownerElement, AXPropertyName::RowSpan);
}

void AccessibleNode::setRowSpan(std::optional<unsigned> value)
{
    setOptionalProperty<unsigned>(AXPropertyName::RowSpan, value);
    notifyAttributeChanged(aria_rowspanAttr);
}

std::optional<bool> AccessibleNode::selected() const
{
    return optionalValueForProperty<bool>(m_ownerElement, AXPropertyName::Selected);
}

void AccessibleNode::setSelected(std::optional<bool> value)
{
    setOptionalProperty<bool>(AXPropertyName::Selected, value);
    notifyAttributeChanged(aria_selectedAttr);
}

std::optional<int> AccessibleNode::setSize() const
{
    return optionalValueForProperty<int>(m_ownerElement, AXPropertyName::SetSize);
}

void AccessibleNode::setSetSize(std::optional<int> value)
{
    setOptionalProperty<int>(AXPropertyName::SetSize, value);
    notifyAttributeChanged(aria_setsizeAttr);
}

String AccessibleNode::sort() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::Sort);
}

void AccessibleNode::setSort(const String& sort)
{
    setStringProperty(AXPropertyName::Sort, sort);
    notifyAttributeChanged(aria_sortAttr);
}

std::optional<double> AccessibleNode::valueMax() const
{
    return optionalValueForProperty<double>(m_ownerElement, AXPropertyName::ValueMax);
}

void AccessibleNode::setValueMax(std::optional<double> value)
{
    setOptionalProperty<double>(AXPropertyName::ValueMax, value);
    notifyAttributeChanged(aria_valuemaxAttr);
}

std::optional<double> AccessibleNode::valueMin() const
{
    return optionalValueForProperty<double>(m_ownerElement, AXPropertyName::ValueMin);
}

void AccessibleNode::setValueMin(std::optional<double> value)
{
    setOptionalProperty<double>(AXPropertyName::ValueMin, value);
    notifyAttributeChanged(aria_valueminAttr);
}

std::optional<double> AccessibleNode::valueNow() const
{
    return optionalValueForProperty<double>(m_ownerElement, AXPropertyName::ValueNow);
}

void AccessibleNode::setValueNow(std::optional<double> value)
{
    setOptionalProperty<double>(AXPropertyName::ValueNow, value);
    notifyAttributeChanged(aria_valuenowAttr);
}

String AccessibleNode::valueText() const
{
    return stringValueForProperty(m_ownerElement, AXPropertyName::ValueText);
}

void AccessibleNode::setValueText(const String& valueText)
{
    setStringProperty(AXPropertyName::ValueText, valueText);
    notifyAttributeChanged(aria_valuetextAttr);
}

} // namespace WebCore
