/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ScrollingCoordinator.h"

#include "Document.h"
#include "EventNames.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "IntRect.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "PluginViewBase.h"
#include "Region.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "Settings.h"
#include "TextStream.h"
#include <wtf/MainThread.h>
#include <wtf/text/StringBuilder.h>

#if USE(COORDINATED_GRAPHICS)
#include "ScrollingCoordinatorCoordinatedGraphics.h"
#endif

#if ENABLE(WEB_REPLAY)
#include "ReplayController.h"
#include <replay/InputCursor.h>
#endif

namespace WebCore {

#if !PLATFORM(COCOA)
Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
#if USE(COORDINATED_GRAPHICS)
    return adoptRef(*new ScrollingCoordinatorCoordinatedGraphics(page));
#endif

    return adoptRef(*new ScrollingCoordinator(page));
}
#endif

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = nullptr;
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(const FrameView& frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!frameView.frame().isMainFrame() && !m_page->settings().scrollingTreeIncludesFrames())
        return false;

    RenderView* renderView = m_page->mainFrame().contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
}

EventTrackingRegions ScrollingCoordinator::absoluteEventTrackingRegionsForFrame(const Frame& frame) const
{
    auto* renderView = frame.contentRenderer();
    if (!renderView || renderView->renderTreeBeingDestroyed())
        return EventTrackingRegions();

#if ENABLE(IOS_TOUCH_EVENTS)
    // On iOS, we use nonFastScrollableRegion to represent the region covered by elements with touch event handlers.
    ASSERT(frame.isMainFrame());
    auto* document = frame.document();
    if (!document)
        return EventTrackingRegions();
    return document->eventTrackingRegions();
#else
    auto* frameView = frame.view();
    if (!frameView)
        return EventTrackingRegions();

    Region nonFastScrollableRegion;

    // FIXME: should ASSERT(!frameView->needsLayout()) here, but need to fix DebugPageOverlays
    // to not ask for regions at bad times.

    if (auto* scrollableAreas = frameView->scrollableAreas()) {
        for (auto& scrollableArea : *scrollableAreas) {
            // Composited scrollable areas can be scrolled off the main thread.
            if (scrollableArea->usesAsyncScrolling())
                continue;

            bool isInsideFixed;
            IntRect box = scrollableArea->scrollableAreaBoundingBox(&isInsideFixed);
            if (isInsideFixed)
                box = IntRect(frameView->fixedScrollableAreaBoundsInflatedForScrolling(LayoutRect(box)));

            nonFastScrollableRegion.unite(box);
        }
    }

    for (auto& widget : frameView->widgetsInRenderTree()) {
        if (!is<PluginViewBase>(*widget))
            continue;
        if (!downcast<PluginViewBase>(*widget).wantsWheelEvents())
            continue;
        auto* renderWidget = RenderWidget::find(*widget);
        if (!renderWidget)
            continue;
        nonFastScrollableRegion.unite(renderWidget->absoluteBoundingBoxRect());
    }
    
    EventTrackingRegions eventTrackingRegions;

    // FIXME: if we've already accounted for this subframe as a scrollable area, we can avoid recursing into it here.
    for (Frame* subframe = frame.tree().firstChild(); subframe; subframe = subframe->tree().nextSibling()) {
        auto* subframeView = subframe->view();
        if (!subframeView)
            continue;

        EventTrackingRegions subframeRegion = absoluteEventTrackingRegionsForFrame(*subframe);
        // Map from the frame document to our document.
        IntPoint offset = subframeView->contentsToContainingViewContents(IntPoint());

        // FIXME: this translation ignores non-trival transforms on the frame.
        subframeRegion.translate(toIntSize(offset));
        eventTrackingRegions.unite(subframeRegion);
    }

    auto wheelHandlerRegion = frame.document()->absoluteRegionForEventTargets(frame.document()->wheelEventTargets());
    bool wheelHandlerInFixedContent = wheelHandlerRegion.second;
    if (wheelHandlerInFixedContent) {
        // FIXME: need to handle position:sticky here too.
        LayoutRect inflatedWheelHandlerBounds = frameView->fixedScrollableAreaBoundsInflatedForScrolling(LayoutRect(wheelHandlerRegion.first.bounds()));
        wheelHandlerRegion.first.unite(enclosingIntRect(inflatedWheelHandlerBounds));
    }
    
    nonFastScrollableRegion.unite(wheelHandlerRegion.first);

    // FIXME: If this is not the main frame, we could clip the region to the frame's bounds.
    eventTrackingRegions.uniteSynchronousRegion(eventNames().wheelEvent, nonFastScrollableRegion);

    return eventTrackingRegions;
#endif
}

