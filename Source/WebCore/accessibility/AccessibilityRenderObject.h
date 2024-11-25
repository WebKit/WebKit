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
#include "PluginViewBase.h"
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
    static Ref<AccessibilityRenderObject> create(AXID, RenderObject&);
    virtual ~AccessibilityRenderObject();
    
    FloatRect frameRect() const final;
    bool isNonLayerSVGObject() const final;

    bool isAttachment() const final;
    bool isDetached() const final { return !m_renderer && AccessibilityNodeObject::isDetached(); }
    bool isOffScreen() const final;
    bool hasBoldFont() const final;
    bool hasItalicFont() const final;
    bool hasPlainText() const final;
    bool hasSameFont(const AXCoreObject&) const final;
    bool hasSameFontColor(const AXCoreObject&) const final;
    bool hasSameStyle(const AXCoreObject&) const final;
    bool hasUnderline() const final;

    void setAccessibleName(const AtomString&) final;

    int layoutCount() const final;

    AccessibilityObject* firstChild() const final;
    AccessibilityObject* lastChild() const final;
    AccessibilityObject* previousSibling() const final;
    AccessibilityObject* nextSibling() const final;
    AccessibilityObject* parentObject() const override;
    AccessibilityObject* observableObject() const override;
    AccessibilityObject* titleUIElement() const override;

    // Should be called on the root accessibility object to kick off a hit test.
    AccessibilityObject* accessibilityHitTest(const IntPoint&) const final;

    Element* anchorElement() const final;
    
    LayoutRect boundingBoxRect() const final;

    RenderObject* renderer() const final { return m_renderer.get(); }
    Document* document() const final;

    URL url() const final;
    CharacterRange selectedTextRange() const final;
    int insertionPointLineNumber() const final;
    String stringValue() const override;
    String helpText() const override;
    String textUnderElement(TextUnderElementMode = TextUnderElementMode()) const override;
    String selectedText() const final;
#if ENABLE(AX_THREAD_TEXT_APIS)
    AXTextRuns textRuns() final;
#endif

    bool isWidget() const final;
    Widget* widget() const final;
    Widget* widgetForAttachmentView() const final;
    AccessibilityChildrenVector documentLinks() final;
    LocalFrameView* documentFrameView() const final;
    bool isPlugin() const final { return is<PluginViewBase>(widget()); }

    void setSelectedTextRange(CharacterRange&&) final;
    bool setValue(const String&) override;

    void addChildren() override;

    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const final;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const final;
    bool isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange&) const;

    VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const final;
    int index(const VisiblePosition&) const final;

    VisiblePosition visiblePositionForIndex(int) const final;
    int indexForVisiblePosition(const VisiblePosition&) const final;

    CharacterRange doAXRangeForLine(unsigned) const final;
    CharacterRange doAXRangeForIndex(unsigned) const final;

    String doAXStringForRange(const CharacterRange&) const final;
    IntRect doAXBoundsForRange(const CharacterRange&) const final;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const final;

    String secureFieldValue() const final;
    void labelText(Vector<AccessibilityText>&) const override;
protected:
    explicit AccessibilityRenderObject(AXID, RenderObject&);
    explicit AccessibilityRenderObject(AXID, Node&);
    void detachRemoteParts(AccessibilityDetachmentType) final;
    ScrollableArea* getScrollableAreaIfScrollable() const final;
    void scrollTo(const IntPoint&) const final;

    bool shouldIgnoreAttributeRole() const override;
    AccessibilityRole determineAccessibilityRole() override;
    bool computeIsIgnored() const override;

#if ENABLE(MATHML)
    virtual bool isIgnoredElementWithinMathTree() const;
#endif

    SingleThreadWeakPtr<RenderObject> m_renderer;

private:
    bool isAccessibilityRenderObject() const final { return true; }
    bool isAllowedChildOfTree() const;
    CharacterRange documentBasedSelectedTextRange() const;
    RefPtr<Element> rootEditableElementForPosition(const Position&) const;
    bool elementIsTextControl(const Element&) const;
    Path elementPath() const final;

    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement&, const IntPoint&) const;
    AccessibilityObject* associatedAXImage(HTMLMapElement&) const;
    AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const override;

    bool renderObjectIsObservable(RenderObject&) const;
    RenderObject* renderParentObject() const;
    RenderObject* markerRenderer() const;

    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    enum CreationChoice { Create, Retrieve };
    AccessibilitySVGRoot* remoteSVGRootElement(CreationChoice createIfNecessary) const;
    AccessibilityObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    bool supportsPath() const final;

#if USE(ATSPI)
    void addNodeOnlyChildren();
    void addCanvasChildren();
#endif // USE(ATSPI)
    void addTextFieldChildren();
    void addImageMapChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
    void addListItemMarker();
#if PLATFORM(COCOA)
    void updateAttachmentViewParents();
#endif
    String expandedTextValue() const override;
    bool supportsExpandedTextValue() const override;
    virtual void updateRoleAfterChildrenCreation();

    bool inheritsPresentationalRole() const override;

    bool shouldGetTextFromNode(const TextUnderElementMode&) const;

#if ENABLE(APPLE_PAY)
    bool isApplePayButton() const;
    ApplePayButtonType applePayButtonType() const;
    String applePayButtonDescription() const;
#endif

    bool canHavePlainText() const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityRenderObject, isAccessibilityRenderObject())
