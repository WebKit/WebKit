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

#include "AccessibilityNodeObject.h"
#include "LayoutRect.h"
#include "RenderObject.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
    
class AccessibilitySVGRoot;
class AXObjectCache;
class Element;
class FrameView;
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class IntPoint;
class IntSize;
class Node;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class Widget;
    
class AccessibilityRenderObject : public AccessibilityNodeObject, public CanMakeWeakPtr<AccessibilityRenderObject> {
public:
    static Ref<AccessibilityRenderObject> create(RenderObject*);
    virtual ~AccessibilityRenderObject();

    void init() override;
    
    bool isAttachment() const override;
    bool isFileUploadButton() const override;

    bool isSelected() const override;
    bool isFocused() const override;
    bool isLoaded() const override;
    bool isOffScreen() const override;
    bool isUnvisited() const override;
    bool isVisited() const override;
    bool isLinked() const override;
    bool hasBoldFont() const override;
    bool hasItalicFont() const override;
    bool hasPlainText() const override;
    bool hasSameFont(RenderObject*) const override;
    bool hasSameFontColor(RenderObject*) const override;
    bool hasSameStyle(RenderObject*) const override;
    bool hasUnderline() const override;

    bool canSetTextRangeAttributes() const override;
    bool canSetExpandedAttribute() const override;

    void setAccessibleName(const AtomicString&) override;
    
    // Provides common logic used by all elements when determining isIgnored.
    AccessibilityObjectInclusion defaultObjectInclusion() const override;
    
    int layoutCount() const override;
    double estimatedLoadingProgress() const override;
    
    AccessibilityObject* firstChild() const override;
    AccessibilityObject* lastChild() const override;
    AccessibilityObject* previousSibling() const override;
    AccessibilityObject* nextSibling() const override;
    AccessibilityObject* parentObject() const override;
    AccessibilityObject* parentObjectIfExists() const override;
    AccessibilityObject* observableObject() const override;
    void linkedUIElements(AccessibilityChildrenVector&) const override;
    bool exposesTitleUIElement() const override;
    AccessibilityObject* titleUIElement() const override;
    AccessibilityObject* correspondingControlForLabelElement() const override;
    AccessibilityObject* correspondingLabelForControlElement() const override;

    bool supportsARIAOwns() const override;
    bool isPresentationalChildOfAriaRole() const override;
    bool ariaRoleHasPresentationalChildren() const override;
    
    // Should be called on the root accessibility object to kick off a hit test.
    AccessibilityObjectInterface* accessibilityHitTest(const IntPoint&) const override;

    Element* anchorElement() const override;
    
    LayoutRect boundingBoxRect() const override;
    LayoutRect elementRect() const override;
    IntPoint clickPoint() override;
    
    void setRenderer(RenderObject*);
    RenderObject* renderer() const override { return m_renderer.get(); }
    RenderBoxModelObject* renderBoxModelObject() const;
    Node* node() const override;

    Document* document() const override;

    RenderView* topRenderer() const;
    RenderTextControl* textControl() const;
    HTMLLabelElement* labelElementContainer() const;
    
    URL url() const override;
    PlainTextRange selectedTextRange() const override;
    VisibleSelection selection() const override;
    String stringValue() const override;
    String helpText() const override;
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    String text() const override;
    int textLength() const override;
    String selectedText() const override;
    const AtomicString& accessKey() const override;
    virtual const String& actionVerb() const;
    Widget* widget() const override;
    Widget* widgetForAttachmentView() const override;
    virtual void getDocumentLinks(AccessibilityChildrenVector&);
    FrameView* documentFrameView() const override;

    void clearChildren() override;
    void updateChildrenIfNecessary() override;
    
    void setFocused(bool) override;
    void setSelectedTextRange(const PlainTextRange&) override;
    void setValue(const String&) override;
    void setSelectedRows(AccessibilityChildrenVector&) override;
    AccessibilityOrientation orientation() const override;
    
    void detach(AccessibilityDetachmentType, AXObjectCache*) override;
    void textChanged() override;
    void addChildren() override;
    bool canHaveChildren() const override;
    bool canHaveSelectedChildren() const override;
    void selectedChildren(AccessibilityChildrenVector&) override;
    void visibleChildren(AccessibilityChildrenVector&) override;
    void tabChildren(AccessibilityChildrenVector&) override;
    bool shouldFocusActiveDescendant() const override;
    bool shouldNotifyActiveDescendant() const;
    AccessibilityObject* activeDescendant() const override;
    void handleActiveDescendantChanged() override;
    void handleAriaExpandedChanged() override;
    
