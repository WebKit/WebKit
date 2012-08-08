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

#include "PlatformGestureCurveTarget.h"
#include "TouchpadFlingPlatformGestureCurve.h"
#include "TraceEvent.h"
#include "WebCompositorImpl.h"
#include "WebCompositorInputHandlerClient.h"
#include "WebInputEvent.h"
#include "cc/CCActiveGestureAnimation.h"
#include "cc/CCProxy.h"
#include <wtf/ThreadingPrimitives.h>

using namespace WebCore;

namespace WebCore {

PassOwnPtr<CCInputHandler> CCInputHandler::create(CCInputHandlerClient* inputHandlerClient)
{
    return WebKit::WebCompositorInputHandlerImpl::create(inputHandlerClient);
}

class PlatformGestureToCCGestureAdapter : public CCGestureCurve, public PlatformGestureCurveTarget {
public:
    static PassOwnPtr<CCGestureCurve> create(PassOwnPtr<PlatformGestureCurve> platformCurve)
    {
        return adoptPtr(new PlatformGestureToCCGestureAdapter(platformCurve));
    }

    virtual const char* debugName() const
    {
        return m_curve->debugName();
    }

    virtual bool apply(double time, CCGestureCurveTarget* target)
    {
        ASSERT(target);
        m_target = target;
        return m_curve->apply(time, this);
    }

    virtual void scrollBy(const IntPoint& scrollDelta)
    {
        ASSERT(m_target);
        m_target->scrollBy(scrollDelta);
    }

private:
    PlatformGestureToCCGestureAdapter(PassOwnPtr<PlatformGestureCurve> curve)
        : m_curve(curve)
        , m_target(0)
    {
    }

    OwnPtr<PlatformGestureCurve> m_curve;
    CCGestureCurveTarget* m_target;
};

}

