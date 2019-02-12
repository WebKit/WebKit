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

namespace WebCore {
    
class AccessibilityScrollbar;
class Scrollbar;
class ScrollView;
    
class AccessibilityScrollView final : public AccessibilityObject {
public:
    static Ref<AccessibilityScrollView> create(ScrollView*);
    AccessibilityRole roleValue() const override { return AccessibilityRole::ScrollArea; }
    ScrollView* scrollView() const { return m_scrollView; }

    virtual ~AccessibilityScrollView();
    void detach(AccessibilityDetachmentType, AXObjectCache*) override;

    AccessibilityObject* webAreaObject() const;

private:
    explicit AccessibilityScrollView(ScrollView*);
    
    ScrollableArea* getScrollableAreaIfScrollable() const override;
    void scrollTo(const IntPoint&) const override;
    bool computeAccessibilityIsIgnored() const override;
    bool isAccessibilityScrollView() const override { return true; }
    bool isEnabled() const override { return true; }
    
    bool isAttachment() const override;
    Widget* widgetForAttachmentView() const override;
    
    AccessibilityObject* scrollBar(AccessibilityOrientation) override;
    void addChildren() override;
    void clearChildren() override;
    AccessibilityObjectInterface* accessibilityHitTest(const IntPoint&) const override;
    void updateChildrenIfNecessary() override;
    void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    void updateScrollbars();
    void setFocused(bool) override;
    bool canSetFocusAttribute() const override;
    bool isFocused() const override;
    
    FrameView* documentFrameView() const override;
    LayoutRect elementRect() const override;
    AccessibilityObject* parentObject() const override;
    AccessibilityObject* parentObjectIfExists() const override;
    
    AccessibilityObject* firstChild() const override { return webAreaObject(); }
    AccessibilityScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AccessibilityObject*);
    
    ScrollView* m_scrollView;
    RefPtr<AccessibilityObject> m_horizontalScrollbar;
    RefPtr<AccessibilityObject> m_verticalScrollbar;
    bool m_childrenDirty;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityScrollView, isAccessibilityScrollView())