    VisiblePositionRange visiblePositionRange() const override;
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override;
    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override;
    IntRect boundsForRange(const RefPtr<Range>) const override;
    IntRect boundsForRects(LayoutRect&, LayoutRect&, RefPtr<Range>) const;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override;
    bool isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange&) const;
    bool hasPopup() const override;

    bool supportsARIADropping() const override;
    bool supportsARIADragging() const override;
    bool isARIAGrabbed() override;
    Vector<String> determineARIADropEffects() override;
    
    VisiblePosition visiblePositionForPoint(const IntPoint&) const override;
    VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const override;
    int index(const VisiblePosition&) const override;

    VisiblePosition visiblePositionForIndex(int) const override;
    int indexForVisiblePosition(const VisiblePosition&) const override;

    void lineBreaks(Vector<int>&) const override;
    PlainTextRange doAXRangeForLine(unsigned) const override;
    PlainTextRange doAXRangeForIndex(unsigned) const override;
    
    String doAXStringForRange(const PlainTextRange&) const override;
    IntRect doAXBoundsForRange(const PlainTextRange&) const override;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const override;
    
    String stringValueForMSAA() const override;
    String stringRoleForMSAA() const override;
    String nameForMSAA() const override;
    String descriptionForMSAA() const override;
    AccessibilityRole roleValueForMSAA() const override;

    String passwordFieldValue() const override;

protected:
    explicit AccessibilityRenderObject(RenderObject*);
    ScrollableArea* getScrollableAreaIfScrollable() const override;
    void scrollTo(const IntPoint&) const override;
    
    bool isDetached() const override { return !m_renderer; }

    AccessibilityRole determineAccessibilityRole() override;
    bool computeAccessibilityIsIgnored() const override;

#if ENABLE(MATHML)
    virtual bool isIgnoredElementWithinMathTree() const;
#endif

    WeakPtr<RenderObject> m_renderer;

private:
    bool isAccessibilityRenderObject() const final { return true; }
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    void ariaListboxVisibleChildren(AccessibilityChildrenVector&);
    bool isAllowedChildOfTree() const;
    bool hasTextAlternative() const;
    String positionalDescriptionForMSAA() const;
    PlainTextRange documentBasedSelectedTextRange() const;
    Element* rootEditableElementForPosition(const Position&) const;
    bool nodeIsTextControl(const Node*) const;
    void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    bool needsToUpdateChildren() const override { return m_childrenDirty; }
    void setNeedsToUpdateSubtree() override { m_subtreeDirty = true; }
    Path elementPath() const override;
    
    bool isTabItemSelected() const;
    LayoutRect checkboxOrRadioRect() const;
    void addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    void addRadioButtonGroupChildren(AccessibilityObject*, AccessibilityChildrenVector&) const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement*) const;
    AccessibilityObjectInterface* elementAccessibilityHitTest(const IntPoint&) const override;

    bool renderObjectIsObservable(RenderObject&) const;
    RenderObject* renderParentObject() const;
    bool isDescendantOfElementType(const QualifiedName& tagName) const;
    
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    enum CreationChoice { Create, Retrieve };
    AccessibilitySVGRoot* remoteSVGRootElement(CreationChoice createIfNecessary) const;
    AccessibilityObjectInterface* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    bool supportsPath() const override;

    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
#if PLATFORM(COCOA)
    void updateAttachmentViewParents();
#endif
    String expandedTextValue() const override;
    bool supportsExpandedTextValue() const override;
    void updateRoleAfterChildrenCreation();
    
    void ariaSelectedRows(AccessibilityChildrenVector&);
    
    bool elementAttributeValue(const QualifiedName&) const;
    void setElementAttributeValue(const QualifiedName&, bool);
    
    OptionSet<SpeakAs> speakAsProperty() const override;
    
    const String liveRegionStatus() const override;
    const String liveRegionRelevant() const override;
    bool liveRegionAtomic() const override;
    bool isBusy() const override;

    bool inheritsPresentationalRole() const override;

    bool shouldGetTextFromNode(AccessibilityTextUnderElementMode) const;

    RenderObject* targetElementForActiveDescendant(const QualifiedName&, AccessibilityObject*) const;
    bool canHavePlainText() const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityRenderObject, isAccessibilityRenderObject())
