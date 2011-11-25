/*
 * Copyright (C) 2011 Benjamin Poulain <benjamin@webkit.org>
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
#include "QtPinchGestureRecognizer.h"

#include "QtWebPageEventHandler.h"
#include <QtCore/QLineF>

namespace WebKit {

const qreal pinchInitialTriggerDistanceThreshold = 5.;

static inline int findTouchPointIndex(const QList<QTouchEvent::TouchPoint>& touchPoints, const QtPinchGestureRecognizer::TouchPointInformation& pointInformation)
{
    const int touchCount = touchPoints.size();
    for (int i = 0; i < touchCount; ++i) {
        const QTouchEvent::TouchPoint& touchPoint = touchPoints.at(i);
        if (touchPoint.id() == pointInformation.id)
            return i;
    }
    return -1;
}

static inline QPointF computePinchCenter(const QTouchEvent::TouchPoint& point1, const QTouchEvent::TouchPoint& point2)
{
    return (point1.pos() + point2.pos()) / 2.0f;
}

QtPinchGestureRecognizer::QtPinchGestureRecognizer(QtWebPageEventHandler* eventHandler)
    : QtGestureRecognizer(eventHandler)
{
    reset();
}

bool QtPinchGestureRecognizer::recognize(const QTouchEvent* event)
{
    if (!interactionEngine())
        return false;

    const QList<QTouchEvent::TouchPoint>& touchPoints = event->touchPoints();
    if (touchPoints.size() < 2) {
        if (m_state == GestureRecognized)
            interactionEngine()->pinchGestureEnded();
        reset();
        return false;
    }

    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        switch (m_state) {
        case NoGesture:
            initializeGesture(touchPoints);
            return false;
        case GestureRecognitionStarted:
        case GestureRecognized:
            ASSERT(m_point1.isValid());
            ASSERT(m_point2.isValid());

            const int point1Index = findTouchPointIndex(touchPoints, m_point1);
            if (point1Index < 0) {
                reset();
                return false;
            }
            const int point2Index = findTouchPointIndex(touchPoints, m_point2);
            if (point2Index < 0) {
                reset();
                return false;
            }

            const QTouchEvent::TouchPoint& point1 = touchPoints.at(point1Index);
            const QTouchEvent::TouchPoint& point2 = touchPoints.at(point2Index);
            if (m_state == GestureRecognitionStarted) {
                // FIXME: The gesture should only start if the touch events were not accepted at the start of the touch sequence.
                const qreal pinchDistance = qAbs(QLineF(point1.screenPos(), point2.screenPos()).length() - QLineF(m_point1.initialScreenPosition, m_point2.initialScreenPosition).length());
                if (pinchDistance < pinchInitialTriggerDistanceThreshold)
                    return false;
                m_state = GestureRecognized;
                interactionEngine()->pinchGestureStarted(computePinchCenter(point1, point2));

                // We reset the initial position to the previous position in order to avoid the jump caused
                // by skipping all the events between the beginning and when the threshold is hit.
                m_point1.initialPosition = point1.lastPos();
                m_point1.initialScreenPosition = point1.lastScreenPos();
                m_point2.initialPosition = point2.lastPos();
                m_point2.initialScreenPosition = point2.lastScreenPos();
            }
            ASSERT(m_state == GestureRecognized);
            const qreal currentSpanDistance = QLineF(point1.screenPos(), point2.screenPos()).length();
            const qreal initialSpanDistance = QLineF(m_point1.initialScreenPosition, m_point2.initialScreenPosition).length();
            const qreal totalScaleFactor = currentSpanDistance / initialSpanDistance;
            const QPointF touchCenterInPageViewCoordinates = computePinchCenter(point1, point2);
            interactionEngine()->pinchGestureRequestUpdate(touchCenterInPageViewCoordinates, totalScaleFactor);
            return true;
            break;
        }
        break;
    case QEvent::TouchEnd:
        if (m_state == GestureRecognized) {
            interactionEngine()->pinchGestureEnded();
            reset();
            return true;
        }
        reset();
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return false;
}

void QtPinchGestureRecognizer::reset()
{
    QtGestureRecognizer::reset();
    m_point1 = TouchPointInformation();
    m_point2 = TouchPointInformation();
}

void QtPinchGestureRecognizer::initializeGesture(const QList<QTouchEvent::TouchPoint>& touchPoints)
{
    ASSERT(!m_point1.isValid());
    ASSERT(!m_point2.isValid());

    m_state = GestureRecognitionStarted;

    m_point1 = TouchPointInformation(touchPoints.at(0));
    m_point2 = TouchPointInformation(touchPoints.at(1));

    ASSERT(m_point1.isValid());
    ASSERT(m_point2.isValid());
}

}
