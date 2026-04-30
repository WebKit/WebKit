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

#include "AXLocalFrame.h"
#include "AXLoggerBase.h"
#include "AXObjectCacheInlines.h"
#include "AXRemoteFrame.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilityScrollbar.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "LocalFrameViewInlines.h"
#include "Page.h"
#include "RemoteFrameView.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "Widget.h"

namespace WebCore {

AccessibilityScrollView::AccessibilityScrollView(AXID axID, ScrollView& view, AXObjectCache& cache)
    : AccessibilityObject(axID, cache)
    , m_childrenDirty(false)
    , m_scrollView(view)
{
    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(view))
        m_frameOwnerElement = localFrameView->frame().ownerElement();
    else if (RefPtr remoteFrameView = dynamicDowncast<RemoteFrameView>(view))
        m_frameOwnerElement = remoteFrameView->frame().ownerElement();
}

AccessibilityScrollView::~AccessibilityScrollView()
{
    AX_ASSERT(isDetached());
}

bool AccessibilityScrollView::isRoot() const
{
    RefPtr frameView = dynamicDowncast<FrameView>(m_scrollView.get());

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    // A remote frame is not a root.
    if (frameView && frameView->isRemoteFrameView())
        return false;

    // Interpret this as "is this the root of the local frame"
    WeakPtr cache = axObjectCache();
    if (!cache)
        return false;

    return document() == cache->document();
#else
    // Interpret this as "is this the root of the whole page"
    return frameView && frameView->frame().isMainFrame();
#endif
}

String AccessibilityScrollView::ownerDebugDescription() const
{
    if (!m_frameOwnerElement) {
        StringBuilder builder;
        builder.append("null frame owner"_s);
        if (isRoot())
            builder.append(" (root)"_s);
        return builder.toString();
    }

    CheckedPtr renderer = m_frameOwnerElement->renderer();
    return makeString("owned by: "_s, renderer ? renderer->debugDescription() : protect(frameOwnerElement())->debugDescription());
}

String AccessibilityScrollView::extraDebugInfo() const
{
    Vector<String> debugStrings = { ownerDebugDescription(), AccessibilityObject::extraDebugInfo() };
    return makeStringByJoining(debugStrings, ","_s);
}

void AccessibilityScrollView::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityObject::detachRemoteParts(detachmentType);

    RefPtr remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get());
    if (remoteFrameView && m_remoteFrame && (detachmentType == AccessibilityDetachmentType::ElementDestroyed || detachmentType == AccessibilityDetachmentType::CacheDestroyed)) {
#if PLATFORM(MAC)
        Ref remoteFrame = remoteFrameView->frame();
        remoteFrame->unbindRemoteAccessibilityFrames(protect(m_remoteFrame)->remoteFramePID());
#endif
        m_remoteFrame = nullptr;
    }

    m_scrollView = nullptr;
    m_frameOwnerElement = nullptr;
}

Ref<AccessibilityScrollView> AccessibilityScrollView::create(AXID axID, ScrollView& view, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityScrollView(axID, view, cache));
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
    if (RefPtr scrollView = currentScrollView())
        return scrollView->platformWidget();
    return false;
}

PlatformWidget AccessibilityScrollView::platformWidget() const
{
    if (RefPtr scrollView = currentScrollView())
        return scrollView->platformWidget();
    return nullptr;
}

bool AccessibilityScrollView::canSetFocusAttribute() const
{
    RefPtr webArea = webAreaObject();
    return webArea && webArea->canSetFocusAttribute();
}

bool AccessibilityScrollView::isFocused() const
{
    RefPtr webArea = webAreaObject();
    return webArea && webArea->isFocused();
}

