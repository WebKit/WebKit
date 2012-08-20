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

#include "CCActiveGestureAnimation.h"
#include "CCInputHandler.h"
#include "CCSingleThreadProxy.h"
#include "WebCompositorInputHandlerClient.h"
#include "WebInputEvent.h"
#include <public/WebCompositor.h>
#include <public/WebFloatPoint.h>
#include <public/WebPoint.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

using namespace WebKit;

namespace {

class MockCCInputHandlerClient : public WebCore::CCInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(MockCCInputHandlerClient);
public:
    MockCCInputHandlerClient()
    {
    }
    virtual ~MockCCInputHandlerClient() { }


    MOCK_METHOD0(pinchGestureBegin, void());
    MOCK_METHOD2(pinchGestureUpdate, void(float magnifyDelta, const WebCore::IntPoint& anchor));
    MOCK_METHOD0(pinchGestureEnd, void());

    MOCK_METHOD0(scheduleAnimation, void());

    MOCK_METHOD2(scrollBegin, ScrollStatus(const WebCore::IntPoint&, WebCore::CCInputHandlerClient::ScrollInputType));
    MOCK_METHOD2(scrollBy, void(const WebCore::IntPoint&, const WebCore::IntSize&));
    MOCK_METHOD0(scrollEnd, void());

private:
    virtual void startPageScaleAnimation(const WebCore::IntSize& targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTimeMs,
                                         double durationMs) OVERRIDE { }

    virtual WebCore::CCActiveGestureAnimation* activeGestureAnimation() OVERRIDE { return 0; }
    virtual void setActiveGestureAnimation(PassOwnPtr<WebCore::CCActiveGestureAnimation>) OVERRIDE { }
};

class MockWebCompositorInputHandlerClient : public WebCompositorInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(MockWebCompositorInputHandlerClient);
public:
    MockWebCompositorInputHandlerClient()
        : WebCompositorInputHandlerClient()
    {
    }
    virtual ~MockWebCompositorInputHandlerClient() { }

    MOCK_METHOD0(willShutdown, void());
    MOCK_METHOD0(didHandleInputEvent, void());
    MOCK_METHOD1(didNotHandleInputEvent, void(bool sendToWidget));

    MOCK_METHOD1(transferActiveWheelFlingAnimation, void(const WebActiveWheelFlingParameters&));

};

TEST(WebCompositorInputHandlerImpl, fromIdentifier)
{
    WebCompositor::initialize(0);
    WebCore::DebugScopedSetImplThread alwaysImplThread;

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
    WebCompositor::shutdown();
}

class WebCompositorInitializer {
public:
    WebCompositorInitializer()
    {
        WebCompositor::initialize(0);
    }

    ~WebCompositorInitializer()
    {
        WebCompositor::shutdown();
    }
};

class WebCompositorInputHandlerImplTest : public testing::Test {
public:
    WebCompositorInputHandlerImplTest()
        : m_expectedDisposition(DidHandle)
    {
        m_inputHandler = WebCompositorInputHandlerImpl::create(&m_mockCCInputHandlerClient);
        m_inputHandler->setClient(&m_mockClient);
    }

    ~WebCompositorInputHandlerImplTest()
    {
        m_inputHandler->setClient(0);
        m_inputHandler.clear();
    }

    // This is defined as a macro because when an expectation is not satisfied the only output you get
    // out of gmock is the line number that set the expectation.
#define VERIFY_AND_RESET_MOCKS() do                                                                                      \
    {                                                                                                                    \
        testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);                                          \
        testing::Mock::VerifyAndClearExpectations(&m_mockClient);                                                        \
        switch (m_expectedDisposition) {                                                                                 \
        case DidHandle:                                                                                                  \
            /* If we expect to handle events, we shouldn't get any didNotHandleInputEvent() calls with any parameter. */ \
            EXPECT_CALL(m_mockClient, didNotHandleInputEvent(::testing::_)).Times(0);                                    \
            EXPECT_CALL(m_mockClient, didHandleInputEvent());                                                            \
            break;                                                                                                       \
        case DidNotHandle:                                                                                               \
            /* If we aren't expecting to handle events, we shouldn't call didHandleInputEvent(). */                      \
            EXPECT_CALL(m_mockClient, didHandleInputEvent()).Times(0);                                                   \
            EXPECT_CALL(m_mockClient, didNotHandleInputEvent(false)).Times(0);                                           \
            EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true));                                                     \
            break;                                                                                                       \
        case DropEvent:                                                                                                  \
            /* If we're expecting to drop, we shouldn't get any didHandle..() or didNotHandleInputEvent(true) calls. */  \
            EXPECT_CALL(m_mockClient, didHandleInputEvent()).Times(0);                                                   \
            EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true)).Times(0);                                            \
            EXPECT_CALL(m_mockClient, didNotHandleInputEvent(false));                                                    \
            break;                                                                                                       \
        }                                                                                                                                                       \
    } while (0)

