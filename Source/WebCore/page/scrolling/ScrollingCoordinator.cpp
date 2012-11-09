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

#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "PluginViewBase.h"
#include "Region.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "ScrollingTreeState.h"
#include <wtf/MainThread.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

#if ENABLE(THREADED_SCROLLING)
#include "ScrollingThread.h"
#include "ScrollingTree.h"
#include <wtf/Functional.h>
#include <wtf/PassRefPtr.h>
#endif

namespace WebCore {

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
    , m_forceMainThreadScrollLayerPositionUpdates(false)
#if ENABLE(THREADED_SCROLLING)
    , m_scrollingTreeState(ScrollingTreeState::create())
    , m_scrollingTree(ScrollingTree::create(this))
    , m_scrollingTreeStateCommitterTimer(this, &ScrollingCoordinator::scrollingTreeStateCommitterTimerFired)
#endif
    , m_private(0)
{
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;

#if ENABLE(THREADED_SCROLLING)
    m_scrollingTreeStateCommitterTimer.stop();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    ScrollingThread::dispatch(bind(&ScrollingTree::invalidate, m_scrollingTree.release()));
#endif
}

#if ENABLE(THREADED_SCROLLING)
ScrollingTree* ScrollingCoordinator::scrollingTree() const
{
    ASSERT(m_scrollingTree);
    return m_scrollingTree.get();
}
#endif

bool ScrollingCoordinator::coordinatesScrollingForFrameView(FrameView* frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // We currently only handle the main frame.
    if (frameView->frame() != m_page->mainFrame())
        return false;

    // We currently only support composited mode.
#if USE(ACCELERATED_COMPOSITING)
    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
#else
    return false;
#endif
}

static Region computeNonFastScrollableRegion(Frame* frame, const IntPoint& frameLocation)
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
            IntRect box = scrollableArea->scrollableAreaBoundingBox();
            box.moveBy(offset);
            nonFastScrollableRegion.unite(box);
        }
    }

    if (const HashSet<RefPtr<Widget> >* children = frameView->children()) {
        for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(), end = children->end(); it != end; ++it) {
            if (!(*it)->isPluginViewBase())
                continue;

            PluginViewBase* pluginViewBase = static_cast<PluginViewBase*>((*it).get());
            if (pluginViewBase->wantsWheelEvents())
                nonFastScrollableRegion.unite(pluginViewBase->frameRect());
        }
    }

    FrameTree* tree = frame->tree();
    for (Frame* subFrame = tree->firstChild(); subFrame; subFrame = subFrame->tree()->nextSibling())
        nonFastScrollableRegion.unite(computeNonFastScrollableRegion(subFrame, offset));

    return nonFastScrollableRegion;
}

void ScrollingCoordinator::frameViewLayoutUpdated(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // Compute the region of the page that we can't do fast scrolling for. This currently includes
    // all scrollable areas, such as subframes, overflow divs and list boxes. We need to do this even if the
    // frame view whose layout was updated is not the main frame.
    Region nonFastScrollableRegion = computeNonFastScrollableRegion(m_page->mainFrame(), IntPoint());
    setNonFastScrollableRegion(nonFastScrollableRegion);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    ScrollParameters scrollParameters;
    scrollParameters.horizontalScrollElasticity = frameView->horizontalScrollElasticity();
    scrollParameters.verticalScrollElasticity = frameView->verticalScrollElasticity();
    scrollParameters.hasEnabledHorizontalScrollbar = frameView->horizontalScrollbar() && frameView->horizontalScrollbar()->enabled();
    scrollParameters.hasEnabledVerticalScrollbar = frameView->verticalScrollbar() && frameView->verticalScrollbar()->enabled();
    scrollParameters.horizontalScrollbarMode = frameView->horizontalScrollbarMode();
    scrollParameters.verticalScrollbarMode = frameView->verticalScrollbarMode();

    scrollParameters.scrollOrigin = frameView->scrollOrigin();
    scrollParameters.viewportRect = IntRect(IntPoint(), frameView->visibleContentRect().size());
    scrollParameters.contentsSize = frameView->contentsSize();

    setScrollParameters(scrollParameters);
}