void AccessibilityScrollView::setFocused(bool focused)
{
    // Call the base class setFocused to ensure the view is focused and active.
    AccessibilityObject::setFocused(focused);

    if (RefPtr webArea = webAreaObject())
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
    RefPtr scrollView = currentScrollView();
    if (!scrollView)
        return;

    bool shouldHideScrollBars = isWithinHiddenWebArea();

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    if (!isRoot())
        shouldHideScrollBars = true;
#endif

    if (shouldHideScrollBars) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;

        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
        return;
    }

    if (scrollView->horizontalScrollbar() && !m_horizontalScrollbar)
        m_horizontalScrollbar = addChildScrollbar(protect(scrollView->horizontalScrollbar()).get());
    else if (!scrollView->horizontalScrollbar() && m_horizontalScrollbar) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;
    }

    if (scrollView->verticalScrollbar() && !m_verticalScrollbar)
        m_verticalScrollbar = addChildScrollbar(protect(scrollView->verticalScrollbar()).get());
    else if (!scrollView->verticalScrollbar() && m_verticalScrollbar) {
        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
    }
}

void AccessibilityScrollView::removeChildScrollbar(AccessibilityObject* scrollbar)
{
    if (!scrollbar)
        return;

    size_t position = m_children.findIf([&scrollbar] (const Ref<AXCoreObject>& child) {
        return child.ptr() == scrollbar;
    });
    if (position != notFound) {
        Ref { m_children[position] }->detachFromParent();
        m_children.removeAt(position);
        resetChildrenIndexInParent();

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

    Ref scrollBarObject = uncheckedDowncast<AccessibilityScrollbar>(*cache->getOrCreate(*scrollbar));
    scrollBarObject->setParent(this);
    addChild(scrollBarObject.get());
    return scrollBarObject.unsafePtr();
}

void AccessibilityScrollView::clearChildren()
{
    AccessibilityObject::clearChildren();

    m_verticalScrollbar = nullptr;
    m_horizontalScrollbar = nullptr;

    m_childrenDirty = false;
}

AccessibilityRole AccessibilityScrollView::determineAccessibilityRole()
{
#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    if (!isRoot())
        return AccessibilityRole::FrameHost;
#endif

    return AccessibilityRole::ScrollArea;
}

bool AccessibilityScrollView::computeIsIgnored() const
{
#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    WeakPtr cache = axObjectCache();
    if (!cache)
        return true;

    // If this is the child of an iframe element, ignore it in favor of the scroll view from the frame's AXObjectCache.
    if (!isRoot())
        return true;
#endif

    // Scroll view's that host remote frames won't have web area objects, but shouldn't be ignored so that they are also available in the isolated tree.
    if (m_remoteFrame)
        return false;

    RefPtr webArea = webAreaObject();
    if (!webArea)
        return true;

    return webArea->isIgnored();
}

void AccessibilityScrollView::addLocalFrameChild()
{
#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    if (!m_localFrame) {
        RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get());
        RefPtr localFrame = localFrameView ? &localFrameView->frame() : nullptr;
        RefPtr document = localFrame ? localFrame->document() : nullptr;
        WeakPtr frameAXObjectCache = document ? document->axObjectCache() : nullptr;
        if (!frameAXObjectCache)
            return;

        RefPtr frameRoot = frameAXObjectCache->rootObjectForFrame(*localFrame);
        if (!frameRoot)
            return;

        // Set the initial hosting node state on the child frame's root scroll view.
        if (RefPtr childScrollView = dynamicDowncast<AccessibilityScrollView>(frameRoot.get())) {
            childScrollView->setInheritedFrameState({ isHostingFrameHidden(), isHostingFrameInert(), isHostingFrameRenderHidden() });

            // Request the screen position for the UI process to update asynchronously.
            if (RefPtr page = this->page())
                page->chrome().client().requestFrameScreenPosition(localFrame->frameID());
        }

        if (RefPtr localFrame = downcast<AXLocalFrame>(cache->create(AccessibilityRole::LocalFrame))) {
            localFrame->setLocalFrameView(localFrameView.get());
            localFrame->setWrapperFrom(*frameRoot);
            m_localFrame = WTF::move(localFrame);
        }
    }

    if (RefPtr protectedLocalFrame = m_localFrame) {
        protectedLocalFrame->setParent(this);
        addChild(*protectedLocalFrame);
    }
