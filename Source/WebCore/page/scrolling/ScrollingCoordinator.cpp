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
#include "ScrollingStateScrollingNode.h"
#include "ScrollingStateTree.h"
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
    , m_scrollingStateTree(ScrollingStateTree::create())
    , m_scrollingTree(ScrollingTree::create(this))
    , m_scrollingStateTreeCommitterTimer(this, &ScrollingCoordinator::scrollingStateTreeCommitterTimerFired)
#endif
    , m_private(0)
{
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;

#if ENABLE(THREADED_SCROLLING)
    m_scrollingStateTreeCommitterTimer.stop();

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

#if ENABLE(THREADED_SCROLLING)
    // In the future, we may want to have the ability to set non-fast scrolling regions for more than
    // just the root node. But right now, this concept only applies to the root.
    setNonFastScrollableRegionForNode(nonFastScrollableRegion, m_scrollingStateTree->rootStateNode());
#else
    setNonFastScrollableRegion(nonFastScrollableRegion);
#endif

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

#if ENABLE(THREADED_SCROLLING)
    ScrollingStateScrollingNode* node = stateNodeForID(frameView->scrollLayerID());
    if (!node)
        return;

    setScrollParametersForNode(scrollParameters, node);
#else
    setScrollParameters(scrollParameters);
#endif
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

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView* frameView)
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

#if ENABLE(THREADED_SCROLLING)
    // If the root layer does not have a ScrollingStateNode, then we should create one.
    ensureRootStateNodeForFrameView(frameView);
    ASSERT(m_scrollingStateTree->rootStateNode());
#endif

    frameViewLayoutUpdated(frameView);
    recomputeWheelEventHandlerCountForFrameView(frameView);
    updateShouldUpdateScrollLayerPositionOnMainThread();
    
#if ENABLE(THREADED_SCROLLING)
    setScrollLayerForNode(scrollLayerForFrameView(frameView), stateNodeForID(frameView->scrollLayerID()));
#else
    setScrollLayer(scrollLayerForFrameView(frameView));
#endif
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

    ScrollingStateScrollingNode* stateNode = stateNodeForID(frameView->scrollLayerID());
    if (!stateNode)
        return false;

    stateNode->setRequestedScrollPosition(scrollPosition, frameView->inProgrammaticScroll());
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

#if ENABLE(THREADED_SCROLLING)
void ScrollingCoordinator::updateMainFrameScrollLayerPosition()
{
#if USE(ACCELERATED_COMPOSITING)
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
#endif

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

void ScrollingCoordinator::recomputeWheelEventHandlerCountForFrameView(FrameView* frameView)
{
    unsigned wheelEventHandlerCount = 0;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            wheelEventHandlerCount += frame->document()->wheelEventHandlerCount();
    }

#if ENABLE(THREADED_SCROLLING)
    ScrollingStateScrollingNode* node = stateNodeForID(frameView->scrollLayerID());
    if (!node)
        return;
    setWheelEventHandlerCountForNode(wheelEventHandlerCount, node);
#else
    setWheelEventHandlerCount(wheelEventHandlerCount);
    UNUSED_PARAM(frameView);
#endif
}

bool ScrollingCoordinator::hasNonLayerFixedObjects(FrameView* frameView)
{
    const FrameView::ViewportConstrainedObjectSet* viewportConstrainedObjects = frameView->viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

#if USE(ACCELERATED_COMPOSITING)
    for (FrameView::ViewportConstrainedObjectSet::const_iterator it = viewportConstrainedObjects->begin(), end = viewportConstrainedObjects->end(); it != end; ++it) {
        RenderObject* viewportConstrainedObject = *it;
        if (!viewportConstrainedObject->isBoxModelObject() || !viewportConstrainedObject->hasLayer())
            return true;
        RenderBoxModelObject* viewportConstrainedBoxModelObject = toRenderBoxModelObject(viewportConstrainedObject);
        if (!viewportConstrainedBoxModelObject->layer()->backing())
            return true;
    }
    return false;
#else
    return viewportConstrainedObjects->size();
#endif
}

void ScrollingCoordinator::updateShouldUpdateScrollLayerPositionOnMainThread()
{
    FrameView* frameView = m_page->mainFrame()->view();

    MainThreadScrollingReasons mainThreadScrollingReasons = (MainThreadScrollingReasons)0;

    if (m_forceMainThreadScrollLayerPositionUpdates)
        mainThreadScrollingReasons |= ForcedOnMainThread;
    if (frameView->hasSlowRepaintObjects())
        mainThreadScrollingReasons |= HasSlowRepaintObjects;
    if (!supportsFixedPositionLayers() && frameView->hasViewportConstrainedObjects())
        mainThreadScrollingReasons |= HasViewportConstrainedObjectsWithoutSupportingFixedLayers;
    if (supportsFixedPositionLayers() && hasNonLayerFixedObjects(frameView))
        mainThreadScrollingReasons |= HasNonLayerFixedObjects;
    if (m_page->mainFrame()->document()->isImageDocument())
        mainThreadScrollingReasons |= IsImageDocument;

    setShouldUpdateScrollLayerPositionOnMainThread(mainThreadScrollingReasons);
}

void ScrollingCoordinator::setForceMainThreadScrollLayerPositionUpdates(bool forceMainThreadScrollLayerPositionUpdates)
{
    if (m_forceMainThreadScrollLayerPositionUpdates == forceMainThreadScrollLayerPositionUpdates)
        return;

    m_forceMainThreadScrollLayerPositionUpdates = forceMainThreadScrollLayerPositionUpdates;
    updateShouldUpdateScrollLayerPositionOnMainThread();
}

ScrollingNodeID ScrollingCoordinator::uniqueScrollLayerID()
{
    static ScrollingNodeID uniqueScrollLayerID = 1;
    return uniqueScrollLayerID++;
}

