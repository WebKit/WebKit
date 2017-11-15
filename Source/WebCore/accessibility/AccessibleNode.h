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
    Role,
    Label
};

struct AXPropertyHashTraits : WTF::GenericHashTraits<AXPropertyName> {
    static const bool emptyValueIsZero = false;
    static AXPropertyName emptyValue() { return AXPropertyName::None; };
    static void constructDeletedValue(AXPropertyName& slot)
    {
        slot = AXPropertyName::None;
    }
    static bool isDeletedValue(AXPropertyName value)
    {
        return value == AXPropertyName::None;
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

    String role() const;
    void setRole(const String&);

    String label() const;
    void setLabel(const String&);

private:
    static const PropertyValueVariant valueForProperty(Element&, AXPropertyName);
    static const String stringValueForProperty(Element&, AXPropertyName);
    void setStringProperty(const String&, AXPropertyName);

    Element& m_ownerElement;

    HashMap<AXPropertyName, PropertyValueVariant, WTF::IntHash<AXPropertyName>, AXPropertyHashTraits> m_propertyMap;
};

} // namespace WebCore

