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

#include "EasingCurves.h"
#include "EwkView.h"
#include "NotImplemented.h"
#include "WKSharedAPICast.h"
#include "WKViewEfl.h"

#if ENABLE(TOUCH_EVENTS)

using namespace WebCore;

namespace WebKit {

class GestureHandler {
public:
    explicit GestureHandler(EwkView*);
    ~GestureHandler();

    EwkView* view() { return m_ewkView; }

    void reset();
    void handleSingleTap(const IntPoint&);
    void handleDoubleTap(const IntPoint&);
    void handleTapAndHold(const IntPoint&);
    void handlePanStarted(const IntPoint&);
    void handlePan(const IntPoint&, double timestamp);
    void handlePanFinished();
    void handleFlick(const IntSize&);
    void handlePinchStarted(const Vector<IntPoint>&);
    void handlePinch(const Vector<IntPoint>&);
    void handlePinchFinished();

private:
    static Eina_Bool panAnimatorCallback(void*);
    static Eina_Bool flickAnimatorCallback(void*);

    static const int s_historyCapacity = 5;

    EwkView* m_ewkView;
    IntPoint m_lastPoint;
    IntPoint m_currentPoint;
    bool m_isCurrentPointUpdated;
    Ecore_Animator* m_panAnimator;
    Ecore_Animator* m_flickAnimator;
    IntSize m_flickOffset;
    unsigned m_flickDuration;
    unsigned m_flickIndex;

