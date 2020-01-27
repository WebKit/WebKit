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

#include "config.h"
#include "AccessibilityScrollView.h"

#include "AXObjectCache.h"
#include "AccessibilityScrollbar.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "RenderElement.h"
#include "ScrollView.h"
#include "Widget.h"

namespace WebCore {
    
AccessibilityScrollView::AccessibilityScrollView(ScrollView* view)
    : m_scrollView(view)
    , m_childrenDirty(false)
{
}

AccessibilityScrollView::~AccessibilityScrollView()
{
    ASSERT(isDetached());
}

void AccessibilityScrollView::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityObject::detachRemoteParts(detachmentType);
    m_scrollView = nullptr;
}

Ref<AccessibilityScrollView> AccessibilityScrollView::create(ScrollView* view)
{
    return adoptRef(*new AccessibilityScrollView(view));
}
    
AccessibilityObject* AccessibilityScrollView::scrollBar(AccessibilityOrientation orientation)
{
    updateScrollbars();
    
    switch (orientation) {
    // ARIA 1.1 Elements with the role scrollbar have an implicit aria-orientation value of vertical.
    case AccessibilityOrientation::Undefined:
    case AccessibilityOrientation::Vertical:
        return m_verticalScrollbar ? m_verticalScrollbar.get() : nullptr;
    case AccessibilityOrientation::Horizontal:
        return m_horizontalScrollbar ? m_horizontalScrollbar.get() : nullptr;
    }
    
    return nullptr;
}

// If this is WebKit1 then the native scroll view needs to return the
// AX information (because there are no scroll bar children in the ScrollView object in WK1).
// In WebKit2, the ScrollView object will return the AX information (because there are no platform widgets).
bool AccessibilityScrollView::isAttachment() const
{
    return m_scrollView && m_scrollView->platformWidget();
}

Widget* AccessibilityScrollView::widgetForAttachmentView() const
{
    return m_scrollView;
}
    
bool AccessibilityScrollView::canSetFocusAttribute() const
{
    AccessibilityObject* webArea = webAreaObject();
    return webArea && webArea->canSetFocusAttribute();
}
    
bool AccessibilityScrollView::isFocused() const
{
    AccessibilityObject* webArea = webAreaObject();
    return webArea && webArea->isFocused();
}
    
void AccessibilityScrollView::setFocused(bool focused)
{
    if (AccessibilityObject* webArea = webAreaObject())
        webArea->setFocused(focused);
}

void AccessibilityScrollView::updateChildrenIfNecessary()
{
    // Always update our children when asked for them so that we don't inadvertently cache them after
    // a new web area has been created for this scroll view (like when moving back and forth through history).
    // Since a ScrollViews children will always be relatively small and limited this should not be a performance problem.
    clearChildren();
    addChildren();
}

void AccessibilityScrollView::updateScrollbars()
{
    if (!m_scrollView)
        return;

    if (m_scrollView->horizontalScrollbar() && !m_horizontalScrollbar)
        m_horizontalScrollbar = addChildScrollbar(m_scrollView->horizontalScrollbar());
    else if (!m_scrollView->horizontalScrollbar() && m_horizontalScrollbar) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;
    }

    if (m_scrollView->verticalScrollbar() && !m_verticalScrollbar)
        m_verticalScrollbar = addChildScrollbar(m_scrollView->verticalScrollbar());
    else if (!m_scrollView->verticalScrollbar() && m_verticalScrollbar) {
        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
    }
}
    
void AccessibilityScrollView::removeChildScrollbar(AccessibilityObject* scrollbar)
{
    size_t pos = m_children.find(scrollbar);
    if (pos != WTF::notFound) {
        m_children[pos]->detachFromParent();
        m_children.remove(pos);
    }
}
    
AccessibilityScrollbar* AccessibilityScrollView::addChildScrollbar(Scrollbar* scrollbar)
{
    if (!scrollbar)
        return nullptr;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    auto& scrollBarObject = downcast<AccessibilityScrollbar>(*cache->getOrCreate(scrollbar));
    scrollBarObject.setParent(this);
    m_children.append(&scrollBarObject);
    return &scrollBarObject;
}
        
void AccessibilityScrollView::clearChildren()
{
    AccessibilityObject::clearChildren();
    m_verticalScrollbar = nullptr;
    m_horizontalScrollbar = nullptr;
}

bool AccessibilityScrollView::computeAccessibilityIsIgnored() const
{
    AccessibilityObject* webArea = webAreaObject();
    if (!webArea)
        return true;
    
    return webArea->accessibilityIsIgnored();
}

void AccessibilityScrollView::addChildren()
{
    ASSERT(!m_haveChildren);
    m_haveChildren = true;
    
    addChild(webAreaObject());
    updateScrollbars();    
}

AccessibilityObject* AccessibilityScrollView::webAreaObject() const
{
    if (!is<FrameView>(m_scrollView))
        return nullptr;
    
    Document* document = downcast<FrameView>(*m_scrollView).frame().document();
    if (!document || !document->hasLivingRenderTree())
        return nullptr;

    if (AXObjectCache* cache = axObjectCache())
        return cache->getOrCreate(document);
    
    return nullptr;
}

AXCoreObject* AccessibilityScrollView::accessibilityHitTest(const IntPoint& point) const
{
    AccessibilityObject* webArea = webAreaObject();
    if (!webArea)
        return nullptr;
    
    if (m_horizontalScrollbar && m_horizontalScrollbar->elementRect().contains(point))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && m_verticalScrollbar->elementRect().contains(point))
        return m_verticalScrollbar.get();
    
    return webArea->accessibilityHitTest(point);
}

LayoutRect AccessibilityScrollView::elementRect() const
{
    if (!m_scrollView)
        return LayoutRect();

    LayoutRect rect = m_scrollView->frameRect();
    float topContentInset = m_scrollView->topContentInset();
    // Top content inset pushes the frame down and shrinks it.
    rect.move(0, topContentInset);
    rect.contract(0, topContentInset);
    return rect;
}

FrameView* AccessibilityScrollView::documentFrameView() const
{
    if (!is<FrameView>(m_scrollView))
        return nullptr;
    
    return downcast<FrameView>(m_scrollView);
}    

AccessibilityObject* AccessibilityScrollView::parentObject() const
{
    if (!is<FrameView>(m_scrollView))
        return nullptr;

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    HTMLFrameOwnerElement* owner = downcast<FrameView>(*m_scrollView).frame().ownerElement();
    if (owner && owner->renderer())
        return cache->getOrCreate(owner);

    return nullptr;
}
    
AccessibilityObject* AccessibilityScrollView::parentObjectIfExists() const
{
    if (!is<FrameView>(m_scrollView))
        return nullptr;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    HTMLFrameOwnerElement* owner = downcast<FrameView>(m_scrollView)->frame().ownerElement();
    if (owner && owner->renderer())
        return cache->get(owner);
    
    return nullptr;
}

ScrollableArea* AccessibilityScrollView::getScrollableAreaIfScrollable() const
{
    return m_scrollView;
}

void AccessibilityScrollView::scrollTo(const IntPoint& point) const
{
    if (m_scrollView)
        m_scrollView->setScrollPosition(point);
}

} // namespace WebCore    
