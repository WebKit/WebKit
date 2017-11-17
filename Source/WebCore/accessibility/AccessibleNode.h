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

typedef Variant<String, bool, int> PropertyValueVariant;

enum class AXPropertyName {
    None,
    Autocomplete,
    Checked,
    Current,
    HasPopUp,
    Invalid,
    KeyShortcuts,
    Label,
    Live,
    Orientation,
    Placeholder,
    Pressed,
    Relevant,
    Role,
    RoleDescription,
    Sort,
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

    static const String effectiveStringValueForElement(Element&, AXPropertyName);
    static bool hasProperty(Element&, AXPropertyName);

    String autocomplete() const;
    void setAutocomplete(const String&);

    String checked() const;
    void setChecked(const String&);

    String current() const;
    void setCurrent(const String&);

    String hasPopUp() const;
    void setHasPopUp(const String&);

    String invalid() const;
    void setInvalid(const String&);

    String keyShortcuts() const;
    void setKeyShortcuts(const String&);

    String live() const;
    void setLive(const String&);

    String label() const;
    void setLabel(const String&);

    String orientation() const;
    void setOrientation(const String&);

    String placeholder() const;
    void setPlaceholder(const String&);

    String pressed() const;
    void setPressed(const String&);

    String relevant() const;
    void setRelevant(const String&);

    String role() const;
    void setRole(const String&);

    String roleDescription() const;
    void setRoleDescription(const String&);

    String sort() const;
    void setSort(const String&);

    String valueText() const;
    void setValueText(const String&);

private:
    static const PropertyValueVariant valueForProperty(Element&, AXPropertyName);
    static const String stringValueForProperty(Element&, AXPropertyName);
    void setStringProperty(const String&, AXPropertyName);
    
    void notifyAttributeChanged(const WebCore::QualifiedName&);

    Element& m_ownerElement;

    HashMap<AXPropertyName, PropertyValueVariant, WTF::IntHash<AXPropertyName>, AXPropertyHashTraits> m_propertyMap;
};

} // namespace WebCore