void ScrollingCoordinator::frameViewWheelEventHandlerCountChanged(FrameView*)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    recomputeWheelEventHandlerCount();
}

void ScrollingCoordinator::frameViewHasSlowRepaintObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

void ScrollingCoordinator::frameViewHasFixedObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

static GraphicsLayer* scrollLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING)
    Frame* frame = frameView->frame();
    if (!frame)
        return 0;

    RenderView* renderView = frame->contentRenderer();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
#else
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
    recomputeWheelEventHandlerCount();
    updateShouldUpdateScrollLayerPositionOnMainThread();
    setScrollLayer(scrollLayerForFrameView(frameView));
}

bool ScrollingCoordinator::requestScrollPositionUpdate(FrameView* frameView, const IntPoint& scrollPosition)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return false;

#if ENABLE(THREADED_SCROLLING)
    if (frameView->frame()->document()->inPageCache()) {
        // If this frame view's document is being put into the page cache, we don't want to update our
        // main frame scroll position. Just let the FrameView think that we did.
        updateMainFrameScrollPosition(scrollPosition, frameView->inProgrammaticScroll());
        return true;
    }

    m_scrollingTreeState->setRequestedScrollPosition(scrollPosition, frameView->inProgrammaticScroll());
    scheduleTreeStateCommit();
    return true;
#else
    UNUSED_PARAM(scrollPosition);
    return false;
#endif
}

bool ScrollingCoordinator::handleWheelEvent(FrameView*, const PlatformWheelEvent& wheelEvent)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

#if ENABLE(THREADED_SCROLLING)
    if (m_scrollingTree->willWheelEventStartSwipeGesture(wheelEvent))
        return false;

    ScrollingThread::dispatch(bind(&ScrollingTree::handleWheelEvent, m_scrollingTree.get(), wheelEvent));
#else
    UNUSED_PARAM(wheelEvent);
#endif
    return true;
}

void ScrollingCoordinator::updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
    frameView->setInProgrammaticScroll(programmaticScroll);

    frameView->setConstrainsScrollingToContentEdge(false);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setConstrainsScrollingToContentEdge(true);

    frameView->setInProgrammaticScroll(oldProgrammaticScroll);
}

void ScrollingCoordinator::updateMainFrameScrollLayerPosition()
{
#if USE(ACCELERATED_COMPOSITING) && ENABLE(THREADED_SCROLLING)
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    if (GraphicsLayer* scrollLayer = scrollLayerForFrameView(frameView))
        scrollLayer->setPosition(-frameView->scrollPosition());
#endif
}

void ScrollingCoordinator::updateMainFrameScrollPositionAndScrollLayerPosition()
{
#if USE(ACCELERATED_COMPOSITING) && ENABLE(THREADED_SCROLLING)
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    IntPoint scrollPosition = m_scrollingTree->mainFrameScrollPosition();

    frameView->setConstrainsScrollingToContentEdge(false);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setConstrainsScrollingToContentEdge(true);

    if (GraphicsLayer* scrollLayer = scrollLayerForFrameView(frameView))
        scrollLayer->setPosition(-frameView->scrollPosition());
#endif
}

#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
void ScrollingCoordinator::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    frameView->scrollAnimator()->handleWheelEventPhase(phase);
}
#endif

void ScrollingCoordinator::recomputeWheelEventHandlerCount()
{
    unsigned wheelEventHandlerCount = 0;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            wheelEventHandlerCount += frame->document()->wheelEventHandlerCount();
    }
    setWheelEventHandlerCount(wheelEventHandlerCount);
}

bool ScrollingCoordinator::shouldUpdateScrollLayerPositionOnMainThread() const
{
    // FIXME: Having fixed objects on the page should not trigger the slow path.
    FrameView* frameView = m_page->mainFrame()->view();
    return m_forceMainThreadScrollLayerPositionUpdates || frameView->hasSlowRepaintObjects() || frameView->hasFixedObjects() || m_page->mainFrame()->document()->isImageDocument();
}

