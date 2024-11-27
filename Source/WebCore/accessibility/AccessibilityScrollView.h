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

#include "AccessibilityObject.h"
#include "ScrollView.h"

namespace WebCore {
    
class AXRemoteFrame;
class AccessibilityScrollbar;
class Scrollbar;
class ScrollView;
    
class AccessibilityScrollView final : public AccessibilityObject {
public:
    static Ref<AccessibilityScrollView> create(AXID, ScrollView&);
    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::ScrollArea; }
    ScrollView* scrollView() const final { return currentScrollView(); }

    virtual ~AccessibilityScrollView();

    AccessibilityObject* webAreaObject() const final;
    void setNeedsToUpdateChildren() final { m_childrenDirty = true; }

private:
    explicit AccessibilityScrollView(AXID, ScrollView&);
    void detachRemoteParts(AccessibilityDetachmentType) final;

    ScrollView* currentScrollView() const;
    ScrollableArea* getScrollableAreaIfScrollable() const final { return currentScrollView(); }
    void scrollTo(const IntPoint&) const final;
    bool computeIsIgnored() const final;
    bool isAccessibilityScrollViewInstance() const final { return true; }
    bool isEnabled() const final { return true; }
    bool hasRemoteFrameChild() const final { return m_remoteFrame; }

    bool isAttachment() const final;
    PlatformWidget platformWidget() const final;
    Widget* widgetForAttachmentView() const final { return currentScrollView(); }

    AccessibilityObject* scrollBar(AccessibilityOrientation) final;
    void addChildren() final;
    void clearChildren() final;
    AccessibilityObject* accessibilityHitTest(const IntPoint&) const final;
    void updateChildrenIfNecessary() final;
    void updateScrollbars();
    void setFocused(bool) final;
    bool canSetFocusAttribute() const final;
    bool isFocused() const final;
    void addRemoteFrameChild();

    Document* document() const final;
    LocalFrameView* documentFrameView() const final;
    LayoutRect elementRect() const final;
    AccessibilityObject* parentObject() const final;

    AccessibilityObject* firstChild() const final { return webAreaObject(); }
    AccessibilityScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AccessibilityObject*);

    SingleThreadWeakPtr<ScrollView> m_scrollView;
    WeakPtr<HTMLFrameOwnerElement, WeakPtrImplWithEventTargetData> m_frameOwnerElement;
    RefPtr<AccessibilityObject> m_horizontalScrollbar;
    RefPtr<AccessibilityObject> m_verticalScrollbar;
    bool m_childrenDirty;
    RefPtr<AXRemoteFrame> m_remoteFrame;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityScrollView) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isAccessibilityScrollViewInstance(); } \
SPECIALIZE_TYPE_TRAITS_END()
