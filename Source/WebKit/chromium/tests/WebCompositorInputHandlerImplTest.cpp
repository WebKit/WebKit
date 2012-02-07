/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebCompositorInputHandlerImpl.h"

#include "WebCompositor.h"
#include "WebCompositorInputHandlerClient.h"
#include "WebInputEvent.h"
#include "cc/CCInputHandler.h"
#include "cc/CCSingleThreadProxy.h"

#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

using WebKit::WebCompositorInputHandler;
using WebKit::WebCompositorInputHandlerImpl;

namespace {

class MockInputHandlerClient : public WebCore::CCInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(MockInputHandlerClient);
public:
    MockInputHandlerClient()
        : m_scrollStatus(ScrollStarted)
    {
    }
    virtual ~MockInputHandlerClient() { }

    void setScrollStatus(ScrollStatus status) { m_scrollStatus = status; }

private:
    virtual void setNeedsRedraw() OVERRIDE { }
    virtual ScrollStatus scrollBegin(const WebCore::IntPoint&) OVERRIDE
    {
        return m_scrollStatus;
    }
    virtual void scrollBy(const WebCore::IntSize&) OVERRIDE { }
    virtual void scrollEnd() OVERRIDE { }

    virtual bool haveWheelEventHandlers() OVERRIDE { return false; }
    virtual void pinchGestureBegin() OVERRIDE { }
    virtual void pinchGestureUpdate(float magnifyDelta, const WebCore::IntPoint& anchor) OVERRIDE { }
    virtual void pinchGestureEnd() OVERRIDE { }
    virtual void startPageScaleAnimation(const WebCore::IntSize& targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTimeMs,
                                         double durationMs) OVERRIDE { }

    ScrollStatus m_scrollStatus;
};

class MockWebCompositorInputHandlerClient : public WebKit::WebCompositorInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(MockWebCompositorInputHandlerClient);
public:
    MockWebCompositorInputHandlerClient()
        : m_handled(false)
        , m_sendToWidget(false)
    {
    }
    virtual ~MockWebCompositorInputHandlerClient() { }

    void reset()
    {
        m_handled = false;
        m_sendToWidget = false;
    }

    bool handled() const { return m_handled; }
    bool sendToWidget() const { return m_sendToWidget; }

private:
    virtual void willShutdown() OVERRIDE { }
    virtual void didHandleInputEvent() OVERRIDE
    {
        m_handled = true;
    }
    virtual void didNotHandleInputEvent(bool sendToWidget) OVERRIDE
    {
        m_sendToWidget = sendToWidget;
    }

    bool m_handled;
    bool m_sendToWidget;
};

TEST(WebCompositorInputHandlerImpl, fromIdentifier)
{
    WebKit::WebCompositor::initialize(0);
#ifndef NDEBUG
    // WebCompositorInputHandler APIs can only be called from the compositor thread.
    WebCore::DebugScopedSetImplThread alwaysImplThread;
#endif

    // Before creating any WebCompositorInputHandlers, lookups for any value should fail and not crash.
    EXPECT_EQ(0, WebCompositorInputHandler::fromIdentifier(2));
    EXPECT_EQ(0, WebCompositorInputHandler::fromIdentifier(0));
    EXPECT_EQ(0, WebCompositorInputHandler::fromIdentifier(-1));

    int compositorIdentifier = -1;
    {
        OwnPtr<WebCompositorInputHandlerImpl> inputHandler = WebCompositorInputHandlerImpl::create(0);
        compositorIdentifier = inputHandler->identifier();
        // The compositor we just created should be locatable.
        EXPECT_EQ(inputHandler.get(), WebCompositorInputHandler::fromIdentifier(compositorIdentifier));

        // But nothing else.
        EXPECT_EQ(0, WebCompositorInputHandler::fromIdentifier(inputHandler->identifier() + 10));
    }

    // After the compositor is destroyed, its entry should be removed from the map.
    EXPECT_EQ(0, WebCompositorInputHandler::fromIdentifier(compositorIdentifier));

    WebKit::WebCompositor::shutdown();
}

TEST(WebCompositorInputHandlerImpl, gestureScroll)
{
    WebKit::WebCompositor::initialize(0);
#ifndef NDEBUG
    // WebCompositorInputHandler APIs can only be called from the compositor thread.
    WebCore::DebugScopedSetImplThread alwaysImplThread;
#endif

    MockInputHandlerClient mockInputHandler;
    OwnPtr<WebCompositorInputHandlerImpl> inputHandler = WebCompositorInputHandlerImpl::create(&mockInputHandler);
    MockWebCompositorInputHandlerClient mockClient;
    inputHandler->setClient(&mockClient);

    WebKit::WebGestureEvent gesture;

    gesture.type = WebKit::WebInputEvent::GestureScrollBegin;
    inputHandler->handleInputEvent(gesture);
    EXPECT_TRUE(mockClient.handled());
    EXPECT_FALSE(mockClient.sendToWidget());
    mockClient.reset();

    gesture.type = WebKit::WebInputEvent::GestureScrollUpdate;
    gesture.deltaY = 40;
    inputHandler->handleInputEvent(gesture);
    EXPECT_TRUE(mockClient.handled());
    EXPECT_FALSE(mockClient.sendToWidget());
    mockClient.reset();

    gesture.type = WebKit::WebInputEvent::GestureScrollEnd;
    gesture.deltaY = 0;
    inputHandler->handleInputEvent(gesture);
    EXPECT_TRUE(mockClient.handled());
    EXPECT_FALSE(mockClient.sendToWidget());
    mockClient.reset();

    mockInputHandler.setScrollStatus(WebCore::CCInputHandlerClient::ScrollFailed);

    gesture.type = WebKit::WebInputEvent::GestureScrollBegin;
    inputHandler->handleInputEvent(gesture);
    EXPECT_FALSE(mockClient.handled());
    EXPECT_TRUE(mockClient.sendToWidget());
    mockClient.reset();

    gesture.type = WebKit::WebInputEvent::GestureScrollUpdate;
    gesture.deltaY = 40;
    inputHandler->handleInputEvent(gesture);
    EXPECT_FALSE(mockClient.handled());
    EXPECT_TRUE(mockClient.sendToWidget());
    mockClient.reset();

    gesture.type = WebKit::WebInputEvent::GestureScrollEnd;
    gesture.deltaY = 0;
    inputHandler->handleInputEvent(gesture);
    EXPECT_FALSE(mockClient.handled());
    EXPECT_TRUE(mockClient.sendToWidget());
    mockClient.reset();

    inputHandler->setClient(0);

    WebKit::WebCompositor::shutdown();
}

}