#if ENABLE(THREADED_SCROLLING)
ScrollingStateScrollingNode* ScrollingCoordinator::stateNodeForID(ScrollingNodeID scrollLayerID)
{
    if (!scrollLayerID)
        return 0;

    HashMap<ScrollingNodeID, ScrollingStateNode*>::const_iterator it = m_stateNodeMap.find(scrollLayerID);
    if (it == m_stateNodeMap.end())
        return 0;

    ScrollingStateNode* node = it->value;
    return toScrollingStateScrollingNode(node);
}

ScrollingNodeID ScrollingCoordinator::attachToStateTree(ScrollingNodeID scrollLayerID)
{
    ASSERT(scrollLayerID);

    ScrollingStateScrollingNode* existingNode = stateNodeForID(scrollLayerID);
    if (existingNode && existingNode == m_scrollingStateTree->rootStateNode())
        return scrollLayerID;

    clearStateTree();

    // FIXME: In the future, this function will have to take a parent ID so that it can
    // append the node in the appropriate spot in the state tree. For now we always assume
    // this is the root node.
    m_scrollingStateTree->setRootStateNode(ScrollingStateScrollingNode::create(m_scrollingStateTree.get()));
    m_stateNodeMap.set(scrollLayerID, m_scrollingStateTree->rootStateNode());
    return scrollLayerID;
}

void ScrollingCoordinator::detachFromStateTree(ScrollingNodeID scrollLayerID)
{
    if (!scrollLayerID)
        return;

    ScrollingStateNode* node = m_stateNodeMap.take(scrollLayerID);

    // FIXME: removeNode() will destroy children, and those children might still be in the HashMap.
    // This will be a big problem once there are actually children in the tree.
    m_scrollingStateTree->removeNode(node);
}

void ScrollingCoordinator::clearStateTree()
{
    m_stateNodeMap.clear();
    if (ScrollingStateScrollingNode* node = m_scrollingStateTree->rootStateNode())
        m_scrollingStateTree->removeNode(node);
}

void ScrollingCoordinator::ensureRootStateNodeForFrameView(FrameView* frameView)
{
    attachToStateTree(frameView->scrollLayerID());
}

void ScrollingCoordinator::setScrollLayerForNode(GraphicsLayer* scrollLayer, ScrollingStateNode* node)
{
    node->setScrollLayer(scrollLayer);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setNonFastScrollableRegionForNode(const Region& region, ScrollingStateScrollingNode* node)
{
    node->setNonFastScrollableRegion(region);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setScrollParametersForNode(const ScrollParameters& scrollParameters, ScrollingStateScrollingNode* node)
{
    node->setHorizontalScrollElasticity(scrollParameters.horizontalScrollElasticity);
    node->setVerticalScrollElasticity(scrollParameters.verticalScrollElasticity);
    node->setHasEnabledHorizontalScrollbar(scrollParameters.hasEnabledHorizontalScrollbar);
    node->setHasEnabledVerticalScrollbar(scrollParameters.hasEnabledVerticalScrollbar);
    node->setHorizontalScrollbarMode(scrollParameters.horizontalScrollbarMode);
    node->setVerticalScrollbarMode(scrollParameters.verticalScrollbarMode);

    node->setScrollOrigin(scrollParameters.scrollOrigin);
    node->setViewportRect(scrollParameters.viewportRect);
    node->setContentsSize(scrollParameters.contentsSize);
    scheduleTreeStateCommit();
}


void ScrollingCoordinator::setWheelEventHandlerCountForNode(unsigned wheelEventHandlerCount, ScrollingStateScrollingNode* node)
{
    node->setWheelEventHandlerCount(wheelEventHandlerCount);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons reasons)
{
    // The FrameView's GraphicsLayer is likely to be out-of-synch with the PlatformLayer
    // at this point. So we'll update it before we switch back to main thread scrolling
    // in order to avoid layer positioning bugs.
    if (reasons)
        updateMainFrameScrollLayerPosition();
    m_scrollingStateTree->rootStateNode()->setShouldUpdateScrollLayerPositionOnMainThread(reasons);
    scheduleTreeStateCommit();
}

void ScrollingCoordinator::scheduleTreeStateCommit()
{
    if (m_scrollingStateTreeCommitterTimer.isActive())
        return;

    if (!m_scrollingStateTree->hasChangedProperties())
        return;

    m_scrollingStateTreeCommitterTimer.startOneShot(0);
}

void ScrollingCoordinator::scrollingStateTreeCommitterTimerFired(Timer<ScrollingCoordinator>*)
{
    commitTreeState();
}

void ScrollingCoordinator::commitTreeStateIfNeeded()
{
    if (!m_scrollingStateTree->hasChangedProperties())
        return;

    commitTreeState();
    m_scrollingStateTreeCommitterTimer.stop();
}

void ScrollingCoordinator::commitTreeState()
{
    ASSERT(m_scrollingStateTree->hasChangedProperties());

    OwnPtr<ScrollingStateTree> treeState = m_scrollingStateTree->commit();
    ScrollingThread::dispatch(bind(&ScrollingTree::commitNewTreeState, m_scrollingTree.get(), treeState.release()));
}

bool ScrollingCoordinator::supportsFixedPositionLayers() const
{
    return false;
}

void ScrollingCoordinator::setLayerIsContainerForFixedPositionLayers(GraphicsLayer*, bool)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::setLayerIsFixedToContainerLayer(GraphicsLayer*, bool)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::scrollableAreaScrollLayerDidChange(ScrollableArea*, GraphicsLayer*)
{
    // FIXME: Implement.
}
#endif // !ENABLE(THREADED_SCROLLING)

} // namespace WebCore
