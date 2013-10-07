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

    virtual void init() OVERRIDE;
    
    virtual bool isAccessibilityNodeObject() const OVERRIDE { return true; }

    virtual bool canvasHasFallbackContent() const OVERRIDE;

    virtual bool isAnchor() const OVERRIDE;
    virtual bool isControl() const OVERRIDE;
    virtual bool isFieldset() const OVERRIDE;
    virtual bool isGroup() const OVERRIDE;
    virtual bool isHeading() const OVERRIDE;
    virtual bool isHovered() const OVERRIDE;
    virtual bool isImage() const OVERRIDE;
    virtual bool isImageButton() const OVERRIDE;
    virtual bool isInputImage() const OVERRIDE;
    virtual bool isLink() const OVERRIDE;
    virtual bool isMenu() const OVERRIDE;
    virtual bool isMenuBar() const OVERRIDE;
    virtual bool isMenuButton() const OVERRIDE;
    virtual bool isMenuItem() const OVERRIDE;
    virtual bool isMenuRelated() const OVERRIDE;
    virtual bool isMultiSelectable() const OVERRIDE;
    virtual bool isNativeCheckboxOrRadio() const;
    virtual bool isNativeImage() const OVERRIDE;
    virtual bool isNativeTextControl() const OVERRIDE;
    virtual bool isPasswordField() const OVERRIDE;
    virtual bool isProgressIndicator() const OVERRIDE;
    virtual bool isSearchField() const OVERRIDE;
    virtual bool isSlider() const OVERRIDE;

    virtual bool isChecked() const OVERRIDE;
    virtual bool isEnabled() const OVERRIDE;
    virtual bool isIndeterminate() const OVERRIDE;
    virtual bool isPressed() const OVERRIDE;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isRequired() const OVERRIDE;
    virtual bool supportsRequiredAttribute() const OVERRIDE;

    virtual bool canSetSelectedAttribute() const OVERRIDE;

    void setNode(Node*);
    virtual Node* node() const OVERRIDE { return m_node; }
    virtual Document* document() const OVERRIDE;

    virtual bool canSetFocusAttribute() const OVERRIDE;
    virtual int headingLevel() const OVERRIDE;

    virtual String valueDescription() const OVERRIDE;
    virtual float valueForRange() const OVERRIDE;
    virtual float maxValueForRange() const OVERRIDE;
    virtual float minValueForRange() const OVERRIDE;
    virtual float stepValueForRange() const OVERRIDE;

    virtual AccessibilityObject* selectedRadioButton() OVERRIDE;
    virtual AccessibilityObject* selectedTabItem() OVERRIDE;
    virtual AccessibilityButtonState checkboxOrRadioValue() const OVERRIDE;

    virtual unsigned hierarchicalLevel() const OVERRIDE;
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;
    virtual String helpText() const OVERRIDE;
    virtual String title() const OVERRIDE;
    virtual String text() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;
    virtual void colorValue(int& r, int& g, int& b) const OVERRIDE;
    virtual String ariaLabeledByAttribute() const OVERRIDE;

    virtual Element* actionElement() const OVERRIDE;
    Element* mouseButtonListener() const;
    virtual Element* anchorElement() const OVERRIDE;
    AccessibilityObject* menuForMenuButton() const;
   
    virtual void changeValueByPercent(float percentChange);
 
    virtual AccessibilityObject* firstChild() const OVERRIDE;
    virtual AccessibilityObject* lastChild() const OVERRIDE;
    virtual AccessibilityObject* previousSibling() const OVERRIDE;
    virtual AccessibilityObject* nextSibling() const OVERRIDE;
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    virtual AccessibilityObject* parentObjectIfExists() const OVERRIDE;

    virtual void detach() OVERRIDE;
    virtual void childrenChanged() OVERRIDE;
    virtual void updateAccessibilityRole() OVERRIDE;

    virtual void increment() OVERRIDE;
    virtual void decrement() OVERRIDE;

    virtual LayoutRect elementRect() const OVERRIDE;

protected:
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
    mutable AccessibilityRole m_roleForMSAA;
#ifndef NDEBUG
    bool m_initialized;
#endif

    virtual bool isDetached() const OVERRIDE { return !m_node; }

    virtual AccessibilityRole determineAccessibilityRole();
    virtual void addChildren() OVERRIDE;
    virtual void addChild(AccessibilityObject*) OVERRIDE;
    virtual void insertChild(AccessibilityObject*, unsigned index) OVERRIDE;

    virtual bool canHaveChildren() const OVERRIDE;
    virtual AccessibilityRole ariaRoleAttribute() const OVERRIDE;
    AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool hasContentEditableAttributeSet() const;
    virtual bool isDescendantOfBarrenParent() const OVERRIDE;
    void alterSliderValue(bool increase);
    void changeValueByStep(bool increase);
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    HTMLLabelElement* labelForElement(Element*) const;
    String ariaAccessibilityDescription() const;
    void ariaLabeledByElements(Vector<Element*>& elements) const;
    String accessibilityDescriptionForElements(Vector<Element*> &elements) const;
    void elementsFromAttribute(Vector<Element*>& elements, const QualifiedName&) const;
    virtual LayoutRect boundingBoxRect() const OVERRIDE;
    virtual String ariaDescribedByAttribute() const OVERRIDE;
    
    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const;
    AccessibilityObject* menuButtonForMenu() const;

private:
    Node* m_node;

    virtual void accessibilityText(Vector<AccessibilityText>&) OVERRIDE;
    virtual void titleElementText(Vector<AccessibilityText>&) const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void visibleText(Vector<AccessibilityText>&) const;
    void helpText(Vector<AccessibilityText>&) const;
    String alternativeTextForWebArea() const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};

inline AccessibilityNodeObject* toAccessibilityNodeObject(AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityNodeObject());
    return static_cast<AccessibilityNodeObject*>(object);
}

inline const AccessibilityNodeObject* toAccessibilityNodeObject(const AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityNodeObject());
    return static_cast<const AccessibilityNodeObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toAccessibilityNodeObject(const AccessibilityNodeObject*);

} // namespace WebCore

#endif // AccessibilityNodeObject_h
