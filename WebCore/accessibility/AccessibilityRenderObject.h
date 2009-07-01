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

#include "AccessibilityObject.h"

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
class RenderObject;
class RenderListBox;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class String;
class Widget;
    
class AccessibilityRenderObject : public AccessibilityObject {
protected:
    AccessibilityRenderObject(RenderObject*);
public:
    static PassRefPtr<AccessibilityRenderObject> create(RenderObject*);
    virtual ~AccessibilityRenderObject();
    
    bool isAccessibilityRenderObject() const { return true; };
    
    virtual bool isAnchor() const;
    virtual bool isAttachment() const;
    virtual bool isHeading() const;
    virtual bool isLink() const;
    virtual bool isImageButton() const;
    virtual bool isImage() const;
    virtual bool isNativeImage() const;
    virtual bool isPasswordField() const;
    virtual bool isTextControl() const;
    virtual bool isNativeTextControl() const;
    virtual bool isWebArea() const;
    virtual bool isCheckboxOrRadio() const;
    virtual bool isFileUploadButton() const;
    virtual bool isInputImage() const;
    virtual bool isProgressIndicator() const;
    virtual bool isSlider() const;
    virtual bool isMenuRelated() const;
    virtual bool isMenu() const;
    virtual bool isMenuBar() const;
    virtual bool isMenuButton() const;
    virtual bool isMenuItem() const;
    virtual bool isControl() const;
    virtual bool isFieldset() const;
    virtual bool isGroup() const;

    virtual bool isEnabled() const;
    virtual bool isSelected() const;
    virtual bool isFocused() const;
    virtual bool isChecked() const;
    virtual bool isHovered() const;
    virtual bool isIndeterminate() const;
    virtual bool isLoaded() const;
    virtual bool isMultiSelect() const;
    virtual bool isOffScreen() const;
    virtual bool isPressed() const;
    virtual bool isReadOnly() const;
    virtual bool isVisited() const;        

    const AtomicString& getAttribute(const QualifiedName&) const;
    virtual bool canSetFocusAttribute() const;
    virtual bool canSetTextRangeAttributes() const;
    virtual bool canSetValueAttribute() const;
    
    virtual bool hasIntValue() const;
    
    virtual bool accessibilityIsIgnored() const;
    
    static int headingLevel(Node*);
    virtual int intValue() const;
    virtual float valueForRange() const;
    virtual float maxValueForRange() const;
    virtual float minValueForRange() const;
    virtual int layoutCount() const;
    
    virtual AccessibilityObject* doAccessibilityHitTest(const IntPoint&) const;
    virtual AccessibilityObject* focusedUIElement() const;
    virtual AccessibilityObject* firstChild() const;
    virtual AccessibilityObject* lastChild() const;
    virtual AccessibilityObject* previousSibling() const;
    virtual AccessibilityObject* nextSibling() const;
    virtual AccessibilityObject* parentObject() const;
    virtual AccessibilityObject* parentObjectIfExists() const;
    virtual AccessibilityObject* observableObject() const;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const;
    virtual bool exposesTitleUIElement() const;
    virtual AccessibilityObject* titleUIElement() const;
    virtual AccessibilityRole ariaRoleAttribute() const;
    virtual bool isPresentationalChildOfAriaRole() const;
    virtual bool ariaRoleHasPresentationalChildren() const;
    void setAriaRole();
    virtual AccessibilityRole roleValue() const;
    virtual AXObjectCache* axObjectCache() const;
    
    virtual Element* actionElement() const;
    Element* mouseButtonListener() const;
    FrameView* frameViewIfRenderView() const;
    virtual Element* anchorElement() const;
    AccessibilityObject* menuForMenuButton() const;
    AccessibilityObject* menuButtonForMenu() const;
    
    virtual IntRect boundingBoxRect() const;
    virtual IntRect elementRect() const;
    virtual IntSize size() const;
    virtual IntPoint clickPoint() const;
    
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }
    RenderObject* renderer() const { return m_renderer; }
    RenderView* topRenderer() const;
    RenderTextControl* textControl() const;
    Document* document() const;
    FrameView* topDocumentFrameView() const;  
    HTMLLabelElement* labelElementContainer() const;
    
    virtual KURL url() const;
    virtual PlainTextRange selectedTextRange() const;
    virtual VisibleSelection selection() const;
    virtual String stringValue() const;
    virtual String ariaAccessiblityName(const String&) const;
    virtual String ariaLabeledByAttribute() const;
    virtual String title() const;
    virtual String ariaDescribedByAttribute() const;
    virtual String accessibilityDescription() const;
    virtual String helpText() const;
    virtual String textUnderElement() const;
    virtual String text() const;
    virtual int textLength() const;
    virtual PassRefPtr<Range> ariaSelectedTextDOMRange() const;
    virtual String selectedText() const;
    virtual const AtomicString& accessKey() const;
    virtual const String& actionVerb() const;
    virtual Widget* widget() const;
    virtual Widget* widgetForAttachmentView() const;
    virtual void getDocumentLinks(AccessibilityChildrenVector&);
    virtual FrameView* documentFrameView() const;
    virtual String language() const;
    
    virtual const AccessibilityChildrenVector& children();
    
    virtual void setFocused(bool);
    virtual void setSelectedTextRange(const PlainTextRange&);
    virtual void setValue(const String&);
    
    virtual void detach();
    virtual void childrenChanged();
    virtual void addChildren();
    virtual bool canHaveChildren() const;
    virtual void selectedChildren(AccessibilityChildrenVector&);
    virtual void visibleChildren(AccessibilityChildrenVector&);
    virtual bool shouldFocusActiveDescendant() const;
    virtual AccessibilityObject* activeDescendant() const;
    virtual void handleActiveDescendantChanged();

    virtual VisiblePositionRange visiblePositionRange() const;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const;
    
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const;
    virtual VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const;    
    virtual int index(const VisiblePosition&) const;

    virtual VisiblePosition visiblePositionForIndex(int) const;
    virtual int indexForVisiblePosition(const VisiblePosition&) const;
    
    virtual PlainTextRange doAXRangeForLine(unsigned) const;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const;
    
    virtual String doAXStringForRange(const PlainTextRange&) const;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const;
    
    virtual void updateBackingStore();
    
protected:
    RenderObject* m_renderer;
    AccessibilityRole m_ariaRole;
    mutable bool m_childrenDirty;
    
    void setRenderObject(RenderObject* renderer) { m_renderer = renderer; }
    
    virtual bool isDetached() const { return !m_renderer; }

private:
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    void ariaListboxVisibleChildren(AccessibilityChildrenVector&);
    bool ariaIsHidden() const;

    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const; 
    AccessibilityRole determineAriaRoleAttribute() const;

    IntRect checkboxOrRadioRect() const;
    void addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement* map) const;

    void markChildrenDirty() const { m_childrenDirty = true; }
};
    
} // namespace WebCore

#endif // AccessibilityRenderObject_h
