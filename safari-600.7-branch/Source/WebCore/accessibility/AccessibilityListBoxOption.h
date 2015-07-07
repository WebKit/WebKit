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

#ifndef AccessibilityListBoxOption_h
#define AccessibilityListBoxOption_h

#include "AccessibilityObject.h"
#include "HTMLElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class AccessibilityListBox;
class Element;
class HTMLElement;
class HTMLSelectElement;
    
class AccessibilityListBoxOption : public AccessibilityObject {

private:
    AccessibilityListBoxOption();
public:
    static PassRefPtr<AccessibilityListBoxOption> create();
    virtual ~AccessibilityListBoxOption();
    
    void setHTMLElement(HTMLElement* element) { m_optionElement = element; }
    
    virtual AccessibilityRole roleValue() const override { return ListBoxOptionRole; }
    virtual bool isSelected() const override;
    virtual bool isEnabled() const override;
    virtual bool isSelectedOptionActive() const override;
    virtual String stringValue() const override;
    virtual Element* actionElement() const override;
    virtual Node* node() const override { return m_optionElement; }
    virtual void setSelected(bool) override;
    virtual bool canSetSelectedAttribute() const override;

    virtual LayoutRect elementRect() const override;
    virtual AccessibilityObject* parentObject() const override;
    virtual bool isListBoxOption() const override final { return true; }

private:
    HTMLElement* m_optionElement;
    
    virtual bool canHaveChildren() const override { return false; }
    HTMLSelectElement* listBoxOptionParentNode() const;
    int listBoxOptionIndex() const;
    IntRect listBoxOptionRect() const;
    AccessibilityObject* listBoxOptionAccessibilityObject(HTMLElement*) const;
    virtual bool computeAccessibilityIsIgnored() const override;
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilityListBoxOption, isListBoxOption())
    
} // namespace WebCore 

#endif // AccessibilityListBoxOption_h
