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

#pragma once

#include "Element.h"
#include "ScriptWrappable.h"
#include <wtf/HashMap.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef Variant<std::nullptr_t, String, bool, int, unsigned, double, Vector<RefPtr<AccessibleNode>>> PropertyValueVariant;

enum class AXPropertyName {
    None,
    ActiveDescendant,
    Atomic,
    Autocomplete,
    Busy,
    Checked,
    ColCount,
    ColIndex,
    ColSpan,
    Current,
    Details,
    Disabled,
    ErrorMessage,
    Expanded,
    HasPopUp,
    Hidden,
    Invalid,
    KeyShortcuts,
    Label,
    Level,
    Live,
    Modal,
    Multiline,
    Multiselectable,
    Orientation,
    Placeholder,
    PosInSet,
    Pressed,
    ReadOnly,
    Relevant,
    Required,
    Role,
    RoleDescription,
    RowCount,
    RowIndex,
    RowSpan,
    Selected,
    SetSize,
    Sort,
    ValueMax,
    ValueMin,
    ValueNow,
    ValueText
};

struct AXPropertyHashTraits : WTF::GenericHashTraits<AXPropertyName> {
    static const bool emptyValueIsZero = false;
    static AXPropertyName emptyValue() { return AXPropertyName::None; };
    static void constructDeletedValue(AXPropertyName& slot)
    {
        slot = static_cast<AXPropertyName>(static_cast<int>(AXPropertyName::None) - 1);
    }
    static bool isDeletedValue(AXPropertyName value)
    {
        return static_cast<int>(value) == static_cast<int>(AXPropertyName::None) - 1;
    }
};

class AccessibleNode : public ScriptWrappable {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AccessibleNode(Element& element)
        : m_ownerElement(element)
    {
    }

    void ref() { m_ownerElement.ref(); }
    void deref() { m_ownerElement.deref(); }

    static QualifiedName attributeFromAXPropertyName(AXPropertyName);

    static const String effectiveStringValueForElement(Element&, AXPropertyName);
    static std::optional<bool> effectiveBoolValueForElement(Element&, AXPropertyName);
    static int effectiveIntValueForElement(Element&, AXPropertyName);
    static unsigned effectiveUnsignedValueForElement(Element&, AXPropertyName);
    static double effectiveDoubleValueForElement(Element&, AXPropertyName);
    static Vector<RefPtr<Element>> effectiveElementsValueForElement(Element&, AXPropertyName);
    static Vector<RefPtr<AccessibleNode>> relationsValueForProperty(Element&, AXPropertyName);
    static bool hasProperty(Element&, AXPropertyName);

    RefPtr<AccessibleNode> activeDescendant() const;
    void setActiveDescendant(AccessibleNode*);

    std::optional<bool> atomic() const;
    void setAtomic(std::optional<bool>);
    
    String autocomplete() const;
    void setAutocomplete(const String&);

    std::optional<bool> busy() const;
    void setBusy(std::optional<bool>);

    String checked() const;
    void setChecked(const String&);

    std::optional<int> colCount() const;
    void setColCount(std::optional<int>);

    std::optional<unsigned> colIndex() const;
    void setColIndex(std::optional<unsigned>);

    std::optional<unsigned> colSpan() const;
    void setColSpan(std::optional<unsigned>);

    String current() const;
    void setCurrent(const String&);

    RefPtr<AccessibleNode> details() const;
    void setDetails(AccessibleNode*);

    std::optional<bool> disabled() const;
    void setDisabled(std::optional<bool>);

    RefPtr<AccessibleNode> errorMessage() const;
    void setErrorMessage(AccessibleNode*);

    std::optional<bool> expanded() const;
    void setExpanded(std::optional<bool>);

    String hasPopUp() const;
    void setHasPopUp(const String&);

    std::optional<bool> hidden() const;
    void setHidden(std::optional<bool>);

    String invalid() const;
    void setInvalid(const String&);

    String keyShortcuts() const;
    void setKeyShortcuts(const String&);

    String label() const;
    void setLabel(const String&);

    std::optional<unsigned> level() const;
    void setLevel(std::optional<unsigned>);

    String live() const;
    void setLive(const String&);

    std::optional<bool> modal() const;
    void setModal(std::optional<bool>);

    std::optional<bool> multiline() const;
    void setMultiline(std::optional<bool>);

    std::optional<bool> multiselectable() const;
    void setMultiselectable(std::optional<bool>);

    String orientation() const;
    void setOrientation(const String&);

    String placeholder() const;
    void setPlaceholder(const String&);

    std::optional<unsigned> posInSet() const;
    void setPosInSet(std::optional<unsigned>);
    
    String pressed() const;
    void setPressed(const String&);

    std::optional<bool> readOnly() const;
    void setReadOnly(std::optional<bool>);

    String relevant() const;
    void setRelevant(const String&);

    std::optional<bool> required() const;
    void setRequired(std::optional<bool>);

    String role() const;
    void setRole(const String&);

    String roleDescription() const;
    void setRoleDescription(const String&);

    std::optional<int> rowCount() const;
    void setRowCount(std::optional<int>);

    std::optional<unsigned> rowIndex() const;
    void setRowIndex(std::optional<unsigned>);

    std::optional<unsigned> rowSpan() const;
    void setRowSpan(std::optional<unsigned>);

    std::optional<bool> selected() const;
    void setSelected(std::optional<bool>);

    std::optional<int> setSize() const;
    void setSetSize(std::optional<int>);

    String sort() const;
    void setSort(const String&);

    std::optional<double> valueMax() const;
    void setValueMax(std::optional<double>);

    std::optional<double> valueMin() const;
    void setValueMin(std::optional<double>);

    std::optional<double> valueNow() const;
    void setValueNow(std::optional<double>);

    String valueText() const;
    void setValueText(const String&);

private:
    static const PropertyValueVariant valueForProperty(Element&, AXPropertyName);
    static const String stringValueForProperty(Element&, AXPropertyName);
    template<typename T> static std::optional<T> optionalValueForProperty(Element&, AXPropertyName);
    static RefPtr<AccessibleNode> singleRelationValueForProperty(Element&, AXPropertyName);
    
    void setProperty(AXPropertyName, PropertyValueVariant&&, bool);
    template<typename T> void setOptionalProperty(AXPropertyName, std::optional<T>);
    void setStringProperty(AXPropertyName, const String&);
    void setRelationProperty(AXPropertyName, AccessibleNode*);
    
    void notifyAttributeChanged(const WebCore::QualifiedName&);

    Element& m_ownerElement;

    HashMap<AXPropertyName, PropertyValueVariant, WTF::IntHash<AXPropertyName>, AXPropertyHashTraits> m_propertyMap;
};

} // namespace WebCore