#endif // ENABLE(ACCESSIBILITY_LOCAL_FRAME)
}

void AccessibilityScrollView::addRemoteFrameChild()
{
    RefPtr scrollFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get());
    if (!scrollFrameView)
        return;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    RefPtr remoteFrame = m_remoteFrame;
    if (!remoteFrame) {
        // Make the faux element that represents the remote transfer element for AX.
        m_remoteFrame = downcast<AXRemoteFrame>(cache->create(AccessibilityRole::RemoteFrame));
        remoteFrame = m_remoteFrame;
        remoteFrame->setParent(this);

#if PLATFORM(COCOA)
        // Generate a new token and pass it back to the other remote frame so it can bind these objects together.
        Ref scrollFrame = scrollFrameView->frame();
        remoteFrame->setFrameID(scrollFrame->frameID());
        scrollFrame->bindRemoteAccessibilityFrames(getpid(), remoteFrame->generateRemoteToken(), [this, protectedThis = Ref { *this }, weakScrollFrame = WeakPtr { scrollFrame.get() }, protectedRemoteFrame = Ref { *remoteFrame }] (AccessibilityRemoteToken token, int processIdentifier) mutable {
            protectedRemoteFrame->initializePlatformElementWithRemoteToken(token, processIdentifier);

            RefPtr scrollFrame = weakScrollFrame.get();
            if (!scrollFrame)
                return;

            // Update the remote side with the offset so it can calculate frames correctly.
            // This is the position of this iframe element within its parent frame,
            // adjusted for scroll via contentsToView().
            auto iframePosition = flooredIntPoint(elementRect().location());
            RefPtr parentAXScrollView = frameRootScrollView();
            if (RefPtr parentScrollView = parentAXScrollView ? parentAXScrollView->scrollView() : nullptr)
                iframePosition = parentScrollView->contentsToView(iframePosition);
            scrollFrame->updateRemoteFrameAccessibilityOffset(iframePosition);

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
            InheritedFrameState state { isHostingFrameHidden(), isHostingFrameInert(), isHostingFrameRenderHidden() };
            scrollFrame->updateRemoteFrameAccessibilityInheritedState(state);

            // Request the screen position for the UI process to update asynchronously.
            if (RefPtr page = this->page())
                page->chrome().client().requestFrameScreenPosition(scrollFrame->frameID());
#endif
        });
#endif // PLATFORM(COCOA)
    } else
        remoteFrame->setParent(this);

    addChild(*remoteFrame);
}

void AccessibilityScrollView::addChildren()
{
    AX_ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    if (isRoot())
        addChild(protect(webAreaObject()).get());
    else {
        // Always add frame children, even if ignored. Ignored state propagates naturally through the tree.
        addLocalFrameChild();
        addRemoteFrameChild();
    }
#else
    addRemoteFrameChild();
    addChild(protect(webAreaObject()).get());
#endif

    updateScrollbars();

#if ASSERT_ENABLED
    verifyChildrenIndexInParent();
#endif
}

AccessibilityObject* AccessibilityScrollView::webAreaObject() const
{
    RefPtr document = this->document();
    if (!document || !document->hasLivingRenderTree() || m_remoteFrame)
        return nullptr;

    if (CheckedPtr cache = axObjectCache())
        return cache->getOrCreate(*document);

    return nullptr;
}

RefPtr<AXCoreObject> AccessibilityScrollView::accessibilityHitTest(const IntPoint& point) const
{
    RefPtr webArea = webAreaObject();
    if (!webArea)
        return nullptr;

    if (m_horizontalScrollbar && protect(m_horizontalScrollbar)->elementRect().contains(point))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && protect(m_verticalScrollbar)->elementRect().contains(point))
        return m_verticalScrollbar.get();

    return webArea->accessibilityHitTest(point);
}