namespace WebKit {

// These statics may only be accessed from the compositor thread.
int WebCompositorInputHandlerImpl::s_nextAvailableIdentifier = 1;
HashSet<WebCompositorInputHandlerImpl*>* WebCompositorInputHandlerImpl::s_compositors = 0;

WebCompositorInputHandler* WebCompositorInputHandler::fromIdentifier(int identifier)
{
    return WebCompositorInputHandlerImpl::fromIdentifier(identifier);
}

PassOwnPtr<WebCompositorInputHandlerImpl> WebCompositorInputHandlerImpl::create(WebCore::CCInputHandlerClient* inputHandlerClient)
{
    return adoptPtr(new WebCompositorInputHandlerImpl(inputHandlerClient));
}

WebCompositorInputHandler* WebCompositorInputHandlerImpl::fromIdentifier(int identifier)
{
    ASSERT(WebCompositorImpl::initialized());
    ASSERT(CCProxy::isImplThread());

    if (!s_compositors)
        return 0;

    for (HashSet<WebCompositorInputHandlerImpl*>::iterator it = s_compositors->begin(); it != s_compositors->end(); ++it) {
        if ((*it)->identifier() == identifier)
            return *it;
    }
    return 0;
}

WebCompositorInputHandlerImpl::WebCompositorInputHandlerImpl(CCInputHandlerClient* inputHandlerClient)
    : m_client(0)
    , m_identifier(s_nextAvailableIdentifier++)
    , m_inputHandlerClient(inputHandlerClient)
#ifndef NDEBUG
    , m_expectScrollUpdateEnd(false)
    , m_expectPinchUpdateEnd(false)
#endif
    , m_gestureScrollStarted(false)
{
    ASSERT(CCProxy::isImplThread());

    if (!s_compositors)
        s_compositors = new HashSet<WebCompositorInputHandlerImpl*>;
    s_compositors->add(this);
}

WebCompositorInputHandlerImpl::~WebCompositorInputHandlerImpl()
{
    ASSERT(CCProxy::isImplThread());
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
    ASSERT(CCProxy::isImplThread());
    // It's valid to set a new client if we've never had one or to clear the client, but it's not valid to change from having one client to a different one.
    ASSERT(!m_client || !client);
    m_client = client;
}

void WebCompositorInputHandlerImpl::handleInputEvent(const WebInputEvent& event)
{
    ASSERT(CCProxy::isImplThread());
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
}

WebCompositorInputHandlerImpl::EventDisposition WebCompositorInputHandlerImpl::handleInputEventInternal(const WebInputEvent& event)
{
    if (event.type == WebInputEvent::MouseWheel) {
        const WebMouseWheelEvent& wheelEvent = *static_cast<const WebMouseWheelEvent*>(&event);
        CCInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(IntPoint(wheelEvent.x, wheelEvent.y), CCInputHandlerClient::Wheel);
        switch (scrollStatus) {
        case CCInputHandlerClient::ScrollStarted: {
            TRACE_EVENT_INSTANT2("cc", "WebCompositorInputHandlerImpl::handleInput wheel scroll", "deltaX", -wheelEvent.deltaX, "deltaY", -wheelEvent.deltaY);
            m_inputHandlerClient->scrollBy(IntSize(-wheelEvent.deltaX, -wheelEvent.deltaY));
            m_inputHandlerClient->scrollEnd();
            return DidHandle;
        }
        case CCInputHandlerClient::ScrollIgnored:
            // FIXME: This should be DropEvent, but in cases where we fail to properly sync scrollability it's safer to send the
            // event to the main thread. Change back to DropEvent once we have synchronization bugs sorted out.
            return DidNotHandle; 
        case CCInputHandlerClient::ScrollOnMainThread:
            return DidNotHandle;
        }
    } else if (event.type == WebInputEvent::GestureScrollBegin) {
        ASSERT(!m_gestureScrollStarted);
        ASSERT(!m_expectScrollUpdateEnd);
#ifndef NDEBUG
        m_expectScrollUpdateEnd = true;
#endif
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        CCInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(IntPoint(gestureEvent.x, gestureEvent.y), CCInputHandlerClient::Gesture);
        switch (scrollStatus) {
        case CCInputHandlerClient::ScrollStarted:
            m_gestureScrollStarted = true;
            return DidHandle;
        case CCInputHandlerClient::ScrollOnMainThread:
            return DidNotHandle;
        case CCInputHandlerClient::ScrollIgnored:
            return DropEvent;
        }
    } else if (event.type == WebInputEvent::GestureScrollUpdate) {
        ASSERT(m_expectScrollUpdateEnd);

        if (!m_gestureScrollStarted)
            return DidNotHandle;

        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        m_inputHandlerClient->scrollBy(IntSize(-gestureEvent.deltaX, -gestureEvent.deltaY));
        return DidHandle;
    } else if (event.type == WebInputEvent::GestureScrollEnd) {
        ASSERT(m_expectScrollUpdateEnd);
#ifndef NDEBUG
        m_expectScrollUpdateEnd = false;
#endif
        if (!m_gestureScrollStarted)
            return DidNotHandle;

        m_inputHandlerClient->scrollEnd();
        m_gestureScrollStarted = false;
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchBegin) {
        ASSERT(!m_expectPinchUpdateEnd);
#ifndef NDEBUG
        m_expectPinchUpdateEnd = true;
#endif
        m_inputHandlerClient->pinchGestureBegin();
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchEnd) {
        ASSERT(m_expectPinchUpdateEnd);
#ifndef NDEBUG
        m_expectPinchUpdateEnd = false;
#endif
        m_inputHandlerClient->pinchGestureEnd();
        return DidHandle;
    } else if (event.type == WebInputEvent::GesturePinchUpdate) {
        ASSERT(m_expectPinchUpdateEnd);
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        m_inputHandlerClient->pinchGestureUpdate(gestureEvent.deltaX, IntPoint(gestureEvent.x, gestureEvent.y));
        return DidHandle;
    } else if (event.type == WebInputEvent::GestureFlingStart) {
        const WebGestureEvent& gestureEvent = *static_cast<const WebGestureEvent*>(&event);
        return handleGestureFling(gestureEvent);
    } else if (event.type == WebInputEvent::GestureFlingCancel) {
        if (cancelCurrentFling())
            return DidHandle;
    } else if (WebInputEvent::isKeyboardEventType(event.type)) {
         cancelCurrentFling();
    }

    return DidNotHandle;
}

WebCompositorInputHandlerImpl::EventDisposition WebCompositorInputHandlerImpl::handleGestureFling(const WebGestureEvent& gestureEvent)
{
    CCInputHandlerClient::ScrollStatus scrollStatus = m_inputHandlerClient->scrollBegin(IntPoint(gestureEvent.x, gestureEvent.y), CCInputHandlerClient::Wheel);
    switch (scrollStatus) {
    case CCInputHandlerClient::ScrollStarted: {
        TRACE_EVENT_INSTANT0("cc", "WebCompositorInputHandlerImpl::handleGestureFling::started");
        OwnPtr<PlatformGestureCurve> flingCurve = TouchpadFlingPlatformGestureCurve::create(FloatPoint(gestureEvent.deltaX, gestureEvent.deltaY));
        m_wheelFlingAnimation = CCActiveGestureAnimation::create(PlatformGestureToCCGestureAdapter::create(flingCurve.release()), this);
        m_wheelFlingParameters.delta = WebFloatPoint(gestureEvent.deltaX, gestureEvent.deltaY);
        m_wheelFlingParameters.point = WebPoint(gestureEvent.x, gestureEvent.y);
        m_wheelFlingParameters.globalPoint = WebPoint(gestureEvent.globalX, gestureEvent.globalY);
        m_wheelFlingParameters.modifiers = gestureEvent.modifiers;
        m_inputHandlerClient->scheduleAnimation();
        return DidHandle;
    }
    case CCInputHandlerClient::ScrollOnMainThread: {
        TRACE_EVENT_INSTANT0("cc", "WebCompositorInputHandlerImpl::handleGestureFling::scrollOnMainThread");
        return DidNotHandle;
    }
    case CCInputHandlerClient::ScrollIgnored: {
        TRACE_EVENT_INSTANT0("cc", "WebCompositorInputHandlerImpl::handleGestureFling::ignored");
        // We still pass the curve to the main thread if there's nothing scrollable, in case something
        // registers a handler before the curve is over.
        return DidNotHandle;
    }
    }
    return DidNotHandle;
}

int WebCompositorInputHandlerImpl::identifier() const
{
    ASSERT(CCProxy::isImplThread());
    return m_identifier;
}

void WebCompositorInputHandlerImpl::animate(double monotonicTime)
{
    if (!m_wheelFlingAnimation)
        return;

    if (!m_wheelFlingParameters.startTime)
        m_wheelFlingParameters.startTime = monotonicTime;

    if (m_wheelFlingAnimation->animate(monotonicTime))
        m_inputHandlerClient->scheduleAnimation();
    else {
        TRACE_EVENT_INSTANT0("cc", "WebCompositorInputHandlerImpl::animate::flingOver");
        cancelCurrentFling();
    }
}

bool WebCompositorInputHandlerImpl::cancelCurrentFling()
{
    bool hadFlingAnimation = m_wheelFlingAnimation;
    TRACE_EVENT_INSTANT1("cc", "WebCompositorInputHandlerImpl::cancelCurrentFling", "hadFlingAnimation", hadFlingAnimation);
    m_wheelFlingAnimation.clear();
    m_wheelFlingParameters = WebActiveWheelFlingParameters();
    return hadFlingAnimation;
}

void WebCompositorInputHandlerImpl::scrollBy(const IntPoint& increment)
{
    if (increment == IntPoint::zero())
        return;

    TRACE_EVENT2("cc", "WebCompositorInputHandlerImpl::scrollBy", "x", increment.x(), "y", increment.y());
    WebMouseWheelEvent syntheticWheel;
    syntheticWheel.type = WebInputEvent::MouseWheel;
    syntheticWheel.deltaX = increment.x();
    syntheticWheel.deltaY = increment.y();
    syntheticWheel.hasPreciseScrollingDeltas = true;
    syntheticWheel.x = m_wheelFlingParameters.point.x;
    syntheticWheel.y = m_wheelFlingParameters.point.y;
    syntheticWheel.globalX = m_wheelFlingParameters.globalPoint.x;
    syntheticWheel.globalY = m_wheelFlingParameters.globalPoint.y;
    syntheticWheel.modifiers = m_wheelFlingParameters.modifiers;

    WebCompositorInputHandlerImpl::EventDisposition disposition = handleInputEventInternal(syntheticWheel);
    switch (disposition) {
    case DidHandle:
        m_wheelFlingParameters.cumulativeScroll.width += increment.x();
        m_wheelFlingParameters.cumulativeScroll.height += increment.y();
    case DropEvent:
        break;
    case DidNotHandle:
        TRACE_EVENT_INSTANT0("cc", "WebCompositorInputHandlerImpl::scrollBy::AbortFling");
        // If we got a DidNotHandle, that means we need to deliver wheels on the main thread.
        // In this case we need to schedule a commit and transfer the fling curve over to the main
        // thread and run the rest of the wheels from there.
        // This can happen when flinging a page that contains a scrollable subarea that we can't
        // scroll on the thread if the fling starts outside the subarea but then is flung "under" the
        // pointer.
        m_client->transferActiveWheelFlingAnimation(m_wheelFlingParameters);
        cancelCurrentFling();
        break;
    }
}

}
