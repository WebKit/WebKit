/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef ViewGestureGeometryCollector_h
#define ViewGestureGeometryCollector_h

#if !PLATFORM(IOS)

#include "MessageReceiver.h"
#include <WTF/RunLoop.h>

namespace WebCore {
class FloatPoint;
}

namespace WebKit {

class WebPage;

class ViewGestureGeometryCollector : private IPC::MessageReceiver {
public:
    ViewGestureGeometryCollector(WebPage&);
    ~ViewGestureGeometryCollector();

    void mainFrameDidLayout();

private:
    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    // Message handlers.
    void collectGeometryForMagnificationGesture();
    void collectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin);
    void setRenderTreeSizeNotificationThreshold(uint64_t size) { m_renderTreeSizeNotificationThreshold = size; }

    void renderTreeSizeNotificationTimerFired();

    WebPage& m_webPage;

    uint64_t m_renderTreeSizeNotificationThreshold;
    RunLoop::Timer<ViewGestureGeometryCollector> m_renderTreeSizeNotificationTimer;
};

} // namespace WebKit

#endif // !PLATFORM(IOS)

#endif // ViewGestureGeometryCollector
