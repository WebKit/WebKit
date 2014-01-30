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

#ifndef AccessibilityRenderObject_h
#define AccessibilityRenderObject_h

#include "AccessibilityNodeObject.h"
#include "LayoutRect.h"
#include <wtf/Forward.h>

namespace WebCore {
    
class AccessibilitySVGRoot;
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
    
class AccessibilityRenderObject : public AccessibilityNodeObject {
protected:
    explicit AccessibilityRenderObject(RenderObject*);
public:
    static PassRefPtr<AccessibilityRenderObject> create(RenderObject*);
    virtual ~AccessibilityRenderObject();
    
    virtual bool isAccessibilityRenderObject() const override { return true; }

    virtual void init() override;
    
    virtual bool isAttachment() const override;
    virtual bool isFileUploadButton() const override;

    virtual bool isSelected() const override;
    virtual bool isFocused() const override;
    virtual bool isLoaded() const override;
    virtual bool isOffScreen() const override;
    virtual bool isReadOnly() const override;
    virtual bool isUnvisited() const override;
    virtual bool isVisited() const override;
    virtual bool isLinked() const override;
    virtual bool hasBoldFont() const override;
    virtual bool hasItalicFont() const override;
    virtual bool hasPlainText() const override;
    virtual bool hasSameFont(RenderObject*) const override;
    virtual bool hasSameFontColor(RenderObject*) const override;
    virtual bool hasSameStyle(RenderObject*) const override;
    virtual bool hasUnderline() const override;

    virtual bool canSetTextRangeAttributes() const override;
    virtual bool canSetValueAttribute() const override;
    virtual bool canSetExpandedAttribute() const override;

    virtual void setAccessibleName(const AtomicString&) override;
    
    // Provides common logic used by all elements when determining isIgnored.
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const override;
    
    virtual int layoutCount() const override;
    virtual double estimatedLoadingProgress() const override;
    
    virtual AccessibilityObject* firstChild() const override;
    virtual AccessibilityObject* lastChild() const override;
    virtual AccessibilityObject* previousSibling() const override;
    virtual AccessibilityObject* nextSibling() const override;
    virtual AccessibilityObject* parentObject() const override;
    virtual AccessibilityObject* parentObjectIfExists() const override;
    virtual AccessibilityObject* observableObject() const override;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const override;
    virtual bool exposesTitleUIElement() const override;
    virtual AccessibilityObject* titleUIElement() const override;
    virtual AccessibilityObject* correspondingControlForLabelElement() const override;
    virtual AccessibilityObject* correspondingLabelForControlElement() const override;

    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const override;
    virtual bool supportsARIAOwns() const override;
    virtual bool isPresentationalChildOfAriaRole() const override;
    virtual bool ariaRoleHasPresentationalChildren() const override;
    
    // Should be called on the root accessibility object to kick off a hit test.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const override;

    virtual Element* anchorElement() const override;
    
    virtual LayoutRect boundingBoxRect() const override;
    virtual LayoutRect elementRect() const override;
    virtual IntPoint clickPoint() override;
    
    void setRenderer(RenderObject*);
    virtual RenderObject* renderer() const override { return m_renderer; }
    RenderBoxModelObject* renderBoxModelObject() const;
    virtual Node* node() const override;

    virtual Document* document() const override;

    RenderView* topRenderer() const;
    RenderTextControl* textControl() const;
    HTMLLabelElement* labelElementContainer() const;
    
    virtual URL url() const override;
    virtual PlainTextRange selectedTextRange() const override;
    virtual VisibleSelection selection() const override;
    virtual String stringValue() const override;
    virtual String helpText() const override;
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    virtual String text() const override;
    virtual int textLength() const override;
    virtual String selectedText() const override;
    virtual const AtomicString& accessKey() const override;
    virtual const String& actionVerb() const;
    virtual Widget* widget() const override;
    virtual Widget* widgetForAttachmentView() const override;
    virtual void getDocumentLinks(AccessibilityChildrenVector&);
    virtual FrameView* documentFrameView() const override;

    virtual void clearChildren() override;
    virtual void updateChildrenIfNecessary() override;
    
    virtual void setFocused(bool) override;
    virtual void setSelectedTextRange(const PlainTextRange&) override;
    virtual void setValue(const String&) override;
    virtual void setSelectedRows(AccessibilityChildrenVector&) override;
    virtual AccessibilityOrientation orientation() const override;
    
    virtual void detach(AccessibilityDetachmentType, AXObjectCache*) override;
    virtual void textChanged() override;
    virtual void addChildren() override;
    virtual bool canHaveChildren() const override;
    virtual void selectedChildren(AccessibilityChildrenVector&) override;
    virtual void visibleChildren(AccessibilityChildrenVector&) override;
    virtual void tabChildren(AccessibilityChildrenVector&) override;
    virtual bool shouldFocusActiveDescendant() const override;
    bool shouldNotifyActiveDescendant() const;
    virtual AccessibilityObject* activeDescendant() const override;
    virtual void handleActiveDescendantChanged() override;
    virtual void handleAriaExpandedChanged() override;
    
    virtual VisiblePositionRange visiblePositionRange() const override;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const override;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override;
    virtual bool supportsARIAFlowTo() const override;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const override;
    virtual bool supportsARIADescribedBy() const override;
    virtual void ariaDescribedByElements(AccessibilityChildrenVector&) const override;
    virtual bool ariaHasPopup() const override;

