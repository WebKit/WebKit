/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include <WebCore/ScrollingTreeScrollingNode.h>

#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ScrollingTreeScrollingNodeDelegate {
    WTF_MAKE_TZONE_ALLOCATED(ScrollingTreeScrollingNodeDelegate);
public:
    WEBCORE_EXPORT explicit ScrollingTreeScrollingNodeDelegate(ScrollingTreeScrollingNode&);
    WEBCORE_EXPORT virtual ~ScrollingTreeScrollingNodeDelegate();

    RefPtr<ScrollingTreeScrollingNode> protectedScrollingNode() const { return m_scrollingNode.get(); }
    ScrollingTreeScrollingNode& scrollingNode() { return  *protectedScrollingNode().unsafeGet(); }
    const ScrollingTreeScrollingNode& scrollingNode() const { return  *protectedScrollingNode().unsafeGet(); }
    
    virtual bool startAnimatedScrollToPosition(FloatPoint) = 0;
    virtual void stopAnimatedScroll() = 0;

    virtual void serviceScrollAnimation(MonotonicTime) = 0;

    virtual void updateFromStateNode(const ScrollingStateScrollingNode&) { }
    
    virtual void handleWheelEventPhase(const PlatformWheelEventPhase) { }
    
    virtual void viewWillStartLiveResize() { }
    virtual void viewWillEndLiveResize() { }
    virtual void viewSizeDidChange() { }
    
    virtual void updateScrollbarLayers() { }
    virtual void initScrollbars() { }

    virtual void handleKeyboardScrollRequest(const RequestedKeyboardScrollData&) { }

    virtual FloatPoint adjustedScrollPosition(const FloatPoint& scrollPosition) const { return scrollPosition; }
    virtual String scrollbarStateForOrientation(ScrollbarOrientation) const { return ""_s; }

protected:
    WEBCORE_EXPORT RefPtr<ScrollingTree> scrollingTree() const;

    WEBCORE_EXPORT FloatPoint lastCommittedScrollPosition() const;
    WEBCORE_EXPORT FloatSize totalContentsSize();
    WEBCORE_EXPORT FloatSize reachableContentsSize();
    WEBCORE_EXPORT IntPoint scrollOrigin() const;

    FloatPoint currentScrollPosition() const { return protectedScrollingNode()->currentScrollPosition(); }
    FloatPoint minimumScrollPosition() const { return protectedScrollingNode()->minimumScrollPosition(); }
    FloatPoint maximumScrollPosition() const { return protectedScrollingNode()->maximumScrollPosition(); }

    FloatSize scrollableAreaSize() const { return protectedScrollingNode()->scrollableAreaSize(); }
    FloatSize totalContentsSize() const { return protectedScrollingNode()->totalContentsSize(); }

    bool allowsHorizontalScrolling() const { return protectedScrollingNode()->allowsHorizontalScrolling(); }
    bool allowsVerticalScrolling() const { return protectedScrollingNode()->allowsVerticalScrolling(); }

    ScrollElasticity horizontalScrollElasticity() const { return protectedScrollingNode()->horizontalScrollElasticity(); }
    ScrollElasticity verticalScrollElasticity() const { return protectedScrollingNode()->verticalScrollElasticity(); }

private:
    ThreadSafeWeakPtr<ScrollingTreeScrollingNode> m_scrollingNode; // m_scrollingNode is expected never be null
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
