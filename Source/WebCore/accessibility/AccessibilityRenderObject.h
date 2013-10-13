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
    
    virtual bool isAccessibilityRenderObject() const OVERRIDE { return true; }

    virtual void init() OVERRIDE;
    
    virtual bool isAttachment() const OVERRIDE;
    virtual bool isFileUploadButton() const OVERRIDE;

    virtual bool isSelected() const OVERRIDE;
    virtual bool isFocused() const OVERRIDE;
    virtual bool isLoaded() const OVERRIDE;
    virtual bool isOffScreen() const OVERRIDE;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isUnvisited() const OVERRIDE;
    virtual bool isVisited() const OVERRIDE;
    virtual bool isLinked() const OVERRIDE;
    virtual bool hasBoldFont() const OVERRIDE;
    virtual bool hasItalicFont() const OVERRIDE;
    virtual bool hasPlainText() const OVERRIDE;
    virtual bool hasSameFont(RenderObject*) const OVERRIDE;
    virtual bool hasSameFontColor(RenderObject*) const OVERRIDE;
    virtual bool hasSameStyle(RenderObject*) const OVERRIDE;
    virtual bool hasUnderline() const OVERRIDE;

    virtual bool canSetTextRangeAttributes() const OVERRIDE;
    virtual bool canSetValueAttribute() const OVERRIDE;
    virtual bool canSetExpandedAttribute() const OVERRIDE;

    virtual void setAccessibleName(const AtomicString&) OVERRIDE;
    
    // Provides common logic used by all elements when determining isIgnored.
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const OVERRIDE;
    
    virtual int layoutCount() const OVERRIDE;
    virtual double estimatedLoadingProgress() const OVERRIDE;
    
    virtual AccessibilityObject* firstChild() const OVERRIDE;
    virtual AccessibilityObject* lastChild() const OVERRIDE;
    virtual AccessibilityObject* previousSibling() const OVERRIDE;
    virtual AccessibilityObject* nextSibling() const OVERRIDE;
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    virtual AccessibilityObject* parentObjectIfExists() const OVERRIDE;
    virtual AccessibilityObject* observableObject() const OVERRIDE;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual bool exposesTitleUIElement() const OVERRIDE;
    virtual AccessibilityObject* titleUIElement() const OVERRIDE;
    virtual AccessibilityObject* correspondingControlForLabelElement() const OVERRIDE;
    virtual AccessibilityObject* correspondingLabelForControlElement() const OVERRIDE;

    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual bool supportsARIAOwns() const OVERRIDE;
    virtual bool isPresentationalChildOfAriaRole() const OVERRIDE;
    virtual bool ariaRoleHasPresentationalChildren() const OVERRIDE;
    
    // Should be called on the root accessibility object to kick off a hit test.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const OVERRIDE;

    virtual Element* anchorElement() const OVERRIDE;
    
    virtual LayoutRect boundingBoxRect() const OVERRIDE;
    virtual LayoutRect elementRect() const OVERRIDE;
    virtual IntPoint clickPoint() OVERRIDE;
    
    void setRenderer(RenderObject*);
    virtual RenderObject* renderer() const OVERRIDE { return m_renderer; }
    RenderBoxModelObject* renderBoxModelObject() const;
    virtual Node* node() const OVERRIDE;

    virtual Document* document() const OVERRIDE;

    RenderView* topRenderer() const;
    RenderTextControl* textControl() const;
    Document* topDocument() const;
    HTMLLabelElement* labelElementContainer() const;
    
    virtual URL url() const OVERRIDE;
    virtual PlainTextRange selectedTextRange() const OVERRIDE;
    virtual VisibleSelection selection() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;
    virtual String helpText() const OVERRIDE;
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const OVERRIDE;
    virtual String text() const OVERRIDE;
    virtual int textLength() const OVERRIDE;
    virtual String selectedText() const OVERRIDE;
    virtual const AtomicString& accessKey() const OVERRIDE;
    virtual const String& actionVerb() const;
    virtual Widget* widget() const OVERRIDE;
    virtual Widget* widgetForAttachmentView() const OVERRIDE;
    virtual void getDocumentLinks(AccessibilityChildrenVector&);
    virtual FrameView* documentFrameView() const OVERRIDE;

    virtual void clearChildren() OVERRIDE;
    virtual void updateChildrenIfNecessary() OVERRIDE;
    
    virtual void setFocused(bool) OVERRIDE;
    virtual void setSelectedTextRange(const PlainTextRange&) OVERRIDE;
    virtual void setValue(const String&) OVERRIDE;
    virtual void setSelectedRows(AccessibilityChildrenVector&) OVERRIDE;
    virtual AccessibilityOrientation orientation() const OVERRIDE;
    
    virtual void detach() OVERRIDE;
    virtual void textChanged() OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    virtual void selectedChildren(AccessibilityChildrenVector&) OVERRIDE;
    virtual void visibleChildren(AccessibilityChildrenVector&) OVERRIDE;
    virtual void tabChildren(AccessibilityChildrenVector&) OVERRIDE;
    virtual bool shouldFocusActiveDescendant() const OVERRIDE;
    bool shouldNotifyActiveDescendant() const;
    virtual AccessibilityObject* activeDescendant() const OVERRIDE;
    virtual void handleActiveDescendantChanged() OVERRIDE;
    virtual void handleAriaExpandedChanged() OVERRIDE;
    
    virtual VisiblePositionRange visiblePositionRange() const OVERRIDE;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const OVERRIDE;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const OVERRIDE;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const OVERRIDE;
    virtual bool supportsARIAFlowTo() const OVERRIDE;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual bool ariaHasPopup() const OVERRIDE;

    virtual bool supportsARIADropping() const OVERRIDE;
    virtual bool supportsARIADragging() const OVERRIDE;
    virtual bool isARIAGrabbed() OVERRIDE;
    virtual void determineARIADropEffects(Vector<String>&) OVERRIDE;
    
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const OVERRIDE;
    virtual VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const OVERRIDE;
    virtual int index(const VisiblePosition&) const OVERRIDE;

    virtual VisiblePosition visiblePositionForIndex(int) const OVERRIDE;
    virtual int indexForVisiblePosition(const VisiblePosition&) const OVERRIDE;

    virtual void lineBreaks(Vector<int>&) const OVERRIDE;
    virtual PlainTextRange doAXRangeForLine(unsigned) const OVERRIDE;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const OVERRIDE;
    
    virtual String doAXStringForRange(const PlainTextRange&) const OVERRIDE;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const OVERRIDE;
    
    virtual String stringValueForMSAA() const OVERRIDE;
    virtual String stringRoleForMSAA() const OVERRIDE;
    virtual String nameForMSAA() const OVERRIDE;
    virtual String descriptionForMSAA() const OVERRIDE;
    virtual AccessibilityRole roleValueForMSAA() const OVERRIDE;

    virtual String passwordFieldValue() const OVERRIDE;

