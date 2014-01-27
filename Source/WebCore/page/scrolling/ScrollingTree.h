/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef ScrollingTree_h
#define ScrollingTree_h

#if ENABLE(ASYNC_SCROLLING)

#include "PlatformWheelEvent.h"
#include "Region.h"
#include "ScrollingCoordinator.h"
#include <wtf/Functional.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class IntPoint;
class ScrollingStateTree;
class ScrollingStateNode;
class ScrollingTreeNode;
class ScrollingTreeScrollingNode;

class ScrollingTree : public ThreadSafeRefCounted<ScrollingTree> {
public:
    ScrollingTree();
    virtual ~ScrollingTree();

    enum EventResult {
        DidNotHandleEvent,
        DidHandleEvent,
        SendToMainThread
    };
    
    virtual bool isThreadedScrollingTree() const { return false; }
    virtual bool isRemoteScrollingTree() const { return false; }
    virtual bool isScrollingTreeIOS() const { return false; }

    virtual EventResult tryToHandleWheelEvent(const PlatformWheelEvent&) = 0;
    bool shouldHandleWheelEventSynchronously(const PlatformWheelEvent&);
    
    virtual void scrollPositionChangedViaDelegatedScrolling(ScrollingNodeID, const IntPoint&);

    void setMainFrameIsRubberBanding(bool);
    bool isRubberBandInProgress();

    virtual void invalidate() { }
    virtual void commitNewTreeState(PassOwnPtr<ScrollingStateTree>);

    void setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight, bool pinnedToTheTop, bool pinnedToTheBottom);

    virtual void scrollingTreeNodeDidScroll(ScrollingNodeID, const IntPoint& scrollPosition, SetOrSyncScrollingLayerPosition = SyncScrollingLayerPosition) = 0;
    IntPoint mainFrameScrollPosition();

#if PLATFORM(MAC) && !PLATFORM(IOS)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) = 0;
#endif

    // Can be called from any thread. Will update what edges allow rubber-banding.
    void setCanRubberBandState(bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom);

    bool rubberBandsAtLeft();
    bool rubberBandsAtRight();
    bool rubberBandsAtTop();
    bool rubberBandsAtBottom();
    bool isHandlingProgrammaticScroll();
    
    void setScrollPinningBehavior(ScrollPinningBehavior);
    ScrollPinningBehavior scrollPinningBehavior();

    bool willWheelEventStartSwipeGesture(const PlatformWheelEvent&);

    void setScrollingPerformanceLoggingEnabled(bool flag);
    bool scrollingPerformanceLoggingEnabled();

    ScrollingTreeScrollingNode* rootNode() const { return m_rootNode.get(); }

protected:
    void setMainFrameScrollPosition(IntPoint);
    virtual void handleWheelEvent(const PlatformWheelEvent&);

private:
    void removeDestroyedNodes(const ScrollingStateTree&);
    void updateTreeFromStateNode(const ScrollingStateNode*);

    virtual PassOwnPtr<ScrollingTreeNode> createNode(ScrollingNodeType, ScrollingNodeID) = 0;

    ScrollingTreeNode* nodeForID(ScrollingNodeID) const;

    OwnPtr<ScrollingTreeScrollingNode> m_rootNode;

    typedef HashMap<ScrollingNodeID, ScrollingTreeNode*> ScrollingTreeNodeMap;
    ScrollingTreeNodeMap m_nodeMap;

    Mutex m_mutex;
    Region m_nonFastScrollableRegion;
    IntPoint m_mainFrameScrollPosition;
    bool m_hasWheelEventHandlers;

    Mutex m_swipeStateMutex;
    bool m_rubberBandsAtLeft;
    bool m_rubberBandsAtRight;
    bool m_rubberBandsAtTop;
    bool m_rubberBandsAtBottom;
    bool m_mainFramePinnedToTheLeft;
    bool m_mainFramePinnedToTheRight;
    bool m_mainFramePinnedToTheTop;
    bool m_mainFramePinnedToTheBottom;
    bool m_mainFrameIsRubberBanding;
    ScrollPinningBehavior m_scrollPinningBehavior;

    bool m_scrollingPerformanceLoggingEnabled;
    
    bool m_isHandlingProgrammaticScroll;
};

#define SCROLLING_TREE_TYPE_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, WebCore::ScrollingTree, value, value->predicate, value.predicate)

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTree_h
