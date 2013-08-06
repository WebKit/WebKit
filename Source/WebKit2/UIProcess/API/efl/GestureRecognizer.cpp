/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "GestureRecognizer.h"

#include "EwkView.h"
#include "NotImplemented.h"
#include "WKSharedAPICast.h"

#if ENABLE(TOUCH_EVENTS)

using namespace WebCore;

namespace WebKit {

class GestureHandler {
public:
    static PassOwnPtr<GestureHandler> create(EwkView* ewkView)
    {
        return adoptPtr(new GestureHandler(ewkView));
    }

    void handleSingleTap(const WebCore::IntPoint&);
    void handleDoubleTap(const WebCore::IntPoint&);
    void handleTapAndHold(const WebCore::IntPoint&);
    void handlePanStarted(const WebCore::IntPoint&);
    void handlePan(const WebCore::IntPoint&);
    void handlePanFinished();
    void handlePinchStarted(const Vector<WebCore::IntPoint>&);
    void handlePinch(const Vector<WebCore::IntPoint>&);
    void handlePinchFinished();

private:
    explicit GestureHandler(EwkView*);

    EwkView* m_ewkView;
};

GestureHandler::GestureHandler(EwkView* ewkView)
    : m_ewkView(ewkView)
{
}

void GestureHandler::handleSingleTap(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handleDoubleTap(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handleTapAndHold(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handlePanStarted(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handlePan(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handlePanFinished()
{
    notImplemented();
}

void GestureHandler::handlePinchStarted(const Vector<IntPoint>&)
{
    notImplemented();
}

void GestureHandler::handlePinch(const Vector<IntPoint>&)
{
    notImplemented();
}

void GestureHandler::handlePinchFinished()
{
    notImplemented();
}

const double GestureRecognizer::s_doubleTapTimeoutInSeconds = 0.4;
const double GestureRecognizer::s_tapAndHoldTimeoutInSeconds = 1.0;
const int GestureRecognizer::s_squaredDoubleTapThreshold = 10000;
const int GestureRecognizer::s_squaredPanThreshold = 100;

GestureRecognizer::GestureRecognizer(EwkView* ewkView)
    : m_recognizerFunction(&GestureRecognizer::noGesture)
    , m_gestureHandler(GestureHandler::create(ewkView))
    , m_tapAndHoldTimer(0)
    , m_doubleTapTimer(0)
{
}

GestureRecognizer::~GestureRecognizer()
{
}

void GestureRecognizer::processTouchEvent(WKTouchEventRef eventRef)
{
    WKEventType type = WKTouchEventGetType(eventRef);
    if (type == kWKEventTypeTouchCancel) {
        reset();
        return;
    }

    (this->*m_recognizerFunction)(type, WKTouchEventGetTouchPoints(eventRef));
}

Eina_Bool GestureRecognizer::doubleTapTimerCallback(void* data)
{
    GestureRecognizer* gestureRecognizer = static_cast<GestureRecognizer*>(data);
    gestureRecognizer->m_doubleTapTimer = 0;

    // If doubleTapTimer is fired we should not process double tap,
    // so process single tap here if touched point is already released
    // or do nothing for processing single tap when touch is released.
    if (gestureRecognizer->m_recognizerFunction == &GestureRecognizer::doubleTapGesture) {
        gestureRecognizer->m_gestureHandler->handleSingleTap(gestureRecognizer->m_firstPressedPoint);
        gestureRecognizer->m_recognizerFunction = &GestureRecognizer::noGesture;
    }

    return ECORE_CALLBACK_CANCEL;
}

Eina_Bool GestureRecognizer::tapAndHoldTimerCallback(void* data)
{
    GestureRecognizer* gestureRecognizer = static_cast<GestureRecognizer*>(data);
    gestureRecognizer->m_tapAndHoldTimer = 0;
    gestureRecognizer->m_gestureHandler->handleTapAndHold(gestureRecognizer->m_firstPressedPoint);
    gestureRecognizer->m_recognizerFunction = &GestureRecognizer::noGesture;

    return ECORE_CALLBACK_CANCEL;
}

inline bool GestureRecognizer::exceedsPanThreshold(const IntPoint& first, const IntPoint& last) const
{
    return first.distanceSquaredToPoint(last) > s_squaredPanThreshold;
}

inline bool GestureRecognizer::exceedsDoubleTapThreshold(const IntPoint& first, const IntPoint& last) const
{
    return first.distanceSquaredToPoint(last) > s_squaredDoubleTapThreshold;
}

static inline WKPoint getPointAtIndex(WKArrayRef array, size_t index)
{
    WKTouchPointRef pointRef = static_cast<WKTouchPointRef>(WKArrayGetItemAtIndex(array, index));
    ASSERT(pointRef);

    return WKTouchPointGetPosition(pointRef);
}

static inline Vector<IntPoint> createVectorWithWKArray(WKArrayRef array, size_t size)
{
    Vector<IntPoint> points;
    points.reserveCapacity(size);
    for (size_t i = 0; i < size; ++i)
        points.uncheckedAppend(toIntPoint(getPointAtIndex(array, i)));

    return points;
}

void GestureRecognizer::noGesture(WKEventType type, WKArrayRef touchPoints)
{
    switch (type) {
    case kWKEventTypeTouchStart:
        m_recognizerFunction = &GestureRecognizer::singleTapGesture;
        m_firstPressedPoint = toIntPoint(getPointAtIndex(touchPoints, 0));
        ASSERT(!m_tapAndHoldTimer);
        m_tapAndHoldTimer = ecore_timer_add(s_tapAndHoldTimeoutInSeconds, tapAndHoldTimerCallback, this);
        m_doubleTapTimer = ecore_timer_add(s_doubleTapTimeoutInSeconds, doubleTapTimerCallback, this);
        break;
    case kWKEventTypeTouchMove:
    case kWKEventTypeTouchEnd:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::singleTapGesture(WKEventType type, WKArrayRef touchPoints)
{
    switch (type) {
    case kWKEventTypeTouchStart:
        stopTapTimers();
        m_recognizerFunction = &GestureRecognizer::pinchGesture;
        m_gestureHandler->handlePinchStarted(createVectorWithWKArray(touchPoints, 2));
        break;
    case kWKEventTypeTouchMove: {
        IntPoint currentPoint = toIntPoint(getPointAtIndex(touchPoints, 0));
        if (exceedsPanThreshold(m_firstPressedPoint, currentPoint)) {
            stopTapTimers();
            m_recognizerFunction = &GestureRecognizer::panGesture;
            m_gestureHandler->handlePanStarted(currentPoint);
        }
        break;
    }
    case kWKEventTypeTouchEnd:
        if (m_tapAndHoldTimer) {
            ecore_timer_del(m_tapAndHoldTimer);
            m_tapAndHoldTimer = 0;
        }

        if (m_doubleTapTimer)
            m_recognizerFunction = &GestureRecognizer::doubleTapGesture;
        else {
            m_gestureHandler->handleSingleTap(m_firstPressedPoint);
            m_recognizerFunction = &GestureRecognizer::noGesture;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::doubleTapGesture(WKEventType type, WKArrayRef touchPoints)
{
    switch (type) {
    case kWKEventTypeTouchStart: {
        if (m_doubleTapTimer) {
            ecore_timer_del(m_doubleTapTimer);
            m_doubleTapTimer = 0;
        }

        size_t numberOfTouchPoints = WKArrayGetSize(touchPoints);
        if (numberOfTouchPoints == 1) {
            if (exceedsDoubleTapThreshold(m_firstPressedPoint, toIntPoint(getPointAtIndex(touchPoints, 0))))
                m_recognizerFunction = &GestureRecognizer::singleTapGesture;
        } else {
            m_recognizerFunction = &GestureRecognizer::pinchGesture;
            m_gestureHandler->handlePinchStarted(createVectorWithWKArray(touchPoints, 2));
        }
        break;
    }
    case kWKEventTypeTouchMove: {
        IntPoint currentPoint = toIntPoint(getPointAtIndex(touchPoints, 0));
        if (exceedsPanThreshold(m_firstPressedPoint, currentPoint)) {
            m_recognizerFunction = &GestureRecognizer::panGesture;
            m_gestureHandler->handlePanStarted(currentPoint);
        }
        break;
    }
    case kWKEventTypeTouchEnd:
        m_gestureHandler->handleDoubleTap(m_firstPressedPoint);
        m_recognizerFunction = &GestureRecognizer::noGesture;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::panGesture(WKEventType type, WKArrayRef touchPoints)
{
    switch (type) {
    case kWKEventTypeTouchStart:
        m_recognizerFunction = &GestureRecognizer::pinchGesture;
        m_gestureHandler->handlePinchStarted(createVectorWithWKArray(touchPoints, 2));
        break;
    case kWKEventTypeTouchMove:
        m_gestureHandler->handlePan(toIntPoint(getPointAtIndex(touchPoints, 0)));
        break;
    case kWKEventTypeTouchEnd:
        m_gestureHandler->handlePanFinished();
        m_recognizerFunction = &GestureRecognizer::noGesture;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::pinchGesture(WKEventType type, WKArrayRef touchPoints)
{
    size_t numberOfTouchPoints = WKArrayGetSize(touchPoints);
    ASSERT(numberOfTouchPoints >= 2);

    switch (type) {
    case kWKEventTypeTouchMove: {
        m_gestureHandler->handlePinch(createVectorWithWKArray(touchPoints, 2));
        break;
    }
    case kWKEventTypeTouchEnd:
        if (numberOfTouchPoints == 2) {
            m_gestureHandler->handlePinchFinished();
            m_recognizerFunction = &GestureRecognizer::panGesture;
            WKTouchPointRef pointRef;
            for (size_t i = 0; i < numberOfTouchPoints; ++i) {
                pointRef = static_cast<WKTouchPointRef>(WKArrayGetItemAtIndex(touchPoints, i));
                WKTouchPointState state = WKTouchPointGetState(pointRef);
                if (state != kWKTouchPointStateTouchReleased && state != kWKTouchPointStateTouchCancelled)
                    break;
            }
            ASSERT(pointRef);
            m_gestureHandler->handlePanStarted(toIntPoint(WKTouchPointGetPosition(pointRef)));
        }
        break;
    case kWKEventTypeTouchStart:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::reset()
{
    stopTapTimers();

    m_recognizerFunction = &GestureRecognizer::noGesture;
}

void GestureRecognizer::stopTapTimers()
{
    if (m_doubleTapTimer) {
        ecore_timer_del(m_doubleTapTimer);
        m_doubleTapTimer = 0;
    }

    if (m_tapAndHoldTimer) {
        ecore_timer_del(m_tapAndHoldTimer);
        m_tapAndHoldTimer = 0;
    }
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
