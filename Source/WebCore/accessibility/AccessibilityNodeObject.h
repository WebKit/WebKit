/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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
#include "LayoutRect.h"
#include <wtf/Forward.h>

namespace WebCore {
    
class AXObjectCache;
class Element;
class HTMLLabelElement;
class Node;

enum MouseButtonListenerResultFilter {
    ExcludeBodyElement = 1,
    IncludeBodyElement,
};

class AccessibilityNodeObject : public AccessibilityObject {
public:
    static Ref<AccessibilityNodeObject> create(Node*);
    virtual ~AccessibilityNodeObject();

    void init() override;

    bool canvasHasFallbackContent() const override;

    bool isControl() const override;
    bool isFieldset() const override;
    bool isGroup() const override;
    bool isHeading() const override;
    bool isHovered() const override;
    bool isImageButton() const override;
    bool isInputImage() const override;
    bool isLink() const override;
    bool isMenu() const override;
    bool isMenuBar() const override;
    bool isMenuButton() const override;
    bool isMenuItem() const override;
    bool isMenuRelated() const override;
    bool isMultiSelectable() const override;
    virtual bool isNativeCheckboxOrRadio() const;
    bool isNativeImage() const override;
    bool isNativeTextControl() const override;
    bool isPasswordField() const override;
    AccessibilityObject* passwordFieldOrContainingPasswordField() override;
    bool isProgressIndicator() const override;
    bool isSearchField() const override;
    bool isSlider() const override;

    bool isChecked() const override;
    bool isEnabled() const override;
    bool isIndeterminate() const override;
    bool isPressed() const override;
    bool isRequired() const override;
    bool supportsRequiredAttribute() const override;

    bool canSetSelectedAttribute() const override;

    void setNode(Node*);
    Node* node() const override { return m_node; }
    Document* document() const override;

    bool canSetFocusAttribute() const override;
    int headingLevel() const override;

    bool canSetValueAttribute() const override;

    String valueDescription() const override;
    float valueForRange() const override;
    float maxValueForRange() const override;
    float minValueForRange() const override;
    float stepValueForRange() const override;

    AXCoreObject* selectedRadioButton() override;
    AXCoreObject* selectedTabItem() override;
    AccessibilityButtonState checkboxOrRadioValue() const override;

    unsigned hierarchicalLevel() const override;
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    String accessibilityDescriptionForChildren() const;
    String accessibilityDescription() const override;
    String helpText() const override;
    String title() const override;
    String text() const override;
    String stringValue() const override;
    void colorValue(int& r, int& g, int& b) const override;
    String ariaLabeledByAttribute() const override;
    bool hasAttributesRequiredForInclusion() const final;
    void setIsExpanded(bool) override;

    Element* actionElement() const override;
    Element* mouseButtonListener(MouseButtonListenerResultFilter = ExcludeBodyElement) const;
    Element* anchorElement() const override;
    AccessibilityObject* menuForMenuButton() const;
   
    virtual void changeValueByPercent(float percentChange);
 
    AccessibilityObject* firstChild() const override;
    AccessibilityObject* lastChild() const override;
    AccessibilityObject* previousSibling() const override;
    AccessibilityObject* nextSibling() const override;
    AccessibilityObject* parentObject() const override;
    AccessibilityObject* parentObjectIfExists() const override;

    void childrenChanged() override;
    void updateAccessibilityRole() override;

    void increment() override;
    void decrement() override;

    LayoutRect elementRect() const override;

protected:
    explicit AccessibilityNodeObject(Node*);
    void detachRemoteParts(AccessibilityDetachmentType) override;

    AccessibilityRole m_ariaRole { AccessibilityRole::Unknown };
    mutable AccessibilityRole m_roleForMSAA { AccessibilityRole::Unknown };
#ifndef NDEBUG
    bool m_initialized { false };
#endif

    bool isDetached() const override { return !m_node; }

    virtual AccessibilityRole determineAccessibilityRole();
    void addChildren() override;

    bool canHaveChildren() const override;
    AccessibilityRole ariaRoleAttribute() const override;
    virtual AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool isDescendantOfBarrenParent() const override;
    void alterSliderValue(bool increase);
    void changeValueByStep(bool increase);
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    bool isLabelable() const;
    HTMLLabelElement* labelForElement(Element*) const;
    String textForLabelElement(Element*) const;
    String ariaAccessibilityDescription() const;
    void ariaLabeledByElements(Vector<Element*>& elements) const;
    String accessibilityDescriptionForElements(Vector<Element*> &elements) const;
    LayoutRect boundingBoxRect() const override;
    String ariaDescribedByAttribute() const override;
    
    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const;
    AccessibilityObject* menuButtonForMenu() const;
    AccessibilityObject* captionForFigure() const;
    virtual void titleElementText(Vector<AccessibilityText>&) const;

private:
    bool isAccessibilityNodeObject() const final { return true; }
    void accessibilityText(Vector<AccessibilityText>&) const override;
    void alternativeText(Vector<AccessibilityText>&) const;
    void visibleText(Vector<AccessibilityText>&) const;
    void helpText(Vector<AccessibilityText>&) const;
    String alternativeTextForWebArea() const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    bool computeAccessibilityIsIgnored() const override;
    bool usesAltTagForTextComputation() const;
    bool roleIgnoresTitle() const;
    bool postKeyboardKeysForValueChange(bool increase);
    void setNodeValue(bool increase, float value);
    bool performDismissAction() final;
    
    Node* m_node;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityNodeObject, isAccessibilityNodeObject())