LayoutRect AccessibilityScrollView::elementRect() const
{
    RefPtr scrollView = currentScrollView();
    if (!scrollView)
        return LayoutRect();

    auto rect = scrollView->frameRectShrunkByInset();

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    // The scrollView's frameRect may include the iframe's position in the parent
    // page (non-zero for subframes, zero for main frames). Remove that offset
    // but preserve any inset offset applied by frameRectShrunkByInset().
    if (isRoot()) {
        auto frameOrigin = scrollView->frameRect().location();
        rect.move(-frameOrigin.x(), -frameOrigin.y());
    }
#else
    auto offset = remoteFrameOffset();
    if (isRoot() && !offset.isZero())
        rect.move(-offset.x(), -offset.y());
#endif

    return rect;
}

Document* AccessibilityScrollView::document() const
{
    if (auto* frameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        return frameView->frame().document();

    // For the RemoteFrameView case, we need to return the document of our hosting parent so axObjectCache() resolves correctly.
    if (RefPtr remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get())) {
        if (auto* owner = remoteFrameView->frame().ownerElement())
            return &(owner->document());
    }

    return AccessibilityObject::document();
}

LocalFrameView* AccessibilityScrollView::documentFrameView() const
{
    if (auto* localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        return localFrameView;

    auto* element = m_frameOwnerElement.get();
    if (element && element->contentDocument())
        return element->contentDocument()->view();
    return nullptr;
}

AccessibilityObject* AccessibilityScrollView::parentObject() const
{
    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    if (isRoot())
        return nullptr;
#endif

    RefPtr<Element> ancestorElement;
    ancestorElement = m_frameOwnerElement.get();
    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        ancestorElement = localFrameView->frame().ownerElement();

    if (!ancestorElement) {
        if (RefPtr remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get()))
            ancestorElement = remoteFrameView->frame().ownerElement();
    }

    RefPtr<AccessibilityObject> ancestorAccessibilityObject;
    while (ancestorElement && !ancestorAccessibilityObject) {
        if ((ancestorAccessibilityObject = cache->getOrCreate(*ancestorElement)))
            break;
        ancestorElement = ancestorElement->parentElementInComposedTree();
    }
    return ancestorAccessibilityObject.unsafeGet();
}

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)

AccessibilityObject* AccessibilityScrollView::crossFrameParentObject() const
{
    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    // If this is the child of an iframe element, do nothing.
    if (!isRoot())
        return nullptr;

    // If this is the main frame, do nothing.
    RefPtr frameView = dynamicDowncast<FrameView>(m_scrollView.get());
    if (!frameView)
        return nullptr;
    if (frameView->frame().isMainFrame())
        return nullptr;

    RefPtr<Element> ancestorElement;
    ancestorElement = m_frameOwnerElement.get();
    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_scrollView.get()))
        ancestorElement = localFrameView->frame().ownerElement();

    if (!ancestorElement)
        return nullptr;

    RefPtr ancestorDocument = ancestorElement->document();
    if (!ancestorDocument || ancestorDocument.get() == cache->document())
        return nullptr;

    WeakPtr ancestorCache = ancestorDocument->axObjectCache();
    RefPtr<AccessibilityObject> ancestorAccessibilityObject;
    while (ancestorElement && !ancestorAccessibilityObject) {
        if ((ancestorAccessibilityObject = ancestorCache->getOrCreate(*ancestorElement)))
            break;
        ancestorElement = ancestorElement->parentElementInComposedTree();
    }

    if (ancestorAccessibilityObject && ancestorAccessibilityObject->isIgnored())
        return ancestorAccessibilityObject->parentObjectUnignored();

    // TODO: this should return a RefPtr
    return ancestorAccessibilityObject.unsafeGet();  // NOLINT
}

AccessibilityObject* AccessibilityScrollView::crossFrameChildObject() const
{
    return nullptr;
}

bool AccessibilityScrollView::isFrameGeometryInitialized() const
{
    if (isRoot()) {
        if (CheckedPtr cache = axObjectCache())
            return cache->frameGeometry().has_value();
        return false;
    }
    return true;
}

