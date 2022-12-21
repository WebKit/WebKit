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
#include "Widget.h"

namespace WebCore {
    
AccessibilityScrollView::AccessibilityScrollView(ScrollView* view)
    : m_scrollView(view)
    , m_childrenDirty(false)
{
    if (is<FrameView>(view))
        m_frameOwnerElement = downcast<FrameView>(*view).frame().ownerElement();
}

AccessibilityScrollView::~AccessibilityScrollView()
{
    ASSERT(isDetached());
}

void AccessibilityScrollView::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityObject::detachRemoteParts(detachmentType);
    m_scrollView = nullptr;
    m_frameOwnerElement = nullptr;
}

Ref<AccessibilityScrollView> AccessibilityScrollView::create(ScrollView* view)
{
    return adoptRef(*new AccessibilityScrollView(view));
}

ScrollView* AccessibilityScrollView::currentScrollView() const
{
    return m_scrollView ? m_scrollView.get() : documentFrameView();
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
    if (auto* scrollView = currentScrollView())
        return scrollView->platformWidget();
    return false;
}

PlatformWidget AccessibilityScrollView::platformWidget() const
{
    if (auto* scrollView = currentScrollView())
        return scrollView->platformWidget();
    return nullptr;
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
    // Call the base class setFocused to ensure the view is focused and active.
    AccessibilityObject::setFocused(focused);

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
    auto* scrollView = currentScrollView();
    if (!scrollView)
        return;

    if (scrollView->horizontalScrollbar() && !m_horizontalScrollbar)
        m_horizontalScrollbar = addChildScrollbar(scrollView->horizontalScrollbar());
    else if (!scrollView->horizontalScrollbar() && m_horizontalScrollbar) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;
    }

    if (scrollView->verticalScrollbar() && !m_verticalScrollbar)
        m_verticalScrollbar = addChildScrollbar(scrollView->verticalScrollbar());
    else if (!scrollView->verticalScrollbar() && m_verticalScrollbar) {
        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
    }
}
    
void AccessibilityScrollView::removeChildScrollbar(AccessibilityObject* scrollbar)
{
    size_t pos = m_children.find(scrollbar);
    if (pos != notFound) {
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
    addChild(&scrollBarObject);
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
    ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;

    addChild(webAreaObject());
    updateScrollbars();
}

AccessibilityObject* AccessibilityScrollView::webAreaObject() const
{
    auto* document = this->document();
    if (!document || !document->hasLivingRenderTree())
        return nullptr;

    if (auto* cache = axObjectCache())
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
    auto* scrollView = currentScrollView();
    if (!scrollView)
        return LayoutRect();

    LayoutRect rect = scrollView->frameRect();
    float topContentInset = scrollView->topContentInset();

    // Top content inset pushes the frame down and shrinks it.
    rect.move(0, topContentInset);
    rect.contract(0, topContentInset);
    return rect;
}

Document* AccessibilityScrollView::document() const
{
    if (auto* frameView = dynamicDowncast<FrameView>(m_scrollView.get())) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frameView->frame()))
            return localFrame->document();
    }
    return AccessibilityObject::document();
}

FrameView* AccessibilityScrollView::documentFrameView() const
{
    if (is<FrameView>(m_scrollView))
        return downcast<FrameView>(m_scrollView.get());

    if (m_frameOwnerElement && m_frameOwnerElement->contentDocument())
        return m_frameOwnerElement->contentDocument()->view();

    return nullptr;
}

AccessibilityObject* AccessibilityScrollView::parentObject() const
{
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    HTMLFrameOwnerElement* owner = m_frameOwnerElement.get();
    if (is<FrameView>(m_scrollView))
        owner = downcast<FrameView>(*m_scrollView).frame().ownerElement();

    if (owner && owner->renderer())
        return cache->getOrCreate(owner);

    return nullptr;
}

void AccessibilityScrollView::scrollTo(const IntPoint& point) const
{
    if (auto* scrollView = currentScrollView())
        scrollView->setScrollPosition(point);
}

} // namespace WebCore    
