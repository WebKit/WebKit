/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if HAVE(APPKIT_GESTURES_SUPPORT)

#import "WKAppKitGestureController.h"

@interface WKAppKitGestureController (MomentumEventGeneration)
- (void)generatedMomentumDidEnd;
@end

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)

#import "MomentumEventDispatcher.h"
#import <WebCore/PageIdentifier.h>
#import <wtf/Markable.h>
#import <wtf/RetainPtr.h>
#import <wtf/SwiftBridging.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakObjCPtr.h>

OBJC_CLASS CADisplayLink;
OBJC_CLASS WKAppKitGestureMomentumDisplayLinkTarget;

namespace WebKit {

class NativeWebWheelEvent;
class WebPageProxy;

class AppKitGestureMomentumEventGenerator final : public MomentumEventDispatcher::Client {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(AppKitGestureMomentumEventGenerator);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AppKitGestureMomentumEventGenerator);
public:
    explicit AppKitGestureMomentumEventGenerator(WKAppKitGestureController *);
    ~AppKitGestureMomentumEventGenerator();

    bool isActive() const { return m_isActive; }

    void start(WebPageProxy&, const NativeWebWheelEvent&);
    void interrupt();
    void displayDidRefresh();

private:
    // Swift can call private virtual functions through the wrappers it generates, so these, which only the dispatcher
    // calls, are unavailable to it.
    NS_SWIFT_UNAVAILABLE("") void handleSyntheticWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<WebCore::RubberBandingBehavior>) final;
    NS_SWIFT_UNAVAILABLE("") void startDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID) final;
    NS_SWIFT_UNAVAILABLE("") void stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID) final;
    NS_SWIFT_UNAVAILABLE("") void didEndSyntheticMomentumScrolling() final;
#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    NS_SWIFT_UNAVAILABLE("") void flushMomentumEventLoggingSoon() final { }
#endif

    WeakObjCPtr<WKAppKitGestureController> m_controller;
    Markable<WebCore::PageIdentifier> m_pageIdentifier;
    WebCore::PlatformDisplayID m_displayID { 0 };
    bool m_isActive { false };
    bool m_isCallingDispatcher { false };
    RetainPtr<WKAppKitGestureMomentumDisplayLinkTarget> m_displayLinkTarget;
    RetainPtr<CADisplayLink> m_displayLink;
    const std::unique_ptr<MomentumEventDispatcher> m_dispatcher;
} SWIFT_SHARED_REFERENCE(incrementCheckedPtrCountOnAppKitGestureMomentumEventGenerator, decrementCheckedPtrCountOnAppKitGestureMomentumEventGenerator) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

} // namespace WebKit

SWIFT_UNSAFE inline void incrementCheckedPtrCountOnAppKitGestureMomentumEventGenerator(WebKit::AppKitGestureMomentumEventGenerator* obj)
{
    obj->incrementCheckedPtrCount();
}

SWIFT_UNSAFE inline void decrementCheckedPtrCountOnAppKitGestureMomentumEventGenerator(WebKit::AppKitGestureMomentumEventGenerator* obj)
{
    obj->decrementCheckedPtrCount();
}

#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
