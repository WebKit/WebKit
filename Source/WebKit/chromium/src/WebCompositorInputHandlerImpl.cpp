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

#include "TraceEvent.h"
#include "WebCompositorInputHandlerClient.h"
#include "WebInputEvent.h"
#include <public/Platform.h>
#include <public/WebInputHandlerClient.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

using namespace WebCore;

namespace WebKit {

// These statics may only be accessed from the compositor thread.
int WebCompositorInputHandlerImpl::s_nextAvailableIdentifier = 1;
HashSet<WebCompositorInputHandlerImpl*>* WebCompositorInputHandlerImpl::s_compositors = 0;

WebCompositorInputHandler* WebCompositorInputHandler::fromIdentifier(int identifier)
{
    return WebCompositorInputHandlerImpl::fromIdentifier(identifier);
}

WebCompositorInputHandler* WebCompositorInputHandlerImpl::fromIdentifier(int identifier)
{

    if (!s_compositors)
        return 0;

    for (HashSet<WebCompositorInputHandlerImpl*>::iterator it = s_compositors->begin(); it != s_compositors->end(); ++it) {
        if ((*it)->identifier() == identifier)
            return *it;
    }
    return 0;
}

WebCompositorInputHandlerImpl::WebCompositorInputHandlerImpl()
    : m_client(0)
    , m_identifier(s_nextAvailableIdentifier++)
    , m_inputHandlerClient(0)
#ifndef NDEBUG
    , m_expectScrollUpdateEnd(false)
    , m_expectPinchUpdateEnd(false)
#endif
    , m_gestureScrollOnImplThread(false)
    , m_gesturePinchOnImplThread(false)
    , m_flingActiveOnMainThread(false)
{
}

WebCompositorInputHandlerImpl::~WebCompositorInputHandlerImpl()
{
    if (m_client)
        m_client->willShutdown();

    ASSERT(s_compositors);
    s_compositors->remove(this);
    if (!s_compositors->size()) {
        delete s_compositors;
        s_compositors = 0;
    }
}

void WebCompositorInputHandlerImpl::setClient(WebCompositorInputHandlerClient* client)
{
    // It's valid to set a new client if we've never had one or to clear the client, but it's not valid to change from having one client to a different one.
    ASSERT(!m_client || !client);
    m_client = client;
}

void WebCompositorInputHandlerImpl::handleInputEvent(const WebInputEvent& event)
{
    ASSERT(m_client);

    WebCompositorInputHandlerImpl::EventDisposition disposition = handleInputEventInternal(event);
    switch (disposition) {
    case DidHandle:
        m_client->didHandleInputEvent();
        break;
    case DidNotHandle:
        m_client->didNotHandleInputEvent(true /* sendToWidget */);
        break;
    case DropEvent:
        m_client->didNotHandleInputEvent(false /* sendToWidget */);
        break;
    }
    if (event.modifiers & WebInputEvent::IsLastInputEventForCurrentVSync)
        m_inputHandlerClient->didReceiveLastInputEventForVSync();
}

WebCompositorInputHandlerImpl::EventDisposition WebCompositorInputHandlerImpl::handleInputEventInternal(const WebInputEvent& event)
{
    if (event.type == WebInputEvent::MouseWheel) {
        const WebMouseWheelEvent& wheelEvent = *static_cast<const WebMouseWheelEvent*>(&event);
        WebInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(WebPoint(wheelEvent.x, wheelEvent.y), WebInputHandlerClient::ScrollInputTypeWheel);
        switch (scrollStatus) {
        case WebInputHandlerClient::ScrollStatusStarted: {
            TRACE_EVENT_INSTANT2("webkit", "WebCompositorInputHandlerImpl::handleInput wheel scroll", "deltaX", -wheelEvent.deltaX, "deltaY", -wheelEvent.deltaY);
            bool didScroll = m_inputHandlerClient->scrollByIfPossible(WebPoint(wheelEvent.x, wheelEvent.y), IntSize(-wheelEvent.deltaX, -wheelEvent.deltaY));
            m_inputHandlerClient->scrollEnd();
            return didScroll ? DidHandle : DropEvent;
        }
        case WebInputHandlerClient::ScrollStatusIgnored:
            // FIXME: This should be DropEvent, but in cases where we fail to properly sync scrollability it's safer to send the
            // event to the main thread. Change back to DropEvent once we have synchronization bugs sorted out.
            return DidNotHandle; 
        case WebInputHandlerClient::ScrollStatusOnMainThread:
            return DidNotHandle;
        }
    } else if (event.type == WebInputEvent::GestureScrollBegin) {
        ASSERT(!m_gestureScrollOnImplThread);
        ASSERT(!m_expectScrollUpdateEnd);
#ifndef NDEBUG
        m_expectScrollUpdateEnd = true;
#endif
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        WebInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(WebPoint(gestureEvent.x, gestureEvent.y), WebInputHandlerClient::ScrollInputTypeGesture);
        switch (scrollStatus) {
        case WebInputHandlerClient::ScrollStatusStarted:
            m_gestureScrollOnImplThread = true;
            return DidHandle;
        case WebInputHandlerClient::ScrollStatusOnMainThread:
            return DidNotHandle;
        case WebInputHandlerClient::ScrollStatusIgnored:
            return DropEvent;
        }
    } else if (event.type == WebInputEvent::GestureScrollUpdate) {
        ASSERT(m_expectScrollUpdateEnd);

        if (!m_gestureScrollOnImplThread && !m_gesturePinchOnImplThread)
            return DidNotHandle;

        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        bool didScroll = m_inputHandlerClient->scrollByIfPossible(WebPoint(gestureEvent.x, gestureEvent.y),
            IntSize(-gestureEvent.data.scrollUpdate.deltaX, -gestureEvent.data.scrollUpdate.deltaY));
        return didScroll ? DidHandle : DropEvent;
    } else if (event.type == WebInputEvent::GestureScrollEnd) {
        ASSERT(m_expectScrollUpdateEnd);
#ifndef NDEBUG
        m_expectScrollUpdateEnd = false;
#endif
        if (!m_gestureScrollOnImplThread)
            return DidNotHandle;

        m_inputHandlerClient->scrollEnd();
        m_gestureScrollOnImplThread = false;
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchBegin) {
        ASSERT(!m_expectPinchUpdateEnd);
#ifndef NDEBUG
        m_expectPinchUpdateEnd = true;
#endif
        m_inputHandlerClient->pinchGestureBegin();
        m_gesturePinchOnImplThread = true;
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchEnd) {
        ASSERT(m_expectPinchUpdateEnd);
#ifndef NDEBUG
        m_expectPinchUpdateEnd = false;
#endif
        m_gesturePinchOnImplThread = false;
        m_inputHandlerClient->pinchGestureEnd();
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchUpdate) {
        ASSERT(m_expectPinchUpdateEnd);
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        m_inputHandlerClient->pinchGestureUpdate(gestureEvent.data.pinchUpdate.scale, WebPoint(gestureEvent.x, gestureEvent.y));
        return DidHandle;
    } else if (event.type == WebInputEvent::GestureFlingStart) {
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        return handleGestureFling(gestureEvent);
    } else if (event.type == WebInputEvent::GestureFlingCancel) {
        if (cancelCurrentFling())
            return DidHandle;
        else if (!m_flingActiveOnMainThread)
            return DropEvent;
#if ENABLE(TOUCH_EVENT_TRACKING)
    } else if (event.type == WebInputEvent::TouchStart) {
        const WebTouchEvent& touchEvent = *static_cast<const WebTouchEvent*>(&event);
        if (!m_inputHandlerClient->haveTouchEventHandlersAt(touchEvent.touches[0].position))
            return DropEvent;
#endif
    } else if (WebInputEvent::isKeyboardEventType(event.type)) {
         cancelCurrentFling();
    }

    return DidNotHandle;
}

WebCompositorInputHandlerImpl::EventDisposition WebCompositorInputHandlerImpl::handleGestureFling(const WebGestureEvent& gestureEvent)
{
    WebInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(WebPoint(gestureEvent.x, gestureEvent.y), WebInputHandlerClient::ScrollInputTypeGesture);
    switch (scrollStatus) {
    case WebInputHandlerClient::ScrollStatusStarted: {
        if (gestureEvent.data.flingStart.sourceDevice == WebGestureEvent::Touchpad)
            m_inputHandlerClient->scrollEnd();
        m_flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(gestureEvent.data.flingStart.sourceDevice, WebFloatPoint(gestureEvent.data.flingStart.velocityX, gestureEvent.data.flingStart.velocityY), WebSize()));
        TRACE_EVENT_ASYNC_BEGIN0("webkit", "WebCompositorInputHandlerImpl::handleGestureFling::started", this);
        m_flingParameters.delta = WebFloatPoint(gestureEvent.data.flingStart.velocityX, gestureEvent.data.flingStart.velocityY);
        m_flingParameters.point = WebPoint(gestureEvent.x, gestureEvent.y);
        m_flingParameters.globalPoint = WebPoint(gestureEvent.globalX, gestureEvent.globalY);
        m_flingParameters.modifiers = gestureEvent.modifiers;
        m_flingParameters.sourceDevice = gestureEvent.data.flingStart.sourceDevice;
        m_inputHandlerClient->scheduleAnimation();
        return DidHandle;
    }
    case WebInputHandlerClient::ScrollStatusOnMainThread: {
        TRACE_EVENT_INSTANT0("webkit", "WebCompositorInputHandlerImpl::handleGestureFling::scrollOnMainThread");
        m_flingActiveOnMainThread =  true;
        return DidNotHandle;
    }
    case WebInputHandlerClient::ScrollStatusIgnored: {
        TRACE_EVENT_INSTANT0("webkit", "WebCompositorInputHandlerImpl::handleGestureFling::ignored");
        if (gestureEvent.data.flingStart.sourceDevice == WebGestureEvent::Touchpad) {
            // We still pass the curve to the main thread if there's nothing scrollable, in case something
            // registers a handler before the curve is over.
            return DidNotHandle;
        }
        return DropEvent;
    }
    }
    return DidNotHandle;
}