protected:
    RenderObject* m_renderer;
    
    void setRenderObject(RenderObject* renderer) { m_renderer = renderer; }
    bool needsToUpdateChildren() const { return m_childrenDirty; }
    virtual ScrollableArea* getScrollableAreaIfScrollable() const OVERRIDE;
    virtual void scrollTo(const IntPoint&) const OVERRIDE;
    
    virtual bool isDetached() const OVERRIDE { return !m_renderer; }

    virtual AccessibilityRole determineAccessibilityRole() OVERRIDE;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;

private:
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    void ariaListboxVisibleChildren(AccessibilityChildrenVector&);
    bool isAllowedChildOfTree() const;
    bool hasTextAlternative() const;
    String positionalDescriptionForMSAA() const;
    PlainTextRange ariaSelectedTextRange() const;
    Element* rootEditableElementForPosition(const Position&) const;
    bool nodeIsTextControl(const Node*) const;
    virtual void setNeedsToUpdateChildren() OVERRIDE { m_childrenDirty = true; }
    virtual Path elementPath() const OVERRIDE;
    
    bool isTabItemSelected() const;
    LayoutRect checkboxOrRadioRect() const;
    void addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement*) const;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const OVERRIDE;

    bool renderObjectIsObservable(RenderObject*) const;
    RenderObject* renderParentObject() const;
    bool isDescendantOfElementType(const QualifiedName& tagName) const;
    
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AccessibilitySVGRoot* remoteSVGRootElement() const;
    AccessibilityObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    virtual bool supportsPath() const OVERRIDE;

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
    
    virtual ESpeak speakProperty() const OVERRIDE;
    
    virtual const AtomicString& ariaLiveRegionStatus() const OVERRIDE;
    virtual const AtomicString& ariaLiveRegionRelevant() const OVERRIDE;
    virtual bool ariaLiveRegionAtomic() const OVERRIDE;
    virtual bool ariaLiveRegionBusy() const OVERRIDE;

    bool inheritsPresentationalRole() const;

