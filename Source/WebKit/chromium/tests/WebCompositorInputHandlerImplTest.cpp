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

#include "WebCompositorInitializer.h"
#include "WebCompositorInputHandlerClient.h"
#include "WebInputEvent.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatSize.h>
#include <public/WebInputHandler.h>
#include <public/WebInputHandlerClient.h>
#include <public/WebPoint.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

class MockWebInputHandlerClient : public WebInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(MockWebInputHandlerClient);
public:
    MockWebInputHandlerClient()
    {
    }
    virtual ~MockWebInputHandlerClient() { }


    MOCK_METHOD0(pinchGestureBegin, void());
    MOCK_METHOD2(pinchGestureUpdate, void(float magnifyDelta, WebPoint anchor));
    MOCK_METHOD0(pinchGestureEnd, void());

    MOCK_METHOD0(scheduleAnimation, void());

    MOCK_METHOD1(haveTouchEventHandlersAt, bool(WebPoint));

    MOCK_METHOD2(scrollBegin, ScrollStatus(WebPoint, WebInputHandlerClient::ScrollInputType));
    MOCK_METHOD2(scrollByIfPossible, bool(WebPoint, WebFloatSize));
    MOCK_METHOD2(scrollVerticallyByPageIfPossible, bool(WebPoint, WebScrollbar::ScrollDirection));
    MOCK_METHOD0(scrollEnd, void());

    MOCK_METHOD0(didReceiveLastInputEventForVSync, void());

private:
    virtual void startPageScaleAnimation(WebSize targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTimeMs,
                                         double durationMs) OVERRIDE { }
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

class WebCompositorInputHandlerImplTest : public testing::Test {
public:
    WebCompositorInputHandlerImplTest()
        : m_initializer(0)
        , m_expectedDisposition(DidHandle)
    {
        m_inputHandler = adoptPtr(new WebCompositorInputHandlerImpl);
        m_inputHandler->bindToClient(&m_mockInputHandlerClient);
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
        testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);                                          \
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
    testing::StrictMock<MockWebInputHandlerClient> m_mockInputHandlerClient;
    OwnPtr<WebCompositorInputHandlerImpl> m_inputHandler;
    testing::StrictMock<MockWebCompositorInputHandlerClient> m_mockClient;
    WebGestureEvent gesture;
    WebKitTests::WebCompositorInitializer m_initializer;

    enum ExpectedDisposition { DidHandle, DidNotHandle, DropEvent };
    ExpectedDisposition m_expectedDisposition;
};


