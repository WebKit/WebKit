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

#ifndef AccessibilityScrollView_h
#define AccessibilityScrollView_h

#include "AccessibilityObject.h"

namespace WebCore {
    
class AccessibilityScrollbar;
class Scrollbar;
class ScrollView;
    
class AccessibilityScrollView : public AccessibilityObject {
public:
    static PassRefPtr<AccessibilityScrollView> create(ScrollView*);    
    virtual AccessibilityRole roleValue() const override { return ScrollAreaRole; }
    ScrollView* scrollView() const { return m_scrollView; }

    virtual ~AccessibilityScrollView();
    virtual void detach(AccessibilityDetachmentType, AXObjectCache*) override;

protected:
    virtual ScrollableArea* getScrollableAreaIfScrollable() const override;
    virtual void scrollTo(const IntPoint&) const override;
    
private:
    explicit AccessibilityScrollView(ScrollView*);
    
    virtual bool computeAccessibilityIsIgnored() const override;
    virtual bool isAccessibilityScrollView() const override { return true; }
    virtual bool isEnabled() const override { return true; }
    
    virtual bool isAttachment() const override;
    virtual Widget* widgetForAttachmentView() const override;
    
    virtual AccessibilityObject* scrollBar(AccessibilityOrientation) override;
    virtual void addChildren() override;
    virtual void clearChildren() override;
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const override;
    virtual void updateChildrenIfNecessary() override;
    virtual void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    void updateScrollbars();
    virtual void setFocused(bool) override;
    virtual bool canSetFocusAttribute() const override;
    virtual bool isFocused() const override;
    
    virtual FrameView* documentFrameView() const override;
    virtual LayoutRect elementRect() const override;
    virtual AccessibilityObject* parentObject() const override;
    virtual AccessibilityObject* parentObjectIfExists() const override;
    
    AccessibilityObject* webAreaObject() const;
    virtual AccessibilityObject* firstChild() const override { return webAreaObject(); }
    AccessibilityScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AccessibilityObject*);
    
    ScrollView* m_scrollView;
    RefPtr<AccessibilityObject> m_horizontalScrollbar;
    RefPtr<AccessibilityObject> m_verticalScrollbar;
    bool m_childrenDirty;
};

ACCESSIBILITY_OBJECT_TYPE_CASTS(AccessibilityScrollView, isAccessibilityScrollView())
    
} // namespace WebCore

#endif // AccessibilityScrollView_h
