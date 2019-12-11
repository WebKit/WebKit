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

#ifndef SmartMagnificationController_h
#define SmartMagnificationController_h

#if PLATFORM(IOS_FAMILY)

#include "MessageReceiver.h"
#include <WebCore/FloatRect.h>
#include <wtf/Noncopyable.h>

OBJC_CLASS WKContentView;
OBJC_CLASS UIScrollView;

namespace WebKit {

class WebPageProxy;

class SmartMagnificationController : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SmartMagnificationController);
public:
    SmartMagnificationController(WKContentView *);
    ~SmartMagnificationController();

    void handleSmartMagnificationGesture(WebCore::FloatPoint origin);
    void handleResetMagnificationGesture(WebCore::FloatPoint origin);

    double zoomFactorForTargetRect(WebCore::FloatRect targetRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale);

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didCollectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin, WebCore::FloatRect renderRect, WebCore::FloatRect visibleContentBounds, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale);
    void magnify(WebCore::FloatPoint origin, WebCore::FloatRect targetRect, WebCore::FloatRect visibleContentRect, double viewportMinimumScale, double viewportMaximumScale);
    void scrollToRect(WebCore::FloatPoint origin, WebCore::FloatRect targetRect);
    std::tuple<WebCore::FloatRect, double, double> smartMagnificationTargetRectAndZoomScales(WebCore::FloatRect targetRect, double minimumScale, double maximumScale, bool addMagnificationPadding);

    WebPageProxy& m_webPageProxy;
    WKContentView *m_contentView;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)

#endif // SmartMagnificationController_h
