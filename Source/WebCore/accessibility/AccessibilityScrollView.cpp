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
#include "AXRemoteFrame.h"
#include "AccessibilityScrollbar.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RemoteFrameView.h"
#include "RenderElement.h"
#include "Widget.h"

namespace WebCore {
    
AccessibilityScrollView::AccessibilityScrollView(AXID axID, ScrollView& view)
    : AccessibilityObject(axID)
    , m_scrollView(view)
    , m_childrenDirty(false)
{
    if (auto* localFrameView = dynamicDowncast<LocalFrameView>(view))
        m_frameOwnerElement = localFrameView->frame().ownerElement();
}

AccessibilityScrollView::~AccessibilityScrollView()
{
    ASSERT(isDetached());
}

void AccessibilityScrollView::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityObject::detachRemoteParts(detachmentType);

    auto* remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get());
    if (remoteFrameView && m_remoteFrame && (detachmentType == AccessibilityDetachmentType::ElementDestroyed || detachmentType == AccessibilityDetachmentType::CacheDestroyed)) {
#if PLATFORM(MAC)
        auto& remoteFrame = remoteFrameView->frame();
        remoteFrame.unbindRemoteAccessibilityFrames(m_remoteFrame->processIdentifier());
#endif
        m_remoteFrame = nullptr;
    }

    m_scrollView = nullptr;
    m_frameOwnerElement = nullptr;
}

Ref<AccessibilityScrollView> AccessibilityScrollView::create(AXID axID, ScrollView& view)
{
    return adoptRef(*new AccessibilityScrollView(axID, view));
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

    if (isWithinHiddenWebArea()) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;

        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
        return;
    }

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

        if (CheckedPtr cache = axObjectCache())
            cache->remove(scrollbar->objectID());
    }
}
    
AccessibilityScrollbar* AccessibilityScrollView::addChildScrollbar(Scrollbar* scrollbar)
{
    if (!scrollbar)
        return nullptr;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    auto& scrollBarObject = uncheckedDowncast<AccessibilityScrollbar>(*cache->getOrCreate(*scrollbar));
    scrollBarObject.setParent(this);
    addChild(&scrollBarObject);
    return &scrollBarObject;
}
        
void AccessibilityScrollView::clearChildren()
{
    AccessibilityObject::clearChildren();

    m_verticalScrollbar = nullptr;
    m_horizontalScrollbar = nullptr;

    m_childrenDirty = false;
}

bool AccessibilityScrollView::computeIsIgnored() const
{
    AccessibilityObject* webArea = webAreaObject();
    if (!webArea)
        return true;

    return webArea->isIgnored();
}

void AccessibilityScrollView::addRemoteFrameChild()
{
    auto* remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get());
    if (!remoteFrameView)
        return;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    if (!m_remoteFrame) {
        // Make the faux element that represents the remote transfer element for AX.
        m_remoteFrame = downcast<AXRemoteFrame>(cache->create(AccessibilityRole::RemoteFrame));
        m_remoteFrame->setParent(this);

#if PLATFORM(MAC)
        // Generate a new token and pass it back to the other remote frame so it can bind these objects together.
        Ref remoteFrame = remoteFrameView->frame();
        remoteFrame->bindRemoteAccessibilityFrames(getpid(), { m_remoteFrame->generateRemoteToken() }, [this, &remoteFrame, protectedAccessbilityRemoteFrame = RefPtr { m_remoteFrame }] (Vector<uint8_t> token, int processIdentifier) mutable {
            protectedAccessbilityRemoteFrame->initializePlatformElementWithRemoteToken(token.span(), processIdentifier);

            // Update the remote side with the offset of this object so it can calculate frames correctly.
            auto location = elementRect().location();
            remoteFrame->updateRemoteFrameAccessibilityOffset(flooredIntPoint(location));
        });
#endif
    } else
        m_remoteFrame->setParent(this);

    addChild(m_remoteFrame.get());
}

void AccessibilityScrollView::addChildren()
{
    ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;

    addRemoteFrameChild();
    addChild(webAreaObject());
    updateScrollbars();
}

AccessibilityObject* AccessibilityScrollView::webAreaObject() const
{
    auto* document = this->document();
    if (!document || !document->hasLivingRenderTree() || m_remoteFrame)
        return nullptr;

    if (auto* cache = axObjectCache())
        return cache->getOrCreate(*document);

    return nullptr;
}

AccessibilityObject* AccessibilityScrollView::accessibilityHitTest(const IntPoint& point) const
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
    return scrollView ? scrollView->frameRectShrunkByInset() : LayoutRect();
}

Document* AccessibilityScrollView::document() const
{
    if (auto* frameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        return frameView->frame().document();

    // For the RemoteFrameView case, we need to return the document of our hosting parent so axObjectCache() resolves correctly.
    if (auto* remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get())) {
        if (auto* owner = remoteFrameView->frame().ownerElement())
            return &(owner->document());
    }

    return AccessibilityObject::document();
}

LocalFrameView* AccessibilityScrollView::documentFrameView() const
{
    if (auto* localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        return localFrameView;

    if (m_frameOwnerElement && m_frameOwnerElement->contentDocument())
        return m_frameOwnerElement->contentDocument()->view();
    return nullptr;
}

AccessibilityObject* AccessibilityScrollView::parentObject() const
{
    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    WeakPtr owner = m_frameOwnerElement.get();
    if (auto* localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        owner = localFrameView->frame().ownerElement();
    else if (auto* remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get()))
        owner = remoteFrameView->frame().ownerElement();

    if (owner && owner->renderer())
        return cache->getOrCreate(*owner);
    return nullptr;
}

void AccessibilityScrollView::scrollTo(const IntPoint& point) const
{
    if (auto* scrollView = currentScrollView())
        scrollView->setScrollPosition(point);
}

} // namespace WebCore    