TEST_F(WebCompositorInputHandlerImplTest, gestureScrollStarted)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    // The event should not be marked as handled if scrolling is not possible.
    m_expectedDisposition = DropEvent;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = -40; // -Y means scroll down - i.e. in the +Y direction.
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::height, testing::Gt(0))))
        .WillOnce(testing::Return(false));
    m_inputHandler->handleInputEvent(gesture);

    // Mark the event as handled if scroll happens.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = -40; // -Y means scroll down - i.e. in the +Y direction.
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::height, testing::Gt(0))))
        .WillOnce(testing::Return(true));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.data.scrollUpdate.deltaY = 0;
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureScrollOnMainThread)
{
    // We should send all events to the widget for this gesture.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(::testing::_, ::testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = 40;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.data.scrollUpdate.deltaY = 0;
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

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusIgnored));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gesturePinch)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchBegin;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureBegin());
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.data.pinchUpdate.scale = 1.5;
    gesture.x = 7;
    gesture.y = 13;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureUpdate(1.5, WebPoint(7, 13)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.data.pinchUpdate.scale = 0.5;
    gesture.x = 9;
    gesture.y = 6;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureUpdate(.5, WebPoint(9, 6)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchEnd;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureEnd());
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gesturePinchAfterScrollOnMainThread)
{
    // Scrolls will start by being sent to the main thread.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(::testing::_, ::testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = 40;
    m_inputHandler->handleInputEvent(gesture);

    // However, after the pinch gesture starts, they should go to the impl
    // thread.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchBegin;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureBegin());
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.data.pinchUpdate.scale = 1.5;
    gesture.x = 7;
    gesture.y = 13;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureUpdate(1.5, WebPoint(7, 13)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollUpdate;
    gesture.data.scrollUpdate.deltaY = -40; // -Y means scroll down - i.e. in the +Y direction.
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::height, testing::Gt(0))))
        .WillOnce(testing::Return(true));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchUpdate;
    gesture.data.pinchUpdate.scale = 0.5;
    gesture.x = 9;
    gesture.y = 6;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureUpdate(.5, WebPoint(9, 6)));
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GesturePinchEnd;
    EXPECT_CALL(m_mockInputHandlerClient, pinchGestureEnd());
    m_inputHandler->handleInputEvent(gesture);

    // After the pinch gesture ends, they should go to back to the main
    // thread.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureScrollEnd;
    gesture.data.scrollUpdate.deltaY = 0;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingStartedTouchpad)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.data.flingStart.velocityX = 10;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Verify that a GestureFlingCancel during an animation cancels it.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingOnMainThreadTouchpad)
{
    // We should send all events to the widget for this gesture.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Even if we didn't start a fling ourselves, we still need to send the cancel event to the widget.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingIgnoredTouchpad)
{
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusIgnored));

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);

    m_expectedDisposition = DropEvent;
    VERIFY_AND_RESET_MOCKS();

    // Since the previous fling was ignored, we should also be dropping the next flingCancel.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingAnimatesTouchpad)
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
    gesture.data.flingStart.velocityX = flingDelta.x;
    gesture.data.flingStart.velocityY = flingDelta.y;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);
    // The first animate call should let us pick up an animation start time, but we
    // shouldn't actually move anywhere just yet. The first frame after the fling start
    // will typically include the last scroll from the gesture that lead to the scroll
    // (either wheel or gesture scroll), so there should be no visible hitch.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::width, testing::Lt(0))))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Let's say on the third call we hit a non-scrollable region. We should abort the fling and not scroll.
    // We also should pass the current fling parameters out to the client so the rest of the fling can be
    // transferred to the main thread.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd()).Times(0);
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

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);
    testing::Mock::VerifyAndClearExpectations(&m_mockClient);

    // Since we've aborted the fling, the next animation should be a no-op and should not result in another
    // frame being requested.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation()).Times(0);
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10.3);

    // Since we've transferred the fling to the main thread, we need to pass the next GestureFlingCancel to the main
    // thread as well.
    EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true));
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingTransferResetsTouchpad)
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
    gesture.data.flingStart.velocityX = flingDelta.x;
    gesture.data.flingStart.velocityY = flingDelta.y;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Start the fling animation at time 10. This shouldn't actually scroll, just establish a start time.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::width, testing::Lt(0))))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Let's say on the third call we hit a non-scrollable region. We should abort the fling and not scroll.
    // We also should pass the current fling parameters out to the client so the rest of the fling can be
    // transferred to the main thread.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd()).Times(0);

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

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);
    testing::Mock::VerifyAndClearExpectations(&m_mockClient);

    // Since we've aborted the fling, the next animation should be a no-op and should not result in another
    // frame being requested.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation()).Times(0);
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(10.3);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Since we've transferred the fling to the main thread, we need to pass the next GestureFlingCancel to the main
    // thread as well.
    EXPECT_CALL(m_mockClient, didNotHandleInputEvent(true));
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();
    m_inputHandler->mainThreadHasStoppedFlinging();

    // Start a second gesture fling, this time in the +Y direction with no X.
    gesture.type = WebInputEvent::GestureFlingStart;
    flingDelta = WebFloatPoint(0, -1000);
    flingPoint = WebPoint(95, 87);
    flingGlobalPoint = WebPoint(32, 71);
    modifiers = 2;
    gesture.data.flingStart.velocityX = flingDelta.x;
    gesture.data.flingStart.velocityY = flingDelta.y;
    gesture.sourceDevice = WebGestureEvent::Touchpad;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Start the second fling animation at time 30.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_)).Times(0);
    m_inputHandler->animate(30);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Tick the second fling once normally.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::height, testing::Gt(0))))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    m_inputHandler->animate(30.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // Then abort the second fling.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::_)).Times(0);
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd()).Times(0);

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

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingStartedTouchscreen)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.data.flingStart.velocityX = 10;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());

    // Verify that a GestureFlingCancel during an animation cancels it.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingOnMainThreadTouchscreen)
{
    // We should send all events to the widget for this gesture.
    m_expectedDisposition = DidNotHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusOnMainThread));

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Even if we didn't start a fling ourselves, we still need to send the cancel event to the widget.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingIgnoredTouchscreen)
{
    m_expectedDisposition = DropEvent;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusIgnored));

    gesture.type = WebInputEvent::GestureFlingStart;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);

    VERIFY_AND_RESET_MOCKS();

    // Even if we didn't start a fling ourselves, we still need to send the cancel event to the widget.
    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureFlingAnimatesTouchscreen)
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
    gesture.data.flingStart.velocityX = flingDelta.x;
    gesture.data.flingStart.velocityY = flingDelta.y;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    m_inputHandler->handleInputEvent(gesture);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);
    // The first animate call should let us pick up an animation start time, but we
    // shouldn't actually move anywhere just yet. The first frame after the fling start
    // will typically include the last scroll from the gesture that lead to the scroll
    // (either wheel or gesture scroll), so there should be no visible hitch.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::width, testing::Lt(0))))
        .WillOnce(testing::Return(true));
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    EXPECT_CALL(m_mockClient, didHandleInputEvent());
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);
}

