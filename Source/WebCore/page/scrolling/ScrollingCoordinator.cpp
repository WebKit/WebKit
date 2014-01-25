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
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "IntRect.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "PluginViewBase.h"
#include "Region.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include <wtf/MainThread.h>
#include <wtf/text/StringBuilder.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

#if USE(COORDINATED_GRAPHICS)
#include "ScrollingCoordinatorCoordinatedGraphics.h"
#endif

namespace WebCore {

#if !PLATFORM(MAC)
PassRefPtr<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
#if USE(COORDINATED_GRAPHICS)
    return adoptRef(new ScrollingCoordinatorCoordinatedGraphics(page));
#endif

    return adoptRef(new ScrollingCoordinator(page));
}
#endif

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
    , m_forceSynchronousScrollLayerPositionUpdates(false)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(FrameView* frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // We currently only handle the main frame.
    if (!frameView->frame().isMainFrame())
        return false;

    // We currently only support composited mode.
#if USE(ACCELERATED_COMPOSITING)
    RenderView* renderView = m_page->mainFrame().contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
#else
    return false;
#endif
}

Region ScrollingCoordinator::computeNonFastScrollableRegion(const Frame* frame, const IntPoint& frameLocation) const
{
    Region nonFastScrollableRegion;
    FrameView* frameView = frame->view();
    if (!frameView)
        return nonFastScrollableRegion;

    IntPoint offset = frameLocation;
    offset.moveBy(frameView->frameRect().location());

    if (const FrameView::ScrollableAreaSet* scrollableAreas = frameView->scrollableAreas()) {
        for (FrameView::ScrollableAreaSet::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
#if USE(ACCELERATED_COMPOSITING)
            // Composited scrollable areas can be scrolled off the main thread.
            if (scrollableArea->usesCompositedScrolling())
                continue;
#endif
            IntRect box = scrollableArea->scrollableAreaBoundingBox();
            box.moveBy(offset);
            nonFastScrollableRegion.unite(box);
        }
    }

    for (auto it = frameView->children().begin(), end = frameView->children().end(); it != end; ++it) {
        if (!(*it)->isPluginViewBase())
            continue;
        PluginViewBase* pluginViewBase = toPluginViewBase((*it).get());
        if (pluginViewBase->wantsWheelEvents())
            nonFastScrollableRegion.unite(pluginViewBase->frameRect());
    }

    for (Frame* subframe = frame->tree().firstChild(); subframe; subframe = subframe->tree().nextSibling())
        nonFastScrollableRegion.unite(computeNonFastScrollableRegion(subframe, offset));

    return nonFastScrollableRegion;
}

unsigned ScrollingCoordinator::computeCurrentWheelEventHandlerCount()
{
    unsigned wheelEventHandlerCount = 0;

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            wheelEventHandlerCount += frame->document()->wheelEventHandlerCount();
    }

    return wheelEventHandlerCount;
}

void ScrollingCoordinator::frameViewWheelEventHandlerCountChanged(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    recomputeWheelEventHandlerCountForFrameView(frameView);
}

void ScrollingCoordinator::frameViewHasSlowRepaintObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateSynchronousScrollingReasons();
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateSynchronousScrollingReasons();
}

#if USE(ACCELERATED_COMPOSITING)
GraphicsLayer* ScrollingCoordinator::scrollLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForScrolling();
}

GraphicsLayer* ScrollingCoordinator::horizontalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForHorizontalScrollbar();
}

GraphicsLayer* ScrollingCoordinator::verticalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForVerticalScrollbar();
}
#endif

GraphicsLayer* ScrollingCoordinator::scrollLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* renderView = frameView->frame().contentRenderer())
        return renderView->compositor().scrollLayer();
    return 0;
#else
    UNUSED_PARAM(frameView);
    return 0;
#endif
}

GraphicsLayer* ScrollingCoordinator::headerLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING) && ENABLE(RUBBER_BANDING)
    if (RenderView* renderView = frameView->frame().contentRenderer())
        renderView->compositor().headerLayer();
    return 0;
#else
    UNUSED_PARAM(frameView);
    return 0;
#endif
}

GraphicsLayer* ScrollingCoordinator::footerLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING) && ENABLE(RUBBER_BANDING)
    if (RenderView* renderView = frameView->frame().contentRenderer())
        return renderView->compositor().footerLayer();
    return 0;
