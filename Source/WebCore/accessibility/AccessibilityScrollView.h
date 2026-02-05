/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "AXRemoteFrame.h"
#include "AccessibilityObject.h"
#include "ScrollView.h"

namespace WebCore {

class AXLocalFrame;
class AXRemoteFrame;
class AccessibilityScrollbar;
class Scrollbar;
class ScrollView;

class AccessibilityScrollView final : public AccessibilityObject {
public:
    static Ref<AccessibilityScrollView> create(AXID, ScrollView&, AXObjectCache&);
    AccessibilityRole determineAccessibilityRole() final;
    ScrollView* scrollView() const final { return currentScrollView(); }

    virtual ~AccessibilityScrollView();

    AccessibilityObject* webAreaObject() const final;
    void setNeedsToUpdateChildren() final { m_childrenDirty = true; }

    RefPtr<AXRemoteFrame> remoteFrame() const { return m_remoteFrame; }

    String ownerDebugDescription() const;
    String extraDebugInfo() const final;

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    AccessibilityObject* crossFrameParentObject() const final;
    AccessibilityObject* crossFrameChildObject() const final;

    void setInheritedFrameState(InheritedFrameState);
    const InheritedFrameState& inheritedFrameState() const { return m_inheritedFrameState; }
    bool isAXHidden() const final;
    bool isARIAHidden() const final;
    void updateHostedFrameInheritedState();

    // Returns true if the iframe element (or ancestors) cause the content to be hidden.
    // We can't use isIgnored() because FrameHost scroll views are always ignored (see computeIsIgnored).
    bool isHostingFrameHidden() const { return isAXHidden(); }
    bool isHostingFrameInert() const;
    bool isHostingFrameRenderHidden() const;
    bool isIgnoredFromHostingFrame() const { return isHostingFrameHidden() || isHostingFrameInert() || isHostingFrameRenderHidden(); }
#endif // ENABLE(ACCESSIBLITY_LOCAL_FRAME)

private:
    explicit AccessibilityScrollView(AXID, ScrollView&, AXObjectCache&);
    void detachRemoteParts(AccessibilityDetachmentType) final;

    ScrollView* currentScrollView() const;
    ScrollableArea* getScrollableAreaIfScrollable() const final { return currentScrollView(); }
    void scrollTo(const IntPoint&) const final;
    bool computeIsIgnored() const final;
    bool isAccessibilityScrollViewInstance() const final { return true; }
    bool isEnabled() const final { return true; }
    bool hasRemoteFrameChild() const final { return m_remoteFrame; }

    bool isRoot() const final;
    bool isAttachment() const final;
    PlatformWidget platformWidget() const final;
    Widget* widgetForAttachmentView() const final { return currentScrollView(); }

    AccessibilityObject* scrollBar(AccessibilityOrientation) final;
    void addChildren() final;
    void clearChildren() final;
    RefPtr<AXCoreObject> accessibilityHitTest(const IntPoint&) const final;
    void updateChildrenIfNecessary() final;
    void updateScrollbars();
    void setFocused(bool) final;
    bool canSetFocusAttribute() const final;
    bool isFocused() const final;
    void addLocalFrameChild();
    void addRemoteFrameChild();

    Document* document() const final;
    LocalFrameView* documentFrameView() const final;
    LayoutRect elementRect() const final;
    LayoutRect boundingBoxRect() const final { return elementRect(); }
    AccessibilityObject* parentObject() const final;
    RefPtr<AccessibilityObject> protectedHorizontalScrollbar() const { return m_horizontalScrollbar; }
    RefPtr<AccessibilityObject> protectedVerticalScrollbar() const { return m_verticalScrollbar; }
    HTMLFrameOwnerElement* frameOwnerElement() const { return m_frameOwnerElement; }
    RefPtr<HTMLFrameOwnerElement> protectedFrameOwnerElement() const { return m_frameOwnerElement; }

    AccessibilityObject* firstChild() const final { return webAreaObject(); }
    AccessibilityScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AccessibilityObject*);

    bool m_childrenDirty;
    SingleThreadWeakPtr<ScrollView> m_scrollView;
    WeakPtr<HTMLFrameOwnerElement, WeakPtrImplWithEventTargetData> m_frameOwnerElement;
    RefPtr<AccessibilityObject> m_horizontalScrollbar;
    RefPtr<AccessibilityObject> m_verticalScrollbar;
#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    RefPtr<AXLocalFrame> m_localFrame;
    InheritedFrameState m_inheritedFrameState;
#endif
    RefPtr<AXRemoteFrame> m_remoteFrame;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityScrollView)
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isAccessibilityScrollViewInstance(); }
    static bool isType(const WebCore::AXCoreObject& object)
    {
        auto* accessibilityObject = dynamicDowncast<WebCore::AccessibilityObject>(object);
        return accessibilityObject && accessibilityObject->isAccessibilityScrollViewInstance();
    }
SPECIALIZE_TYPE_TRAITS_END()
