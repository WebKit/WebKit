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
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "PluginViewBase.h"
#include "Region.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "Settings.h"
#include <wtf/MainThread.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#if PLATFORM(IOS_FAMILY) || !ENABLE(ASYNC_SCROLLING)
Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
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

    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (!localFrame)
        return false;

    if (!localFrame->isMainFrame() && !m_page->settings().scrollingTreeIncludesFrames()
#if PLATFORM(MAC) || USE(NICOSIA)
        && !m_page->settings().asyncFrameScrollingEnabled()
#endif
    )
        return false;

    auto* renderView = localFrame->contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
}

bool ScrollingCoordinator::coordinatesScrollingForOverflowLayer(const RenderLayer& layer) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    return layer.hasCompositedScrollableOverflow();
}

ScrollingNodeID ScrollingCoordinator::scrollableContainerNodeID(const RenderObject&) const
{
    return 0;
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
        return { };
    return document->eventTrackingRegions();
#else
    auto* frameView = frame.view();
    if (!frameView)
        return EventTrackingRegions();

    Region nonFastScrollableRegion;

    // FIXME: should ASSERT(!frameView->needsLayout()) here, but need to fix DebugPageOverlays
    // to not ask for regions at bad times.

    if (auto* scrollableAreas = frameView->scrollableAreas()) {
        for (auto& area : *scrollableAreas) {
            CheckedPtr<ScrollableArea> scrollableArea(area);
            // Composited scrollable areas can be scrolled off the main thread.
            if (!scrollableArea->isVisibleToHitTesting() || scrollableArea->usesAsyncScrolling())
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
    for (auto* subframe = frame.tree().firstChild(); subframe; subframe = subframe->tree().nextSibling()) {
        auto* localSubframe = dynamicDowncast<LocalFrame>(subframe);
        if (!localSubframe)
            continue;
        auto* subframeView = localSubframe->view();
        if (!subframeView)
            continue;

        EventTrackingRegions subframeRegion = absoluteEventTrackingRegionsForFrame(*localSubframe);
        // Map from the frame document to our document.
        IntPoint offset = subframeView->contentsToContainingViewContents(IntPoint());

        // FIXME: this translation ignores non-trival transforms on the frame.
        subframeRegion.translate(toIntSize(offset));
        eventTrackingRegions.unite(subframeRegion);
    }

#if !ENABLE(WHEEL_EVENT_REGIONS)
    auto wheelHandlerRegion = frame.document()->absoluteRegionForEventTargets(frame.document()->wheelEventTargets());
    bool wheelHandlerInFixedContent = wheelHandlerRegion.second;
    if (wheelHandlerInFixedContent) {
        // FIXME: need to handle position:sticky here too.
        LayoutRect inflatedWheelHandlerBounds = frameView->fixedScrollableAreaBoundsInflatedForScrolling(LayoutRect(wheelHandlerRegion.first.bounds()));
        wheelHandlerRegion.first.unite(enclosingIntRect(inflatedWheelHandlerBounds));
    }
    
    nonFastScrollableRegion.unite(wheelHandlerRegion.first);
#endif

    // FIXME: If this is not the main frame, we could clip the region to the frame's bounds.
    eventTrackingRegions.uniteSynchronousRegion(EventTrackingRegions::EventType::Wheel, nonFastScrollableRegion);

    return eventTrackingRegions;
#endif
}

EventTrackingRegions ScrollingCoordinator::absoluteEventTrackingRegions() const
{
    return absoluteEventTrackingRegionsForFrame(m_page->mainFrame());
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView& frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateSynchronousScrollingReasons(frameView);
}

GraphicsLayer* ScrollingCoordinator::scrollContainerLayerForFrameView(FrameView& frameView)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().scrollContainerLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::scrolledContentsLayerForFrameView(FrameView& frameView)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().scrolledContentsLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::headerLayerForFrameView(FrameView& frameView)
{
#if HAVE(RUBBER_BANDING)
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().headerLayer();
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::footerLayerForFrameView(FrameView& frameView)
{
#if HAVE(RUBBER_BANDING)
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().footerLayer();
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::counterScrollingLayerForFrameView(FrameView& frameView)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().fixedRootBackgroundLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::insetClipLayerForFrameView(FrameView& frameView)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().clipLayer();
    return nullptr;
}

GraphicsLayer* ScrollingCoordinator::contentShadowLayerForFrameView(FrameView& frameView)
{
#if HAVE(RUBBER_BANDING)
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().layerForContentShadow();
    
    return nullptr;
#else
    UNUSED_PARAM(frameView);
    return nullptr;
#endif
}

GraphicsLayer* ScrollingCoordinator::rootContentsLayerForFrameView(FrameView& frameView)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (auto* renderView = localFrame ? localFrame->contentRenderer() : nullptr)
        return renderView->compositor().rootContentsLayer();
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

bool ScrollingCoordinator::hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView& frameView) const
{
    auto* viewportConstrainedObjects = frameView.viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

    for (auto& viewportConstrainedObject : *viewportConstrainedObjects) {
        if (!is<RenderBoxModelObject>(viewportConstrainedObject) || !viewportConstrainedObject.hasLayer())
            return true;
        auto& layer = *downcast<RenderBoxModelObject>(viewportConstrainedObject).layer();
        // Any explicit reason that a fixed position element is not composited shouldn't cause slow scrolling.
        if (!layer.isComposited() && layer.viewportConstrainedNotCompositedReason() == RenderLayer::NoNotCompositedReason)
            return true;
    }
    return false;
}

void ScrollingCoordinator::updateSynchronousScrollingReasons(FrameView& frameView)
{
    ASSERT(coordinatesScrollingForFrameView(frameView));

    OptionSet<SynchronousScrollingReason> newSynchronousScrollingReasons;

    // RenderLayerCompositor::updateSynchronousScrollingReasons maintains this bit, so maintain its current value.
    if (synchronousScrollingReasons(frameView.scrollingNodeID()).contains(SynchronousScrollingReason::HasSlowRepaintObjects))
        newSynchronousScrollingReasons.add(SynchronousScrollingReason::HasSlowRepaintObjects);

    if (m_forceSynchronousScrollLayerPositionUpdates)
        newSynchronousScrollingReasons.add(SynchronousScrollingReason::ForcedOnMainThread);

    if (hasVisibleSlowRepaintViewportConstrainedObjects(frameView))
        newSynchronousScrollingReasons.add(SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects);

    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (localFrame
        && localFrame->mainFrame().document()
        && localFrame->document()->isImageDocument())
        newSynchronousScrollingReasons.add(SynchronousScrollingReason::IsImageDocument);

    setSynchronousScrollingReasons(frameView.scrollingNodeID(), newSynchronousScrollingReasons);
}

void ScrollingCoordinator::updateSynchronousScrollingReasonsForAllFrames()
{
    for (AbstractFrame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* frameView = localFrame->view()) {
            if (coordinatesScrollingForFrameView(*frameView))
                updateSynchronousScrollingReasons(*frameView);
        }
    }
}

void ScrollingCoordinator::setForceSynchronousScrollLayerPositionUpdates(bool forceSynchronousScrollLayerPositionUpdates)
{
    if (m_forceSynchronousScrollLayerPositionUpdates == forceSynchronousScrollLayerPositionUpdates)
        return;

    m_forceSynchronousScrollLayerPositionUpdates = forceSynchronousScrollLayerPositionUpdates;
    updateSynchronousScrollingReasonsForAllFrames();
}

bool ScrollingCoordinator::shouldUpdateScrollLayerPositionSynchronously(const FrameView& frameView) const
{
    if (&frameView == m_page->mainFrame().view())
        return !synchronousScrollingReasons(frameView.scrollingNodeID()).isEmpty();
    
    return true;
}

ScrollingNodeID ScrollingCoordinator::uniqueScrollingNodeID()
{
    static ScrollingNodeID uniqueScrollingNodeID = 1;
    return uniqueScrollingNodeID++;
}

void ScrollingCoordinator::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    ASSERT(isMainThread());
    if (!m_page)
        return;

    if (auto monitor = m_page->wheelEventTestMonitor())
        monitor->receivedWheelEventWithPhases(phase, momentumPhase);
}