#else
    UNUSED_PARAM(frameView);
    return 0;
#endif
}

GraphicsLayer* ScrollingCoordinator::counterScrollingLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING)
    if (RenderView* renderView = frameView->frame().contentRenderer())
        return renderView->compositor().fixedRootBackgroundLayer();
    return 0;
#else
    UNUSED_PARAM(frameView);
    return 0;
#endif
}

void ScrollingCoordinator::frameViewRootLayerDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    frameViewLayoutUpdated(frameView);
    recomputeWheelEventHandlerCountForFrameView(frameView);
    updateSynchronousScrollingReasons();
}

#if PLATFORM(MAC)
void ScrollingCoordinator::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return;

    frameView->scrollAnimator()->handleWheelEventPhase(phase);
}
#endif

bool ScrollingCoordinator::hasVisibleSlowRepaintViewportConstrainedObjects(FrameView* frameView) const
{
    const FrameView::ViewportConstrainedObjectSet* viewportConstrainedObjects = frameView->viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

#if USE(ACCELERATED_COMPOSITING)
    for (FrameView::ViewportConstrainedObjectSet::const_iterator it = viewportConstrainedObjects->begin(), end = viewportConstrainedObjects->end(); it != end; ++it) {
        RenderObject* viewportConstrainedObject = *it;
        if (!viewportConstrainedObject->isBoxModelObject() || !viewportConstrainedObject->hasLayer())
            return true;
        RenderLayer* layer = toRenderBoxModelObject(viewportConstrainedObject)->layer();
        // Any explicit reason that a fixed position element is not composited shouldn't cause slow scrolling.
        if (!layer->isComposited() && layer->viewportConstrainedNotCompositedReason() == RenderLayer::NoNotCompositedReason)
            return true;
    }
    return false;
#else
    return viewportConstrainedObjects->size();
#endif
}

SynchronousScrollingReasons ScrollingCoordinator::synchronousScrollingReasons() const
{
    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return static_cast<SynchronousScrollingReasons>(0);

    SynchronousScrollingReasons synchronousScrollingReasons = (SynchronousScrollingReasons)0;

    if (m_forceSynchronousScrollLayerPositionUpdates)
        synchronousScrollingReasons |= ForcedOnMainThread;
    if (frameView->hasSlowRepaintObjects())
        synchronousScrollingReasons |= HasSlowRepaintObjects;
    if (!supportsFixedPositionLayers() && frameView->hasViewportConstrainedObjects())
        synchronousScrollingReasons |= HasViewportConstrainedObjectsWithoutSupportingFixedLayers;
    if (supportsFixedPositionLayers() && hasVisibleSlowRepaintViewportConstrainedObjects(frameView))
        synchronousScrollingReasons |= HasNonLayerViewportConstrainedObjects;
    if (m_page->mainFrame().document() && m_page->mainFrame().document()->isImageDocument())
        synchronousScrollingReasons |= IsImageDocument;

    return synchronousScrollingReasons;
}

void ScrollingCoordinator::updateSynchronousScrollingReasons()
{
    setSynchronousScrollingReasons(synchronousScrollingReasons());
}

void ScrollingCoordinator::setForceSynchronousScrollLayerPositionUpdates(bool forceSynchronousScrollLayerPositionUpdates)
{
    if (m_forceSynchronousScrollLayerPositionUpdates == forceSynchronousScrollLayerPositionUpdates)
        return;

    m_forceSynchronousScrollLayerPositionUpdates = forceSynchronousScrollLayerPositionUpdates;
    updateSynchronousScrollingReasons();
}

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
        stringBuilder.append("Forced on main thread, ");
    if (reasons & ScrollingCoordinator::HasSlowRepaintObjects)
        stringBuilder.append("Has slow repaint objects, ");
    if (reasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
        stringBuilder.append("Has viewport constrained objects without supporting fixed layers, ");
    if (reasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
        stringBuilder.append("Has non-layer viewport-constrained objects, ");
    if (reasons & ScrollingCoordinator::IsImageDocument)
        stringBuilder.append("Is image document, ");

    if (stringBuilder.length())
        stringBuilder.resize(stringBuilder.length() - 2);
    return stringBuilder.toString();
}

String ScrollingCoordinator::synchronousScrollingReasonsAsText() const
{
    return synchronousScrollingReasonsAsText(synchronousScrollingReasons());
}

} // namespace WebCore
