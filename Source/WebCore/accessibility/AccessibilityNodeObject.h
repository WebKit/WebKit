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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef AccessibilityNodeObject_h
#define AccessibilityNodeObject_h

#include "AccessibilityObject.h"
#include "LayoutRect.h"
#include <wtf/Forward.h>

namespace WebCore {
    
class AXObjectCache;
class Element;
class Frame;
class FrameView;
class HitTestResult;
class HTMLAnchorElement;
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class HTMLSelectElement;
class IntPoint;
class IntSize;
class Node;
class RenderListBox;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class Widget;
    
class AccessibilityNodeObject : public AccessibilityObject {
protected:
    explicit AccessibilityNodeObject(Node*);
public:
    static PassRefPtr<AccessibilityNodeObject> create(Node*);
    virtual ~AccessibilityNodeObject();

    virtual void init() override;
    
    virtual bool isAccessibilityNodeObject() const override { return true; }

    virtual bool canvasHasFallbackContent() const override;

    virtual bool isAnchor() const override;
    virtual bool isControl() const override;
    virtual bool isFieldset() const override;
    virtual bool isGroup() const override;
    virtual bool isHeading() const override;
    virtual bool isHovered() const override;
    virtual bool isImage() const override;
    virtual bool isImageButton() const override;
    virtual bool isInputImage() const override;
    virtual bool isLink() const override;
    virtual bool isMenu() const override;
    virtual bool isMenuBar() const override;
    virtual bool isMenuButton() const override;
    virtual bool isMenuItem() const override;
    virtual bool isMenuRelated() const override;
    virtual bool isMultiSelectable() const override;
    virtual bool isNativeCheckboxOrRadio() const;
    virtual bool isNativeImage() const override;
    virtual bool isNativeTextControl() const override;
    virtual bool isPasswordField() const override;
    virtual bool isProgressIndicator() const override;
    virtual bool isSearchField() const override;
    virtual bool isSlider() const override;

    virtual bool isChecked() const override;
    virtual bool isEnabled() const override;
    virtual bool isIndeterminate() const override;
    virtual bool isPressed() const override;
    virtual bool isReadOnly() const override;
    virtual bool isRequired() const override;
    virtual bool supportsRequiredAttribute() const override;

    virtual bool canSetSelectedAttribute() const override;

    void setNode(Node*);
    virtual Node* node() const override { return m_node; }
    virtual Document* document() const override;

    virtual bool canSetFocusAttribute() const override;
    virtual int headingLevel() const override;

    virtual String valueDescription() const override;
    virtual float valueForRange() const override;
    virtual float maxValueForRange() const override;
    virtual float minValueForRange() const override;
    virtual float stepValueForRange() const override;

    virtual AccessibilityObject* selectedRadioButton() override;
    virtual AccessibilityObject* selectedTabItem() override;
    virtual AccessibilityButtonState checkboxOrRadioValue() const override;

    virtual unsigned hierarchicalLevel() const override;
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    virtual String accessibilityDescription() const override;
    virtual String helpText() const override;
    virtual String title() const override;
    virtual String text() const override;
    virtual String stringValue() const override;
    virtual void colorValue(int& r, int& g, int& b) const override;
    virtual String ariaLabeledByAttribute() const override;
    virtual bool hasAttributesRequiredForInclusion() const override final;

    virtual Element* actionElement() const override;
    Element* mouseButtonListener() const;
    virtual Element* anchorElement() const override;
    AccessibilityObject* menuForMenuButton() const;
   
    virtual void changeValueByPercent(float percentChange);
 
    virtual AccessibilityObject* firstChild() const override;
    virtual AccessibilityObject* lastChild() const override;
    virtual AccessibilityObject* previousSibling() const override;
    virtual AccessibilityObject* nextSibling() const override;
    virtual AccessibilityObject* parentObject() const override;
    virtual AccessibilityObject* parentObjectIfExists() const override;

    virtual void detach(AccessibilityDetachmentType, AXObjectCache*) override;
    virtual void childrenChanged() override;
    virtual void updateAccessibilityRole() override;

    virtual void increment() override;
    virtual void decrement() override;

    virtual LayoutRect elementRect() const override;

protected:
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
    mutable AccessibilityRole m_roleForMSAA;
#ifndef NDEBUG
    bool m_initialized;
#endif

    virtual bool isDetached() const override { return !m_node; }

    virtual AccessibilityRole determineAccessibilityRole();
    virtual void addChildren() override;
    virtual void addChild(AccessibilityObject*) override;
    virtual void insertChild(AccessibilityObject*, unsigned index) override;

    virtual bool canHaveChildren() const override;
    virtual AccessibilityRole ariaRoleAttribute() const override;
    AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool hasContentEditableAttributeSet() const;
    virtual bool isDescendantOfBarrenParent() const override;
    void alterSliderValue(bool increase);
    void changeValueByStep(bool increase);
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    HTMLLabelElement* labelForElement(Element*) const;
    String ariaAccessibilityDescription() const;
    void ariaLabeledByElements(Vector<Element*>& elements) const;
    String accessibilityDescriptionForElements(Vector<Element*> &elements) const;
    void elementsFromAttribute(Vector<Element*>& elements, const QualifiedName&) const;
    virtual LayoutRect boundingBoxRect() const override;
    virtual String ariaDescribedByAttribute() const override;
    
    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const;
    AccessibilityObject* menuButtonForMenu() const;

private:
    Node* m_node;

    virtual void accessibilityText(Vector<AccessibilityText>&) override;
    virtual void titleElementText(Vector<AccessibilityText>&) const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void visibleText(Vector<AccessibilityText>&) const;
    void helpText(Vector<AccessibilityText>&) const;
    String alternativeTextForWebArea() const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    virtual bool computeAccessibilityIsIgnored() const override;
    bool usesAltTagForTextComputation() const;
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilityNodeObject, isAccessibilityNodeObject())

} // namespace WebCore

#endif // AccessibilityNodeObject_h