void WebCompositorInputHandlerImpl::bindToClient(WebInputHandlerClient* client)
{
    ASSERT(!m_inputHandlerClient);

    TRACE_EVENT_INSTANT0("webkit", "WebCompositorInputHandlerImpl::bindToClient");
    if (!s_compositors)
        s_compositors = new HashSet<WebCompositorInputHandlerImpl*>;
    s_compositors->add(this);

    m_inputHandlerClient = client;
}

void WebCompositorInputHandlerImpl::animate(double monotonicTime)
{
    if (!m_flingCurve)
        return;

    if (!m_flingParameters.startTime) {
        m_flingParameters.startTime = monotonicTime;
        m_inputHandlerClient->scheduleAnimation();
        return;
    }

    if (m_flingCurve->apply(monotonicTime - m_flingParameters.startTime, this))
        m_inputHandlerClient->scheduleAnimation();
    else {
        TRACE_EVENT_INSTANT0("webkit", "WebCompositorInputHandlerImpl::animate::flingOver");
        cancelCurrentFling();
    }
}

bool WebCompositorInputHandlerImpl::cancelCurrentFling()
{
    bool hadFlingAnimation = m_flingCurve;
    if (hadFlingAnimation && m_flingParameters.sourceDevice == WebGestureEvent::Touchscreen) {
        m_inputHandlerClient->scrollEnd();
        TRACE_EVENT_ASYNC_END0("webkit", "WebCompositorInputHandlerImpl::handleGestureFling::started", this);
    }

    TRACE_EVENT_INSTANT1("webkit", "WebCompositorInputHandlerImpl::cancelCurrentFling", "hadFlingAnimation", hadFlingAnimation);
    m_flingCurve.clear();
    m_flingParameters = WebActiveWheelFlingParameters();
    return hadFlingAnimation;
}

