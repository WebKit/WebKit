/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef ViewGestureController_h
#define ViewGestureController_h

#if !PLATFORM(IOS)

#include "MessageReceiver.h"
#include <WebCore/FloatRect.h>
#include <WebCore/Timer.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS NSEvent;
OBJC_CLASS NSView;

namespace WebKit {

class WebBackForwardListItem;
class WebPageProxy;

class ViewGestureController : private IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(ViewGestureController);
public:
    ViewGestureController(WebPageProxy&);
    ~ViewGestureController();

    double magnification() const;

    void handleMagnificationGesture(double scale, WebCore::FloatPoint origin);
    void handleSmartMagnificationGesture(WebCore::FloatPoint origin);

    bool handleScrollWheelEvent(NSEvent *);
    void didHitRenderTreeSizeThreshold();

    void wheelEventWasNotHandledByWebCore(NSEvent *);

    void setCustomSwipeViews(Vector<RetainPtr<NSView>> views) { m_customSwipeViews = std::move(views); }
    WebCore::FloatRect windowRelativeBoundsForCustomSwipeViews() const;

    void endActiveGesture();

    enum class ViewGestureType {
        None,
        Magnification,
        SmartMagnification,
        Swipe
    };

    enum class SwipeTransitionStyle {
        Overlap,
        Push
    };

private:
    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    // Message handlers.
    void didCollectGeometryForMagnificationGesture(WebCore::FloatRect visibleContentBounds, bool frameHandlesMagnificationGesture);
    void didCollectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin, WebCore::FloatRect renderRect, WebCore::FloatRect visibleContentBounds, bool isReplacedElement, bool frameHandlesMagnificationGesture);

    void endMagnificationGesture();
    WebCore::FloatPoint scaledMagnificationOrigin(WebCore::FloatPoint origin, double scale);

    enum class SwipeDirection { Left, Right };
    void trackSwipeGesture(NSEvent *, SwipeDirection);
    void beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection);
    void handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection);
    void endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled);
    void removeSwipeSnapshot();
    void swipeSnapshotWatchdogTimerFired(WebCore::Timer<ViewGestureController>*);

    WebPageProxy& m_webPageProxy;

    double m_magnification;
    WebCore::FloatPoint m_magnificationOrigin;

    WebCore::FloatRect m_lastSmartMagnificationUnscaledTargetRect;
    bool m_lastMagnificationGestureWasSmartMagnification;
    WebCore::FloatPoint m_lastSmartMagnificationOrigin;

    ViewGestureType m_activeGestureType;

    WebCore::FloatRect m_visibleContentRect;
    bool m_visibleContentRectIsValid;
    bool m_frameHandlesMagnificationGesture;

    RetainPtr<CALayer> m_swipeSnapshotLayer;
    Vector<RetainPtr<CALayer>> m_currentSwipeLiveLayers;

    SwipeTransitionStyle m_swipeTransitionStyle;
    WebCore::Timer<ViewGestureController> m_swipeWatchdogTimer;
    Vector<RetainPtr<NSView>> m_customSwipeViews;
    WebCore::FloatRect m_currentSwipeCustomViewBounds;

    // If we need to wait for content to decide if it is going to consume
    // the scroll event that would have started a swipe, we'll fill these in.
    bool m_hasPendingSwipe;
    SwipeDirection m_pendingSwipeDirection;
};

} // namespace WebKit

#endif // !PLATFORM(IOS)

#endif // ViewGestureController_h
