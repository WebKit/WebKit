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
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class IntPoint;
class IntSize;
class LocalFrameView;
class Node;
class RenderTextControl;
class RenderView;
class VisibleSelection;

class AccessibilityRenderObject : public AccessibilityNodeObject {
public:
    static Ref<AccessibilityRenderObject> create(RenderObject*);
    virtual ~AccessibilityRenderObject();
    
    bool isAttachment() const override;
    bool isOffScreen() const override;
    bool hasBoldFont() const override;
    bool hasItalicFont() const override;
    bool hasPlainText() const override;
    bool hasSameFont(const AXCoreObject&) const override;
    bool hasSameFontColor(const AXCoreObject&) const override;
    bool hasSameStyle(const AXCoreObject&) const override;
    bool hasUnderline() const override;

    void setAccessibleName(const AtomString&) override;
    
    int layoutCount() const override;
    
    AccessibilityObject* firstChild() const override;
    AccessibilityObject* lastChild() const override;
    AccessibilityObject* previousSibling() const override;
    AccessibilityObject* nextSibling() const override;
    AccessibilityObject* parentObject() const override;
    AccessibilityObject* parentObjectIfExists() const override;
    AccessibilityObject* observableObject() const override;
    AccessibilityObject* titleUIElement() const override;

    // Should be called on the root accessibility object to kick off a hit test.
    AXCoreObject* accessibilityHitTest(const IntPoint&) const override;

    Element* anchorElement() const override;
    
    LayoutRect boundingBoxRect() const override;

    RenderObject* renderer() const final { return m_renderer.get(); }
    Node* node() const override;

    Document* document() const override;

    URL url() const override;
    CharacterRange selectedTextRange() const override;
    int insertionPointLineNumber() const override;
    String stringValue() const override;
    String helpText() const override;
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    String selectedText() const override;

    bool isWidget() const override;
    Widget* widget() const override;
    Widget* widgetForAttachmentView() const override;
    AccessibilityChildrenVector documentLinks() override;
    LocalFrameView* documentFrameView() const override;

    void setSelectedTextRange(CharacterRange&&) override;
    bool setValue(const String&) override;

    void addChildren() override;

    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override;
    bool isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange&) const;

    VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const override;
    int index(const VisiblePosition&) const override;

    VisiblePosition visiblePositionForIndex(int) const final;
    int indexForVisiblePosition(const VisiblePosition&) const final;

    CharacterRange doAXRangeForLine(unsigned) const override;
    CharacterRange doAXRangeForIndex(unsigned) const override;
    
    String doAXStringForRange(const CharacterRange&) const override;
    IntRect doAXBoundsForRange(const CharacterRange&) const override;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const override;
    
    String secureFieldValue() const override;
    void titleElementText(Vector<AccessibilityText>&) const override;

protected:
    explicit AccessibilityRenderObject(RenderObject*);
    explicit AccessibilityRenderObject(Node&);
    void detachRemoteParts(AccessibilityDetachmentType) override;
    ScrollableArea* getScrollableAreaIfScrollable() const override;
    void scrollTo(const IntPoint&) const override;
    
    bool isDetached() const final { return !m_renderer && AccessibilityNodeObject::isDetached(); }

    bool shouldIgnoreAttributeRole() const override;
    AccessibilityRole determineAccessibilityRole() override;
    bool computeAccessibilityIsIgnored() const override;

#if ENABLE(MATHML)
    virtual bool isIgnoredElementWithinMathTree() const;
#endif

    WeakPtr<RenderObject> m_renderer;

private:
    bool isAccessibilityRenderObject() const final { return true; }
    bool isAllowedChildOfTree() const;
    CharacterRange documentBasedSelectedTextRange() const;
    Element* rootEditableElementForPosition(const Position&) const;
    bool nodeIsTextControl(const Node*) const;
    Path elementPath() const override;
    
    AXCoreObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement*) const;
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override;

    bool renderObjectIsObservable(RenderObject&) const;
    RenderObject* renderParentObject() const;
#if USE(ATSPI)
    RenderObject* markerRenderer() const;
#endif

    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    enum CreationChoice { Create, Retrieve };
    AccessibilitySVGRoot* remoteSVGRootElement(CreationChoice createIfNecessary) const;
    AXCoreObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    bool supportsPath() const override;

    void addNodeOnlyChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
#if USE(ATSPI)
    void addListItemMarker();
#endif
#if PLATFORM(COCOA)
    void updateAttachmentViewParents();
#endif
    String expandedTextValue() const override;
    bool supportsExpandedTextValue() const override;
    void updateRoleAfterChildrenCreation();
    
    bool inheritsPresentationalRole() const override;

    bool shouldGetTextFromNode(AccessibilityTextUnderElementMode) const;

#if ENABLE(APPLE_PAY)
    bool isApplePayButton() const;
    ApplePayButtonType applePayButtonType() const;
    String applePayButtonDescription() const;
#endif

    bool canHavePlainText() const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityRenderObject, isAccessibilityRenderObject())