bool WebCompositorInputHandlerImpl::touchpadFlingScroll(const WebPoint& increment)
{
    WebMouseWheelEvent syntheticWheel;
    syntheticWheel.type = WebInputEvent::MouseWheel;
    syntheticWheel.deltaX = increment.x;
    syntheticWheel.deltaY = increment.y;
    syntheticWheel.hasPreciseScrollingDeltas = true;
    syntheticWheel.x = m_flingParameters.point.x;
    syntheticWheel.y = m_flingParameters.point.y;
    syntheticWheel.globalX = m_flingParameters.globalPoint.x;
    syntheticWheel.globalY = m_flingParameters.globalPoint.y;
    syntheticWheel.modifiers = m_flingParameters.modifiers;

    WebCompositorInputHandlerImpl::EventDisposition disposition = handleInputEventInternal(syntheticWheel);
    switch (disposition) {
    case DidHandle:
        return true;
    case DropEvent:
        break;
    case DidNotHandle:
        TRACE_EVENT_INSTANT0("webkit", "WebCompositorInputHandlerImpl::scrollBy::AbortFling");
        // If we got a DidNotHandle, that means we need to deliver wheels on the main thread.
        // In this case we need to schedule a commit and transfer the fling curve over to the main
        // thread and run the rest of the wheels from there.
        // This can happen when flinging a page that contains a scrollable subarea that we can't
        // scroll on the thread if the fling starts outside the subarea but then is flung "under" the
        // pointer.
        m_client->transferActiveWheelFlingAnimation(m_flingParameters);
        cancelCurrentFling();
        break;
    }

    return false;
}

void WebCompositorInputHandlerImpl::scrollBy(const WebPoint& increment)
{
    if (increment == WebPoint())
        return;

    TRACE_EVENT2("webkit", "WebCompositorInputHandlerImpl::scrollBy", "x", increment.x, "y", increment.y);

    bool didScroll = false;

    switch (m_flingParameters.sourceDevice) {
    case WebGestureEvent::Touchpad:
        didScroll = touchpadFlingScroll(increment);
        break;
    case WebGestureEvent::Touchscreen:
        didScroll = m_inputHandlerClient->scrollByIfPossible(m_flingParameters.point, IntSize(-increment.x, -increment.y));
        break;
    }

    if (didScroll) {
        m_flingParameters.cumulativeScroll.width += increment.x;
        m_flingParameters.cumulativeScroll.height += increment.y;
    }
}

void WebCompositorInputHandlerImpl::mainThreadHasStoppedFlinging()
{
    m_flingActiveOnMainThread =  false;
}

}