void ScrollingCoordinator::updateShouldUpdateScrollLayerPositionOnMainThread()
{
    setShouldUpdateScrollLayerPositionOnMainThread(shouldUpdateScrollLayerPositionOnMainThread());
}

void ScrollingCoordinator::setForceMainThreadScrollLayerPositionUpdates(bool forceMainThreadScrollLayerPositionUpdates)
{
    if (m_forceMainThreadScrollLayerPositionUpdates == forceMainThreadScrollLayerPositionUpdates)
        return;

    m_forceMainThreadScrollLayerPositionUpdates = forceMainThreadScrollLayerPositionUpdates;
    updateShouldUpdateScrollLayerPositionOnMainThread();
}

#if ENABLE(THREADED_SCROLLING)
void ScrollingCoordinator::setScrollLayer(GraphicsLayer* scrollLayer)
{
    m_scrollingTreeState->setScrollLayer(scrollLayer);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setNonFastScrollableRegion(const Region& region)
{
    m_scrollingTreeState->setNonFastScrollableRegion(region);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setScrollParameters(const ScrollParameters& scrollParameters)
{
    m_scrollingTreeState->setHorizontalScrollElasticity(scrollParameters.horizontalScrollElasticity);
    m_scrollingTreeState->setVerticalScrollElasticity(scrollParameters.verticalScrollElasticity);
    m_scrollingTreeState->setHasEnabledHorizontalScrollbar(scrollParameters.hasEnabledHorizontalScrollbar);
    m_scrollingTreeState->setHasEnabledVerticalScrollbar(scrollParameters.hasEnabledVerticalScrollbar);
    m_scrollingTreeState->setHorizontalScrollbarMode(scrollParameters.horizontalScrollbarMode);
    m_scrollingTreeState->setVerticalScrollbarMode(scrollParameters.verticalScrollbarMode);

    m_scrollingTreeState->setScrollOrigin(scrollParameters.scrollOrigin);
    m_scrollingTreeState->setViewportRect(scrollParameters.viewportRect);
    m_scrollingTreeState->setContentsSize(scrollParameters.contentsSize);
    scheduleTreeStateCommit();
}


void ScrollingCoordinator::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    m_scrollingTreeState->setWheelEventHandlerCount(wheelEventHandlerCount);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(bool shouldUpdateScrollLayerPositionOnMainThread)
{
    // The FrameView's GraphicsLayer is likely to be out-of-synch with the PlatformLayer
    // at this point. So we'll update it before we switch back to main thread scrolling
    // in order to avoid layer positioning bugs.
    if (shouldUpdateScrollLayerPositionOnMainThread)
        updateMainFrameScrollLayerPosition();
    m_scrollingTreeState->setShouldUpdateScrollLayerPositionOnMainThread(shouldUpdateScrollLayerPositionOnMainThread);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::scheduleTreeStateCommit()
{
    if (m_scrollingTreeStateCommitterTimer.isActive())
        return;

    if (!m_scrollingTreeState->hasChangedProperties())
        return;

    m_scrollingTreeStateCommitterTimer.startOneShot(0);
}

void ScrollingCoordinator::scrollingTreeStateCommitterTimerFired(Timer<ScrollingCoordinator>*)
{
    commitTreeState();
}

void ScrollingCoordinator::commitTreeStateIfNeeded()
{
    if (!m_scrollingTreeState->hasChangedProperties())
        return;

    commitTreeState();
    m_scrollingTreeStateCommitterTimer.stop();
}

void ScrollingCoordinator::commitTreeState()
{
    ASSERT(m_scrollingTreeState->hasChangedProperties());

    OwnPtr<ScrollingTreeState> treeState = m_scrollingTreeState->commit();
    ScrollingThread::dispatch(bind(&ScrollingTree::commitNewTreeState, m_scrollingTree.get(), treeState.release()));
}
#endif

} // namespace WebCore