    struct HistoryItem {
        IntPoint point;
        double timestamp;
    };
    Vector<HistoryItem> m_history;
    size_t m_oldestHistoryItemIndex;
};

GestureHandler::GestureHandler(EwkView* ewkView)
    : m_ewkView(ewkView)
    , m_isCurrentPointUpdated(false)
    , m_panAnimator(0)
    , m_flickAnimator(0)
    , m_flickDuration(0)
    , m_flickIndex(0)
    , m_oldestHistoryItemIndex(0)
{
    m_history.reserveInitialCapacity(s_historyCapacity);
}

GestureHandler::~GestureHandler()
{
    reset();
}

void GestureHandler::reset()
{
    if (m_panAnimator) {
        ecore_animator_del(m_panAnimator);
        m_panAnimator = 0;

        m_oldestHistoryItemIndex = 0;
        if (m_history.size())
            m_history.resize(0);
    }

    if (m_flickAnimator) {
        ecore_animator_del(m_flickAnimator);
        m_flickAnimator = 0;
    }
}

void GestureHandler::handleSingleTap(const IntPoint& position)
{
    Evas* evas = evas_object_evas_get(m_ewkView->evasObject());
    ASSERT(evas);

    // Send mouse move, down and up event to create fake click event.
    Evas_Event_Mouse_Move mouseMove;
    mouseMove.buttons = 0;
    mouseMove.prev.output.x = mouseMove.prev.canvas.x = position.x();
    mouseMove.prev.output.y = mouseMove.prev.canvas.y = position.y();
    mouseMove.cur.output.x = mouseMove.cur.canvas.x = position.x();
    mouseMove.cur.output.y = mouseMove.cur.canvas.y = position.y();
    mouseMove.data = 0;
    mouseMove.modifiers = const_cast<Evas_Modifier*>(evas_key_modifier_get(evas));
    mouseMove.locks = const_cast<Evas_Lock*>(evas_key_lock_get(evas));
    mouseMove.timestamp = ecore_loop_time_get();
    mouseMove.event_flags = EVAS_EVENT_FLAG_NONE;
    mouseMove.dev = 0;
    WKViewSendMouseMoveEvent(m_ewkView->wkView(), &mouseMove);

    Evas_Event_Mouse_Down mouseDown;
    mouseDown.button = 1;
    mouseDown.output.x = mouseDown.canvas.x = position.x();
    mouseDown.output.y = mouseDown.canvas.y = position.y();
    mouseDown.data = 0;
    mouseDown.modifiers = const_cast<Evas_Modifier*>(evas_key_modifier_get(evas));
    mouseDown.locks = const_cast<Evas_Lock*>(evas_key_lock_get(evas));
    mouseDown.flags = EVAS_BUTTON_NONE;
    mouseDown.timestamp = ecore_loop_time_get();
    mouseDown.event_flags = EVAS_EVENT_FLAG_NONE;
    mouseDown.dev = 0;
    WKViewSendMouseDownEvent(m_ewkView->wkView(), &mouseDown);

    Evas_Event_Mouse_Up mouseUp;
    mouseUp.button = 1;
    mouseUp.output.x = mouseUp.canvas.x = position.x();
    mouseUp.output.y = mouseUp.canvas.y = position.y();
    mouseUp.data = 0;
    mouseUp.modifiers = const_cast<Evas_Modifier*>(evas_key_modifier_get(evas));
    mouseUp.locks = const_cast<Evas_Lock*>(evas_key_lock_get(evas));
    mouseUp.flags = EVAS_BUTTON_NONE;
    mouseUp.timestamp = ecore_loop_time_get();
    mouseUp.event_flags = EVAS_EVENT_FLAG_NONE;
    mouseUp.dev = 0;
    WKViewSendMouseUpEvent(m_ewkView->wkView(), &mouseUp);
}

void GestureHandler::handleDoubleTap(const IntPoint&)
{
    notImplemented();
}

void GestureHandler::handleTapAndHold(const IntPoint&)
{
    notImplemented();
}

Eina_Bool GestureHandler::panAnimatorCallback(void* data)
{
    GestureHandler* gestureHandler = static_cast<GestureHandler*>(data);
    if (!gestureHandler->m_isCurrentPointUpdated || gestureHandler->m_lastPoint == gestureHandler->m_currentPoint)
        return ECORE_CALLBACK_RENEW;

    gestureHandler->view()->scrollBy(gestureHandler->m_lastPoint - gestureHandler->m_currentPoint);
    gestureHandler->m_lastPoint = gestureHandler->m_currentPoint;
    gestureHandler->m_isCurrentPointUpdated = false;

    return ECORE_CALLBACK_RENEW;
}

void GestureHandler::handlePanStarted(const IntPoint& point)
{
    ASSERT(!m_panAnimator);
    m_panAnimator = ecore_animator_add(panAnimatorCallback, this);
    m_lastPoint = m_currentPoint = point;
}

void GestureHandler::handlePan(const IntPoint& point, double timestamp)
{
    m_currentPoint = point;
    m_isCurrentPointUpdated = true;

    // Save current point to use to calculate offset of flick.
    HistoryItem item = { m_currentPoint, timestamp };
    if (m_history.size() < m_history.capacity())
        m_history.uncheckedAppend(item);
    else {
        m_history[m_oldestHistoryItemIndex++] = item;
        if (m_oldestHistoryItemIndex == m_history.capacity())
            m_oldestHistoryItemIndex = 0;
    }
}

void GestureHandler::handlePanFinished()
{
    ASSERT(m_panAnimator);
    ecore_animator_del(m_panAnimator);
    m_panAnimator = 0;

    if (!m_history.isEmpty()) {
        // Calculate offset to move during one frame.
        const HistoryItem& oldestHistoryItem = m_history[m_oldestHistoryItemIndex];
        double frame = (ecore_time_get() - oldestHistoryItem.timestamp) / ecore_animator_frametime_get();
        IntSize offset = oldestHistoryItem.point - m_currentPoint;
        offset.scale(1 / frame);
        handleFlick(offset);
    }

    m_oldestHistoryItemIndex = 0;
    if (m_history.size())
        m_history.resize(0);
}

Eina_Bool GestureHandler::flickAnimatorCallback(void* data)
{
    GestureHandler* gestureHandler = static_cast<GestureHandler*>(data);
    float multiplier = easeInOutQuad(gestureHandler->m_flickIndex, 0, 1.0, gestureHandler->m_flickDuration);
    float offsetWidth = gestureHandler->m_flickOffset.width() * multiplier;
    float offsetHeight = gestureHandler->m_flickOffset.height() * multiplier;
    offsetWidth = (offsetWidth > 0) ? ceilf(offsetWidth) : floorf(offsetWidth);
    offsetHeight = (offsetHeight > 0) ? ceilf(offsetHeight) : floorf(offsetHeight);
    IntSize offset(offsetWidth, offsetHeight);
    gestureHandler->m_flickIndex--;

    if (offset.isZero() || !gestureHandler->view()->scrollBy(offset) || !gestureHandler->m_flickIndex) {
        gestureHandler->m_flickAnimator = 0;
        return ECORE_CALLBACK_CANCEL;
    }

    return ECORE_CALLBACK_RENEW;
}

void GestureHandler::handleFlick(const IntSize& offset)
{
    m_flickOffset = offset;
    m_flickIndex = m_flickDuration = 1 / ecore_animator_frametime_get();
    m_flickAnimator = ecore_animator_add(flickAnimatorCallback, this);
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
    , m_gestureHandler(std::make_unique<GestureHandler>(ewkView))
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

    (this->*m_recognizerFunction)(eventRef);
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

void GestureRecognizer::noGesture(WKTouchEventRef eventRef)
{
    switch (WKTouchEventGetType(eventRef)) {
    case kWKEventTypeTouchStart: {
        WKArrayRef touchPoints = WKTouchEventGetTouchPoints(eventRef);
        switch (WKArrayGetSize(touchPoints)) {
        case 1:
            m_gestureHandler->reset();
            m_recognizerFunction = &GestureRecognizer::singleTapGesture;
            m_firstPressedPoint = toIntPoint(getPointAtIndex(touchPoints, 0));
            ASSERT(!m_tapAndHoldTimer);
            m_tapAndHoldTimer = ecore_timer_add(s_tapAndHoldTimeoutInSeconds, tapAndHoldTimerCallback, this);
            m_doubleTapTimer = ecore_timer_add(s_doubleTapTimeoutInSeconds, doubleTapTimerCallback, this);
            break;
        case 2:
            m_recognizerFunction = &GestureRecognizer::pinchGesture;
            m_gestureHandler->handlePinchStarted(createVectorWithWKArray(touchPoints, 2));
            break;
        default:
            // There's no defined gesture when we touch three or more points.
            notImplemented();
            break;
        }
        break;
    }
    case kWKEventTypeTouchMove:
    case kWKEventTypeTouchEnd:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GestureRecognizer::singleTapGesture(WKTouchEventRef eventRef)
{
    WKArrayRef touchPoints = WKTouchEventGetTouchPoints(eventRef);

    switch (WKTouchEventGetType(eventRef)) {
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

void GestureRecognizer::doubleTapGesture(WKTouchEventRef eventRef)
{
    WKArrayRef touchPoints = WKTouchEventGetTouchPoints(eventRef);

    switch (WKTouchEventGetType(eventRef)) {
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

void GestureRecognizer::panGesture(WKTouchEventRef eventRef)
{
    WKArrayRef touchPoints = WKTouchEventGetTouchPoints(eventRef);

    switch (WKTouchEventGetType(eventRef)) {
    case kWKEventTypeTouchStart:
        m_recognizerFunction = &GestureRecognizer::pinchGesture;
        m_gestureHandler->handlePinchStarted(createVectorWithWKArray(touchPoints, 2));
        break;
    case kWKEventTypeTouchMove:
        m_gestureHandler->handlePan(toIntPoint(getPointAtIndex(touchPoints, 0)), WKTouchEventGetTimestamp(eventRef));
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

void GestureRecognizer::pinchGesture(WKTouchEventRef eventRef)
{
    WKArrayRef touchPoints = WKTouchEventGetTouchPoints(eventRef);
    size_t numberOfTouchPoints = WKArrayGetSize(touchPoints);
    ASSERT(numberOfTouchPoints >= 2);

    switch (WKTouchEventGetType(eventRef)) {
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
    m_gestureHandler->reset();

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