TEST_F(WebCompositorInputHandlerImplTest, gestureScrollOnImplThreadFlagClearedAfterFling)
{
    // We shouldn't send any events to the widget for this gesture.
    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));

    gesture.type = WebInputEvent::GestureScrollBegin;
    m_inputHandler->handleInputEvent(gesture);

    // After sending a GestureScrollBegin, the member variable
    // |m_gestureScrollOnImplThread| should be true.
    ASSERT(m_inputHandler->isGestureScrollOnImplThread());

    m_expectedDisposition = DidHandle;
    VERIFY_AND_RESET_MOCKS();

    // On the fling start, we should schedule an animation but not actually start
    // scrolling.
    gesture.type = WebInputEvent::GestureFlingStart;
    WebFloatPoint flingDelta = WebFloatPoint(1000, 0);
    WebPoint flingPoint = WebPoint(7, 13);
    WebPoint flingGlobalPoint = WebPoint(17, 23);
    int modifiers = 7;
    gesture.data.flingStart.velocityX = flingDelta.x;
    gesture.data.flingStart.velocityY = flingDelta.y;
    gesture.sourceDevice = WebGestureEvent::Touchscreen;
    gesture.x = flingPoint.x;
    gesture.y = flingPoint.y;
    gesture.globalX = flingGlobalPoint.x;
    gesture.globalY = flingGlobalPoint.y;
    gesture.modifiers = modifiers;
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(WebInputHandlerClient::ScrollStatusStarted));
    m_inputHandler->handleInputEvent(gesture);

    // |m_gestureScrollOnImplThread| should still be true after
    // a GestureFlingStart is sent.
    ASSERT(m_inputHandler->isGestureScrollOnImplThread());

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);
    // The first animate call should let us pick up an animation start time, but we
    // shouldn't actually move anywhere just yet. The first frame after the fling start
    // will typically include the last scroll from the gesture that lead to the scroll
    // (either wheel or gesture scroll), so there should be no visible hitch.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    m_inputHandler->animate(10);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    // The second call should start scrolling in the -X direction.
    EXPECT_CALL(m_mockInputHandlerClient, scheduleAnimation());
    EXPECT_CALL(m_mockInputHandlerClient, scrollByIfPossible(testing::_, testing::Field(&WebFloatSize::width, testing::Lt(0))))
        .WillOnce(testing::Return(true));
    m_inputHandler->animate(10.1);

    testing::Mock::VerifyAndClearExpectations(&m_mockInputHandlerClient);

    EXPECT_CALL(m_mockClient, didHandleInputEvent());
    EXPECT_CALL(m_mockInputHandlerClient, scrollEnd());
    gesture.type = WebInputEvent::GestureFlingCancel;
    m_inputHandler->handleInputEvent(gesture);

    // |m_gestureScrollOnImplThread| should be false once
    // the fling has finished (note no GestureScrollEnd has been sent).
    ASSERT(!m_inputHandler->isGestureScrollOnImplThread());
}

TEST_F(WebCompositorInputHandlerImplTest, lastInputEventForVSync)
{
    m_expectedDisposition = DropEvent;
    VERIFY_AND_RESET_MOCKS();

    gesture.type = WebInputEvent::GestureFlingCancel;
    gesture.modifiers |= WebInputEvent::IsLastInputEventForCurrentVSync;
    EXPECT_CALL(m_mockInputHandlerClient, didReceiveLastInputEventForVSync());
    m_inputHandler->handleInputEvent(gesture);
}

}