EventTrackingRegions ScrollingCoordinator::absoluteEventTrackingRegions() const
{
    return absoluteEventTrackingRegionsForFrame(m_page->mainFrame());
}

void ScrollingCoordinator::frameViewHasSlowRepaintObjectsDidChange(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateSynchronousScrollingReasons(frameView);
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateSynchronousScrollingReasons(frameView);
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForScrollableArea(ScrollableArea& scrollableArea)
{
    return scrollableArea.layerForScrolling();
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForFrameView(FrameView& frameView)
{
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().scrollLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::headerLayerForFrameView(FrameView& frameView)
{
#if ENABLE(RUBBER_BANDING)
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().headerLayer();
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::footerLayerForFrameView(FrameView& frameView)
{
#if ENABLE(RUBBER_BANDING)
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().footerLayer();
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::counterScrollingLayerForFrameView(FrameView& frameView)
{
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().fixedRootBackgroundLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::insetClipLayerForFrameView(FrameView& frameView)
{
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().clipLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::contentShadowLayerForFrameView(FrameView& frameView)
{
#if ENABLE(RUBBER_BANDING)
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().layerForContentShadow();
    
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::rootContentLayerForFrameView(FrameView& frameView)
{
    if (RenderView* renderView = frameView.frame().contentRenderer())
        return renderView->compositor().rootContentLayer();
    return nullptr;
}

void ScrollingCoordinator::frameViewRootLayerDidChange(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    frameViewLayoutUpdated(frameView);
    updateSynchronousScrollingReasons(frameView);
}

#if PLATFORM(COCOA)
void ScrollingCoordinator::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return;

    frameView->scrollAnimator().handleWheelEventPhase(phase);
}
#endif

bool ScrollingCoordinator::hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView& frameView) const
{
    const FrameView::ViewportConstrainedObjectSet* viewportConstrainedObjects = frameView.viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

    for (auto& viewportConstrainedObject : *viewportConstrainedObjects) {
        if (!is<RenderBoxModelObject>(*viewportConstrainedObject) || !viewportConstrainedObject->hasLayer())
            return true;
        RenderLayer& layer = *downcast<RenderBoxModelObject>(*viewportConstrainedObject).layer();
        // Any explicit reason that a fixed position element is not composited shouldn't cause slow scrolling.
        if (!layer.isComposited() && layer.viewportConstrainedNotCompositedReason() == RenderLayer::NoNotCompositedReason)
            return true;
    }
    return false;
}

SynchronousScrollingReasons ScrollingCoordinator::synchronousScrollingReasons(const FrameView& frameView) const
{
    SynchronousScrollingReasons synchronousScrollingReasons = (SynchronousScrollingReasons)0;

    if (m_forceSynchronousScrollLayerPositionUpdates)
        synchronousScrollingReasons |= ForcedOnMainThread;
#if ENABLE(WEB_REPLAY)
    InputCursor& cursor = m_page->replayController().activeInputCursor();
    if (cursor.isCapturing() || cursor.isReplaying())
        synchronousScrollingReasons |= ForcedOnMainThread;
#endif
    if (frameView.hasSlowRepaintObjects())
        synchronousScrollingReasons |= HasSlowRepaintObjects;
    if (!supportsFixedPositionLayers() && frameView.hasViewportConstrainedObjects())
        synchronousScrollingReasons |= HasViewportConstrainedObjectsWithoutSupportingFixedLayers;
    if (supportsFixedPositionLayers() && hasVisibleSlowRepaintViewportConstrainedObjects(frameView))
        synchronousScrollingReasons |= HasNonLayerViewportConstrainedObjects;
    if (frameView.frame().mainFrame().document() && frameView.frame().document()->isImageDocument())
        synchronousScrollingReasons |= IsImageDocument;

    return synchronousScrollingReasons;
}

void ScrollingCoordinator::updateSynchronousScrollingReasons(const FrameView& frameView)
{
    // FIXME: Once we support async scrolling of iframes, we'll have to track the synchronous scrolling
    // reasons per frame (maybe on scrolling tree nodes).
    if (!frameView.frame().isMainFrame())
        return;

    setSynchronousScrollingReasons(synchronousScrollingReasons(frameView));
}

void ScrollingCoordinator::setForceSynchronousScrollLayerPositionUpdates(bool forceSynchronousScrollLayerPositionUpdates)
{
    if (m_forceSynchronousScrollLayerPositionUpdates == forceSynchronousScrollLayerPositionUpdates)
        return;

    m_forceSynchronousScrollLayerPositionUpdates = forceSynchronousScrollLayerPositionUpdates;
    if (FrameView* frameView = m_page->mainFrame().view())
        updateSynchronousScrollingReasons(*frameView);
}

bool ScrollingCoordinator::shouldUpdateScrollLayerPositionSynchronously(const FrameView& frameView) const
{
    if (&frameView == m_page->mainFrame().view())
        return synchronousScrollingReasons(frameView);
    
    return true;
}

#if ENABLE(WEB_REPLAY)
void ScrollingCoordinator::replaySessionStateDidChange()
{
    // FIXME: Once we support async scrolling of iframes, this should go through all subframes.
    if (FrameView* frameView = m_page->mainFrame().view())
        updateSynchronousScrollingReasons(*frameView);
}
#endif

ScrollingNodeID ScrollingCoordinator::uniqueScrollLayerID()
{
    static ScrollingNodeID uniqueScrollLayerID = 1;
    return uniqueScrollLayerID++;
}

String ScrollingCoordinator::scrollingStateTreeAsText() const
{
    return String();
}

String ScrollingCoordinator::synchronousScrollingReasonsAsText(SynchronousScrollingReasons reasons)
{
    StringBuilder stringBuilder;

    if (reasons & ScrollingCoordinator::ForcedOnMainThread)
        stringBuilder.appendLiteral("Forced on main thread, ");
    if (reasons & ScrollingCoordinator::HasSlowRepaintObjects)
        stringBuilder.appendLiteral("Has slow repaint objects, ");
    if (reasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
        stringBuilder.appendLiteral("Has viewport constrained objects without supporting fixed layers, ");
    if (reasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
        stringBuilder.appendLiteral("Has non-layer viewport-constrained objects, ");
    if (reasons & ScrollingCoordinator::IsImageDocument)
        stringBuilder.appendLiteral("Is image document, ");

    if (stringBuilder.length())
        stringBuilder.resize(stringBuilder.length() - 2);
    return stringBuilder.toString();
}

String ScrollingCoordinator::synchronousScrollingReasonsAsText() const
{
    if (FrameView* frameView = m_page->mainFrame().view())
        return synchronousScrollingReasonsAsText(synchronousScrollingReasons(*frameView));

    return String();
}

TextStream& operator<<(TextStream& ts, ScrollingNodeType nodeType)
{
    switch (nodeType) {
    case FrameScrollingNode:
        ts << "frame-scrolling";
        break;
    case OverflowScrollingNode:
        ts << "overflow-scrolling";
        break;
    case FixedNode:
        ts << "fixed";
        break;
    case StickyNode:
        ts << "sticky";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollingLayerPositionAction action)
{
    switch (action) {
    case ScrollingLayerPositionAction::Set:
        ts << "set";
        break;
    case ScrollingLayerPositionAction::SetApproximate:
        ts << "set approximate";
        break;
    case ScrollingLayerPositionAction::Sync:
        ts << "sync";
        break;
    }
    return ts;
}

} // namespace WebCore