void ScrollingCoordinator::deferWheelEventTestCompletionForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    ASSERT(isMainThread());
    if (!m_page)
        return;

    if (auto monitor = m_page->wheelEventTestMonitor())
        monitor->deferForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(nodeID), reason);
}

void ScrollingCoordinator::removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    ASSERT(isMainThread());
    if (!m_page)
        return;

    if (auto monitor = m_page->wheelEventTestMonitor())
        monitor->removeDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(nodeID), reason);
}

String ScrollingCoordinator::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior>) const
{
    return emptyString();
}

String ScrollingCoordinator::scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior>) const
{
    return emptyString();
}

String ScrollingCoordinator::synchronousScrollingReasonsAsText(OptionSet<SynchronousScrollingReason> reasons)
{
    auto string = makeString(reasons.contains(SynchronousScrollingReason::ForcedOnMainThread) ? "Forced on main thread, " : "",
        reasons.contains(SynchronousScrollingReason::HasSlowRepaintObjects) ? "Has slow repaint objects, " : "",
        reasons.contains(SynchronousScrollingReason::HasViewportConstrainedObjectsWithoutSupportingFixedLayers) ? "Has viewport constrained objects without supporting fixed layers, " : "",
        reasons.contains(SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects) ? "Has non-layer viewport-constrained objects, " : "",
        reasons.contains(SynchronousScrollingReason::IsImageDocument) ? "Is image document, " : "",
        reasons.contains(SynchronousScrollingReason::DescendantScrollersHaveSynchronousScrolling) ? "Has slow repaint descendant scrollers, " : "");
    return string.isEmpty() ? string : string.left(string.length() - 2);
}

String ScrollingCoordinator::synchronousScrollingReasonsAsText() const
{
    if (auto* frameView = m_page->mainFrame().view())
        return synchronousScrollingReasonsAsText(synchronousScrollingReasons(frameView->scrollingNodeID()));

    return String();
}

} // namespace WebCore