#if ENABLE(MATHML)
    // All math elements return true for isMathElement().
    virtual bool isMathElement() const OVERRIDE;
    virtual bool isMathFraction() const OVERRIDE;
    virtual bool isMathFenced() const OVERRIDE;
    virtual bool isMathSubscriptSuperscript() const OVERRIDE;
    virtual bool isMathRow() const OVERRIDE;
    virtual bool isMathUnderOver() const OVERRIDE;
    virtual bool isMathRoot() const OVERRIDE;
    virtual bool isMathSquareRoot() const OVERRIDE;
    virtual bool isMathText() const OVERRIDE;
    virtual bool isMathNumber() const OVERRIDE;
    virtual bool isMathOperator() const OVERRIDE;
    virtual bool isMathFenceOperator() const OVERRIDE;
    virtual bool isMathSeparatorOperator() const OVERRIDE;
    virtual bool isMathIdentifier() const OVERRIDE;
    virtual bool isMathTable() const OVERRIDE;
    virtual bool isMathTableRow() const OVERRIDE;
    virtual bool isMathTableCell() const OVERRIDE;
    virtual bool isMathMultiscript() const OVERRIDE;
    
    // Generic components.
    virtual AccessibilityObject* mathBaseObject() OVERRIDE;
    
    // Root components.
    virtual AccessibilityObject* mathRadicandObject() OVERRIDE;
    virtual AccessibilityObject* mathRootIndexObject() OVERRIDE;
    
    // Fraction components.
    virtual AccessibilityObject* mathNumeratorObject() OVERRIDE;
    virtual AccessibilityObject* mathDenominatorObject() OVERRIDE;

    // Under over components.
    virtual AccessibilityObject* mathUnderObject() OVERRIDE;
    virtual AccessibilityObject* mathOverObject() OVERRIDE;
    
    // Subscript/superscript components.
    virtual AccessibilityObject* mathSubscriptObject() OVERRIDE;
    virtual AccessibilityObject* mathSuperscriptObject() OVERRIDE;
    
    // Fenced components.
    virtual String mathFencedOpenString() const OVERRIDE;
    virtual String mathFencedCloseString() const OVERRIDE;
    virtual int mathLineThickness() const OVERRIDE;

    // Multiscripts components.
    virtual void mathPrescripts(AccessibilityMathMultiscriptPairs&) OVERRIDE;
    virtual void mathPostscripts(AccessibilityMathMultiscriptPairs&) OVERRIDE;
    
    bool isIgnoredElementWithinMathTree() const;
#endif
};

inline AccessibilityRenderObject* toAccessibilityRenderObject(AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityRenderObject());
    return static_cast<AccessibilityRenderObject*>(object);
}

inline const AccessibilityRenderObject* toAccessibilityRenderObject(const AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityRenderObject());
    return static_cast<const AccessibilityRenderObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toAccessibilityRenderObject(const AccessibilityRenderObject*);

} // namespace WebCore

#endif // AccessibilityRenderObject_h