protected:
    MockCCInputHandlerClient m_mockCCInputHandlerClient;
    OwnPtr<WebCompositorInputHandlerImpl> m_inputHandler;
    MockWebCompositorInputHandlerClient m_mockClient;
    WebGestureEvent gesture;
    WebCore::DebugScopedSetImplThread alwaysImplThread;
    WebCompositorInitializer initializer;

    enum ExpectedDisposition { DidHandle, DidNotHandle, DropEvent };
    ExpectedDisposition m_expectedDisposition;
};


TEST_F(WebCompositorInputHandlerImplTest, gestureScrollStarted)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.deltaY = -40; // -Y means scroll down - i.e. in the +Y direction.
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::Property(&WebCore::IntSize::height, testing::Gt(0))));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.deltaY = 0;
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd());
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureScrollOnMainThread)
{
    // We should send all events to the widget for this gesture.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(::testing::_, ::testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollOnMainThread));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.deltaY = 40;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.deltaY = 0;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureScrollIgnored)
{
    // We shouldn't handle the GestureScrollBegin.
    // Instead, we should get one didNotHandleInputEvent(false) call per handleInputEvent(),
    // indicating that we could determine that there's nothing that could scroll or otherwise
    // react to this gesture sequence and thus we should drop the whole gesture sequence on the floor.
    m_expectedDisposition = DropEvent;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollIgnored));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gesturePinch)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchBegin;
    EXPECT_CALL(m_mockCCInputHandlerClient, pinchGestureBegin());
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.deltaX = 1.5;
    gesture.x = 7;
    gesture.y = 13;
    EXPECT_CALL(m_mockCCInputHandlerClient, pinchGestureUpdate(1.5, WebCore::IntPoint(7, 13)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.deltaX = 0.5;
    gesture.x = 9;
    gesture.y = 6;
    EXPECT_CALL(m_mockCCInputHandlerClient, pinchGestureUpdate(.5, WebCore::IntPoint(9, 6)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchEnd;
    EXPECT_CALL(m_mockCCInputHandlerClient, pinchGestureEnd());
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingStarted)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.deltaX = 10;
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Verify that a GestureFlingCancel during an animation cancels it.
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingFailed)
{
    // We should send all events to the widget for this gesture.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollOnMainThread));

    gesture.type = WebInputEvent::GestureFlingStart;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Even if we didn't start a fling ourselves, we still need to send the cancel event to the widget.
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingIgnored)
{
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollIgnored));

    gesture.type = WebInputEvent::GestureFlingStart;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Even if we didn't start a fling ourselves, we still need to send the cancel event to the widget.
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingAnimates)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    // On the fling start, we should schedule an animation but not actually start
    // scrolling.
    gesture.type = WebInputEvent::GestureFlingStart;
    WebFloatPoint flingDelta = WebFloatPoint(1000, 0);
    WebPoint flingPoint = WebPoint(7, 13);
    WebPoint flingGlobalPoint = WebPoint(17, 23);
    int modifiers = 7;
    gesture.deltaX = flingDelta.x;
    gesture.deltaY = flingDelta.y;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);
    // The first animate call should let us pick up an animation start time, but we
    // shouldn't actually move anywhere just yet. The first frame after the fling start
    // will typically include the last scroll from the gesture that lead to the scroll
    // (either wheel or gesture scroll), so there should be no visible hitch.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::Property(&WebCore::IntSize::width, testing::Lt(0))));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd());
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Let's say on the third call we hit a non-scrollable region. We should abort the fling and not scroll.
    // We also should pass the current fling parameters out to the client so the rest of the fling can be
    // transferred to the main thread.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollOnMainThread));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd()).Times(0);

    // Expected wheel fling animation parameters:
    // *) flingDelta and flingPoint should match the original GestureFlingStart event
    // *) startTime should be 10 to match the time parameter of the first animate() call after the GestureFlingStart
    // *) cumulativeScroll depends on the curve, but since we've animated in the -X direction the X value should be < 0
    EXPECT_CALL(m_mockClient, transferActiveWheelFlingAnimation(testing::AllOf(
        testing::Field(&WebActiveWheelFlingParameters::delta, testing::Eq(flingDelta)),
        testing::Field(&WebActiveWheelFlingParameters::point, testing::Eq(flingPoint)),
        testing::Field(&WebActiveWheelFlingParameters::globalPoint, testing::Eq(flingGlobalPoint)),
        testing::Field(&WebActiveWheelFlingParameters::modifiers, testing::Eq(modifiers)),
        testing::Field(&WebActiveWheelFlingParameters::startTime, testing::Eq(10)),
        testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
            testing::Field(&WebSize::width, testing::Gt(0))))));
    m_inputHandler->animate(10.2);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);
    testing::Mock::VerifyAndClearExpectations(&m_mockClient);

    // Since we've aborted the fling, the next animation should be a no-op and should not result in another
    // frame being requested.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation()).Times(0);
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10.3);

    // Since we've transferred the fling to the main thread, we need to pass the next GestureFlingCancel to the main
    // thread as well.
    EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true));
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingTransferResets)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    // Start a gesture fling in the -X direction with zero Y movement.
    gesture.type = WebInputEvent::GestureFlingStart;
    WebFloatPoint flingDelta = WebFloatPoint(1000, 0);
    WebPoint flingPoint = WebPoint(7, 13);
    WebPoint flingGlobalPoint = WebPoint(17, 23);
    int modifiers = 1;
    gesture.deltaX = flingDelta.x;
    gesture.deltaY = flingDelta.y;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Start the fling animation at time 10. This shouldn't actually scroll, just establish a start time.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::Property(&WebCore::IntSize::width, testing::Lt(0))));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd());
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Let's say on the third call we hit a non-scrollable region. We should abort the fling and not scroll.
    // We also should pass the current fling parameters out to the client so the rest of the fling can be
    // transferred to the main thread.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollOnMainThread));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd()).Times(0);

    // Expected wheel fling animation parameters:
    // *) flingDelta and flingPoint should match the original GestureFlingStart event
    // *) startTime should be 10 to match the time parameter of the first animate() call after the GestureFlingStart
    // *) cumulativeScroll depends on the curve, but since we've animated in the -X direction the X value should be < 0
    EXPECT_CALL(m_mockClient, transferActiveWheelFlingAnimation(testing::AllOf(
        testing::Field(&WebActiveWheelFlingParameters::delta, testing::Eq(flingDelta)),
        testing::Field(&WebActiveWheelFlingParameters::point, testing::Eq(flingPoint)),
        testing::Field(&WebActiveWheelFlingParameters::globalPoint, testing::Eq(flingGlobalPoint)),
        testing::Field(&WebActiveWheelFlingParameters::modifiers, testing::Eq(modifiers)),
        testing::Field(&WebActiveWheelFlingParameters::startTime, testing::Eq(10)),
        testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
            testing::Field(&WebSize::width, testing::Gt(0))))));
    m_inputHandler->animate(10.2);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);
    testing::Mock::VerifyAndClearExpectations(&m_mockClient);

    // Since we've aborted the fling, the next animation should be a no-op and should not result in another
    // frame being requested.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation()).Times(0);
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10.3);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Since we've transferred the fling to the main thread, we need to pass the next GestureFlingCancel to the main
    // thread as well.
    EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true));
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Start a second gesture fling, this time in the +Y direction with no X.
    gesture.type = WebInputEvent::GestureFlingStart;
    flingDelta = WebFloatPoint(0, -1000);
    flingPoint = WebPoint(95, 87);
    flingGlobalPoint = WebPoint(32, 71);
    modifiers = 2;
    gesture.deltaX = flingDelta.x;
    gesture.deltaY = flingDelta.y;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Start the second fling animation at time 30.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(30);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Tick the second fling once normally.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollStarted));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::Property(&WebCore::IntSize::height, testing::Gt(0))));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd());
    m_inputHandler->animate(30.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockCCInputHandlerClient);

    // Then abort the second fling.
    EXPECT_CALL(m_mockCCInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebCore::CCInputHandlerClient::ScrollOnMainThread));
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollBy(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockCCInputHandlerClient, scrollEnd()).Times(0);

    // We should get parameters from the second fling, nothing from the first fling should "leak".
    EXPECT_CALL(m_mockClient, transferActiveWheelFlingAnimation(testing::AllOf(
        testing::Field(&WebActiveWheelFlingParameters::delta, testing::Eq(flingDelta)),
        testing::Field(&WebActiveWheelFlingParameters::point, testing::Eq(flingPoint)),
        testing::Field(&WebActiveWheelFlingParameters::globalPoint, testing::Eq(flingGlobalPoint)),
        testing::Field(&WebActiveWheelFlingParameters::modifiers, testing::Eq(modifiers)),
        testing::Field(&WebActiveWheelFlingParameters::startTime, testing::Eq(30)),
        testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
            testing::Field(&WebSize::height, testing::Lt(0))))));
    m_inputHandler->animate(30.2);
}

}
