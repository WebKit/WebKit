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
    virtual AccessibilityRole roleValue() const { return ScrollAreaRole; }
    ScrollView* scrollView() const { return m_scrollView; }

    virtual ~AccessibilityScrollView();
    virtual void detach();

protected:
    virtual ScrollableArea* getScrollableAreaIfScrollable() const;
    virtual void scrollTo(const IntPoint&) const;
    
private:
    explicit AccessibilityScrollView(ScrollView*);
    
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
    virtual bool isAccessibilityScrollView() const OVERRIDE { return true; }
    virtual bool isEnabled() const OVERRIDE { return true; }
    
    virtual bool isAttachment() const OVERRIDE;
    virtual Widget* widgetForAttachmentView() const OVERRIDE;
    
    virtual AccessibilityObject* scrollBar(AccessibilityOrientation) OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual void clearChildren() OVERRIDE;
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const OVERRIDE;
    virtual void updateChildrenIfNecessary() OVERRIDE;
    virtual void setNeedsToUpdateChildren() OVERRIDE { m_childrenDirty = true; }
    void updateScrollbars();
    virtual void setFocused(bool) OVERRIDE;
    virtual bool canSetFocusAttribute() const OVERRIDE;
    virtual bool isFocused() const OVERRIDE;
    
    virtual FrameView* documentFrameView() const OVERRIDE;
    virtual LayoutRect elementRect() const OVERRIDE;
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    virtual AccessibilityObject* parentObjectIfExists() const OVERRIDE;
    
    AccessibilityObject* webAreaObject() const;
    virtual AccessibilityObject* firstChild() const OVERRIDE { return webAreaObject(); }
    AccessibilityScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AccessibilityObject*);
    
    ScrollView* m_scrollView;
    RefPtr<AccessibilityObject> m_horizontalScrollbar;
    RefPtr<AccessibilityObject> m_verticalScrollbar;
    bool m_childrenDirty;
};

inline AccessibilityScrollView* toAccessibilityScrollView(AccessibilityObject* object)
{
    ASSERT(!object || object->isAccessibilityScrollView());
    if (!object->isAccessibilityScrollView())
        return 0;
    
    return static_cast<AccessibilityScrollView*>(object);
}
    
} // namespace WebCore

#endif // AccessibilityScrollView_h