AXFrameGeometry AccessibilityScrollView::frameGeometry() const
{
    if (CheckedPtr cache = axObjectCache()) {
        if (std::optional geometry = cache->frameGeometry())
            return *geometry;
        // Geometry hasn't been populated yet — request it asynchronously.
        if (RefPtr page = this->page())
            page->chrome().client().requestFrameScreenPosition(cache->frameID());
    }
    return { };
}

bool AccessibilityScrollView::isARIAHidden() const
{
    // This is necessary to implement on root scrollviews so that the base isAXHidden ancestor traversal works as expected.
    // If this is the root scroll view, use the inherited state from the parent frame.
    if (isRoot())
        return m_inheritedFrameState.isAXHidden;

    return AccessibilityObject::isARIAHidden();
}

bool AccessibilityScrollView::isHostingFrameInert() const
{
    if (isRoot())
        return m_inheritedFrameState.isInert;

    RefPtr frameOwner = frameOwnerElement();
    if (auto* renderer = frameOwner ? frameOwner->renderer() : nullptr)
        return renderer->style().effectiveInert();

    return false;
}

bool AccessibilityScrollView::isHostingFrameRenderHidden() const
{
    if (isRoot())
        return m_inheritedFrameState.isRenderHidden;

    RefPtr frameOwner = frameOwnerElement();
    if (auto* renderer = frameOwner ? frameOwner->renderer() : nullptr)
        return WebCore::isRenderHidden(protect(renderer->style()).get());

    return false;
}

void AccessibilityScrollView::setInheritedFrameState(InheritedFrameState state)
{
    if (m_inheritedFrameState.isAXHidden == state.isAXHidden
        && m_inheritedFrameState.isInert == state.isInert
        && m_inheritedFrameState.isRenderHidden == state.isRenderHidden)
        return;

    m_inheritedFrameState = state;

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    recomputeIsIgnoredForDescendants(/* includeSelf */ true);
#else
    if (WeakPtr cache = axObjectCache())
        cache->childrenChanged(this);
#endif
}

void AccessibilityScrollView::updateHostedFrameInheritedState()
{
    if (m_localFrame) {
        RefPtr localFrameView = m_localFrame->localFrameView();
        RefPtr document = localFrameView ? localFrameView->frame().document() : nullptr;
        WeakPtr hostedFrameCache = document ? document->axObjectCache() : nullptr;
        if (!hostedFrameCache)
            return;

        RefPtr protectedFrame = localFrameView->frame();
        RefPtr root = protectedFrame ? hostedFrameCache->rootObjectForFrame(*protectedFrame) : nullptr;
        RefPtr hostedFrameScrollView = dynamicDowncast<AccessibilityScrollView>(root);
        if (!hostedFrameScrollView)
            return;

        InheritedFrameState state { isHostingFrameHidden(), isHostingFrameInert(), isHostingFrameRenderHidden() };
        hostedFrameScrollView->setInheritedFrameState(state);
    }

    if (m_remoteFrame) {
        RefPtr remoteFrameView = dynamicDowncast<RemoteFrameView>(m_scrollView.get());
        if (!remoteFrameView)
            return;

        Ref remoteFrame = remoteFrameView->frame();
        InheritedFrameState state { isHostingFrameHidden(), isHostingFrameInert(), isHostingFrameRenderHidden() };
        remoteFrame->updateRemoteFrameAccessibilityInheritedState(state);
    }
}

#endif // ENABLE(ACCESSIBILITY_LOCAL_FRAME)

const AccessibilityScrollView* AccessibilityScrollView::frameRootScrollView() const
{
    if (isRoot())
        return this;

    return dynamicDowncast<AccessibilityScrollView>(ancestorAccessibilityScrollView(/* includeSelf */ false));
}

void AccessibilityScrollView::scrollTo(const IntPoint& point) const
{
    if (RefPtr scrollView = currentScrollView())
        scrollView->setScrollPosition(point);
}

} // namespace WebCore
