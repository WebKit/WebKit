/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "AccessibilityObject.h"

namespace WebCore {

class HTMLElement;
class HTMLSelectElement;

class AccessibilityListBoxOption final : public AccessibilityObject {
public:
    static Ref<AccessibilityListBoxOption> create(HTMLElement&);
    virtual ~AccessibilityListBoxOption();

    bool isSelected() const final;
    void setSelected(bool) final;

private:
    explicit AccessibilityListBoxOption(HTMLElement&);

    AccessibilityRole roleValue() const final { return AccessibilityRole::ListBoxOption; }
    bool isEnabled() const final;
    bool isSelectedOptionActive() const final;
    String stringValue() const final;
    Element* actionElement() const final;
    Node* node() const final;
    bool canSetSelectedAttribute() const final;

    LayoutRect elementRect() const final;
    AccessibilityObject* parentObject() const final;

    bool isListBoxOption() const final { return true; }
    bool canHaveChildren() const final { return false; }
    HTMLSelectElement* listBoxOptionParentNode() const;
    int listBoxOptionIndex() const;
    IntRect listBoxOptionRect() const;
    AccessibilityObject* listBoxOptionAccessibilityObject(HTMLElement*) const;
    bool computeAccessibilityIsIgnored() const final;

    WeakPtr<HTMLElement> m_optionElement;
};
    
} // namespace WebCore 

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityListBoxOption, isListBoxOption())