    virtual bool supportsARIADropping() const override;
    virtual bool supportsARIADragging() const override;
    virtual bool isARIAGrabbed() override;
    virtual void determineARIADropEffects(Vector<String>&) override;
    
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const override;
    virtual VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const override;
    virtual int index(const VisiblePosition&) const override;

    virtual VisiblePosition visiblePositionForIndex(int) const override;
    virtual int indexForVisiblePosition(const VisiblePosition&) const override;

    virtual void lineBreaks(Vector<int>&) const override;
    virtual PlainTextRange doAXRangeForLine(unsigned) const override;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const override;
    
    virtual String doAXStringForRange(const PlainTextRange&) const override;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const override;
    
    virtual String stringValueForMSAA() const override;
    virtual String stringRoleForMSAA() const override;
    virtual String nameForMSAA() const override;
    virtual String descriptionForMSAA() const override;
    virtual AccessibilityRole roleValueForMSAA() const override;

    virtual String passwordFieldValue() const override;

protected:
    RenderObject* m_renderer;
    
    void setRenderObject(RenderObject* renderer) { m_renderer = renderer; }
    bool needsToUpdateChildren() const { return m_childrenDirty; }
    virtual ScrollableArea* getScrollableAreaIfScrollable() const override;
    virtual void scrollTo(const IntPoint&) const override;
    
    virtual bool isDetached() const override { return !m_renderer; }

    virtual AccessibilityRole determineAccessibilityRole() override;
    virtual bool computeAccessibilityIsIgnored() const override;

private:
    void ariaElementsFromAttribute(AccessibilityChildrenVector&, const QualifiedName&) const;
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    void ariaListboxVisibleChildren(AccessibilityChildrenVector&);
    bool isAllowedChildOfTree() const;
    bool hasTextAlternative() const;
    String positionalDescriptionForMSAA() const;
    PlainTextRange ariaSelectedTextRange() const;
    Element* rootEditableElementForPosition(const Position&) const;
    bool nodeIsTextControl(const Node*) const;
    virtual void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    virtual Path elementPath() const override;
    
    bool isTabItemSelected() const;
    LayoutRect checkboxOrRadioRect() const;
    void addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement*) const;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const override;

    bool renderObjectIsObservable(RenderObject*) const;
    RenderObject* renderParentObject() const;
    bool isDescendantOfElementType(const QualifiedName& tagName) const;
    
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AccessibilitySVGRoot* remoteSVGRootElement() const;
    AccessibilityObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    virtual bool supportsPath() const override;

    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
#if PLATFORM(MAC)
    void updateAttachmentViewParents();
#endif

    void ariaSelectedRows(AccessibilityChildrenVector&);
    
    bool elementAttributeValue(const QualifiedName&) const;
    void setElementAttributeValue(const QualifiedName&, bool);
    
    virtual ESpeak speakProperty() const override;
    
    virtual const AtomicString& ariaLiveRegionStatus() const override;
    virtual const AtomicString& ariaLiveRegionRelevant() const override;
    virtual bool ariaLiveRegionAtomic() const override;
    virtual bool ariaLiveRegionBusy() const override;

    bool inheritsPresentationalRole() const;

#if ENABLE(MATHML)
    // All math elements return true for isMathElement().
    virtual bool isMathElement() const override;
    virtual bool isMathFraction() const override;
    virtual bool isMathFenced() const override;
    virtual bool isMathSubscriptSuperscript() const override;
    virtual bool isMathRow() const override;
    virtual bool isMathUnderOver() const override;
    virtual bool isMathRoot() const override;
    virtual bool isMathSquareRoot() const override;
    virtual bool isMathText() const override;
    virtual bool isMathNumber() const override;
    virtual bool isMathOperator() const override;
    virtual bool isMathFenceOperator() const override;
    virtual bool isMathSeparatorOperator() const override;
    virtual bool isMathIdentifier() const override;
    virtual bool isMathTable() const override;
    virtual bool isMathTableRow() const override;
    virtual bool isMathTableCell() const override;
    virtual bool isMathMultiscript() const override;
    
    // Generic components.
    virtual AccessibilityObject* mathBaseObject() override;
    
    // Root components.
    virtual AccessibilityObject* mathRadicandObject() override;
    virtual AccessibilityObject* mathRootIndexObject() override;
    
    // Fraction components.
    virtual AccessibilityObject* mathNumeratorObject() override;
    virtual AccessibilityObject* mathDenominatorObject() override;

    // Under over components.
    virtual AccessibilityObject* mathUnderObject() override;
    virtual AccessibilityObject* mathOverObject() override;
    
    // Subscript/superscript components.
    virtual AccessibilityObject* mathSubscriptObject() override;
    virtual AccessibilityObject* mathSuperscriptObject() override;
    
    // Fenced components.
    virtual String mathFencedOpenString() const override;
    virtual String mathFencedCloseString() const override;
    virtual int mathLineThickness() const override;

    // Multiscripts components.
    virtual void mathPrescripts(AccessibilityMathMultiscriptPairs&) override;
    virtual void mathPostscripts(AccessibilityMathMultiscriptPairs&) override;
    
    bool isIgnoredElementWithinMathTree() const;
#endif
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilityRenderObject, isAccessibilityRenderObject())

} // namespace WebCore

#endif // AccessibilityRenderObject_h
