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

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

#include <UIKit/UIScrollView.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/ScrollingTreeScrollingNode.h>
#include <WebCore/ScrollingTreeScrollingNodeDelegate.h>

OBJC_CLASS CALayer;
OBJC_CLASS WKScrollingNodeScrollViewDelegate;

namespace WebCore {

class FloatPoint;
class FloatRect;
class FloatSize;
class ScrollingTreeScrollingNode;

}

namespace WebKit {

class ScrollingTreeScrollingNodeDelegateIOS : public WebCore::ScrollingTreeScrollingNodeDelegate {
public:
    explicit ScrollingTreeScrollingNodeDelegateIOS(WebCore::ScrollingTreeScrollingNode&);
    ~ScrollingTreeScrollingNodeDelegateIOS() final;

    void scrollWillStart() const;
    void scrollDidEnd() const;
    void scrollViewWillStartPanGesture() const;
    void scrollViewDidScroll(const WebCore::FloatPoint& scrollPosition, bool inUserInteraction);

    void currentSnapPointIndicesDidChange(unsigned horizontal, unsigned vertical) const;
    CALayer *scrollLayer() const { return m_scrollLayer.get(); }

    void resetScrollViewDelegate();
    void commitStateBeforeChildren(const WebCore::ScrollingStateScrollingNode&);
    void commitStateAfterChildren(const WebCore::ScrollingStateScrollingNode&);

    void repositionScrollingLayers();

#if ENABLE(POINTER_EVENTS)
    Optional<TouchActionData> touchActionData() const;
    void cancelPointersForGestureRecognizer(UIGestureRecognizer*);
#endif

private:
    RetainPtr<CALayer> m_scrollLayer;
    RetainPtr<CALayer> m_scrolledContentsLayer;
    RetainPtr<WKScrollingNodeScrollViewDelegate> m_scrollViewDelegate;
    bool m_updatingFromStateNode { false };
};

} // namespace WebKit

@interface WKScrollingNodeScrollViewDelegate : NSObject <UIScrollViewDelegate> {
    WebKit::ScrollingTreeScrollingNodeDelegateIOS* _scrollingTreeNodeDelegate;
}

@property (nonatomic, getter=_isInUserInteraction) BOOL inUserInteraction;

- (instancetype)initWithScrollingTreeNodeDelegate:(WebKit::ScrollingTreeScrollingNodeDelegateIOS*)delegate;

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
