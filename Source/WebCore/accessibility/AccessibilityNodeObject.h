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

    bool isBusy() const override;
    bool isControl() const override;
    bool isFieldset() const override;
    bool isGroup() const override;
    bool isHeading() const override;
    bool isHovered() const override;
    bool isInputImage() const override;
    bool isLink() const override;
    bool isMenu() const override;
    bool isMenuBar() const override;
    bool isMenuButton() const override;
    bool isMenuItem() const override;
    bool isMenuRelated() const override;
    bool isMultiSelectable() const override;
    virtual bool isNativeCheckboxOrRadio() const;
    bool isNativeImage() const;
    bool isNativeTextControl() const override;
    bool isPasswordField() const override;
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

    void setFocused(bool) override;
    bool isFocused() const override;
    bool canSetFocusAttribute() const override;
    unsigned headingLevel() const override;

    bool canSetValueAttribute() const override;

    String valueDescription() const override;
    float valueForRange() const override;
    float maxValueForRange() const override;
    float minValueForRange() const override;
    float stepValueForRange() const override;

    AccessibilityOrientation orientation() const override;

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
    void alternativeText(Vector<AccessibilityText>&) const;
    void helpText(Vector<AccessibilityText>&) const;
    String stringValue() const override;
    SRGBA<uint8_t> colorValue() const override;
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

    void updateRole() override;

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
    enum class TreatStyleFormatGroupAsInline {
        No,
        Yes
    };
    AccessibilityRole determineAccessibilityRoleFromNode(TreatStyleFormatGroupAsInline = TreatStyleFormatGroupAsInline::No) const;
    AccessibilityRole ariaRoleAttribute() const override;
    virtual AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;

    void addChildren() override;
    void clearChildren() override;
    void updateChildrenIfNecessary() override;
    bool canHaveChildren() const override;
    bool isDescendantOfBarrenParent() const override;

    enum class StepAction : bool { Decrement, Increment };
    void alterRangeValue(StepAction);
    void changeValueByStep(StepAction);
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;

    bool elementAttributeValue(const QualifiedName&) const;

    const String liveRegionStatus() const override;
    const String liveRegionRelevant() const override;
    bool liveRegionAtomic() const override;

    bool isLabelable() const;
    AccessibilityObject* correspondingControlForLabelElement() const override;
    AccessibilityObject* correspondingLabelForControlElement() const override;
    HTMLLabelElement* labelForElement(Element*) const;
    String textForLabelElement(Element*) const;
    HTMLLabelElement* labelElementContainer() const;

    String ariaAccessibilityDescription() const;
    Vector<Element*> ariaLabeledByElements() const;
    String descriptionForElements(Vector<Element*>&&) const;
    LayoutRect boundingBoxRect() const override;
    String ariaDescribedByAttribute() const override;
    
    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const;
    AccessibilityObject* menuButtonForMenu() const;
    AccessibilityObject* captionForFigure() const;
    virtual void titleElementText(Vector<AccessibilityText>&) const;
    bool exposesTitleUIElement() const override;

private:
    bool isAccessibilityNodeObject() const final { return true; }
    void accessibilityText(Vector<AccessibilityText>&) const override;
    void visibleText(Vector<AccessibilityText>&) const;
    String alternativeTextForWebArea() const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    bool computeAccessibilityIsIgnored() const override;
    bool usesAltTagForTextComputation() const;
    bool roleIgnoresTitle() const;
    bool postKeyboardKeysForValueChange(StepAction);
    void setNodeValue(StepAction, float);
    bool performDismissAction() final;
    bool hasTextAlternative() const;

    void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    bool needsToUpdateChildren() const override { return m_childrenDirty; }
    void setNeedsToUpdateSubtree() override { m_subtreeDirty = true; }

    bool isDescendantOfElementType(const HashSet<QualifiedName>&) const;

    Node* m_node;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityNodeObject, isAccessibilityNodeObject())
