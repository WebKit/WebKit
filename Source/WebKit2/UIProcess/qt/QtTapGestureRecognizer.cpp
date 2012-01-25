/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtTapGestureRecognizer.h"

#include "QtWebPageEventHandler.h"
#include <QLineF>
#include <QTouchEvent>

namespace WebKit {

QtTapGestureRecognizer::QtTapGestureRecognizer(QtWebPageEventHandler* eventHandler)
    : QtGestureRecognizer(eventHandler)
    , m_tapState(NoTap)
{
    reset();
}

bool QtTapGestureRecognizer::recognize(const QTouchEvent* event, qint64 eventTimestampMillis)
{
    if (event->touchPoints().size() != 1) {
        reset();
        return false;
    }

    switch (event->type()) {
    case QEvent::TouchBegin:
        ASSERT(m_tapState == NoTap);
        ASSERT(!m_tapAndHoldTimer.isActive());

        m_tapAndHoldTimer.start(tapAndHoldTime, this);

        if (m_doubleTapTimer.isActive()) {
            // Might be double tap.
            ASSERT(m_touchBeginEventForTap);
            m_doubleTapTimer.stop();
            QPointF lastPosition = m_touchBeginEventForTap->touchPoints().first().screenPos();
            QPointF newPosition = event->touchPoints().first().screenPos();
            if (QLineF(lastPosition, newPosition).length() < maxDoubleTapDistance)
                m_tapState = DoubleTapCandidate;
            else {
                // Received a new tap, that is unrelated to the previous one.
                tapTimeout();
                m_tapState = SingleTapStarted;
            }
        } else
            m_tapState = SingleTapStarted;
        m_touchBeginEventForTap = adoptPtr(new QTouchEvent(*event));

        if (m_tapState == SingleTapStarted) {
            const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();
            m_eventHandler->handlePotentialSingleTapEvent(touchPoint);
        }
        break;
    case QEvent::TouchUpdate:
        // If the touch point moves further than the threshold, we cancel the tap gesture.
        if (m_tapState == SingleTapStarted) {
            const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();
            QPointF offset(touchPoint.scenePos() - m_touchBeginEventForTap->touchPoints().first().scenePos());
            const qreal distX = qAbs(offset.x());
            const qreal distY = qAbs(offset.y());
            if (distX > initialTriggerDistanceThreshold || distY > initialTriggerDistanceThreshold)
                reset();
        }
        break;
    case QEvent::TouchEnd:
        m_tapAndHoldTimer.stop();

        switch (m_tapState) {
        case DoubleTapCandidate:
            {
                ASSERT(!m_doubleTapTimer.isActive());
                m_tapState = NoTap;

                const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();
                QPointF startPosition = touchPoint.startScreenPos();
                QPointF endPosition = touchPoint.screenPos();
                if (QLineF(endPosition, startPosition).length() < maxDoubleTapDistance && m_eventHandler)
                    m_eventHandler->handleDoubleTapEvent(touchPoint);
                break;
            }
        case SingleTapStarted:
            ASSERT(!m_doubleTapTimer.isActive());
            m_doubleTapTimer.start(doubleClickInterval, this);
            m_tapState = NoTap;
            break;
        case TapAndHold:
            m_tapState = NoTap;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (m_tapState == NoTap)
        m_eventHandler->handlePotentialSingleTapEvent(QTouchEvent::TouchPoint());

    return false;
}

void QtTapGestureRecognizer::tapTimeout()
{
    m_doubleTapTimer.stop();
    m_eventHandler->handleSingleTapEvent(m_touchBeginEventForTap->touchPoints().at(0));
    m_touchBeginEventForTap.clear();
}

void QtTapGestureRecognizer::tapAndHoldTimeout()
{
    ASSERT(m_touchBeginEventForTap);
    m_tapAndHoldTimer.stop();
#if 0 // No support for synthetic context menus in WK2 yet.
    QTouchEvent::TouchPoint tapPoint = m_touchBeginEventForTap->touchPoints().at(0);
    WebGestureEvent gesture(WebEvent::GestureTapAndHold, tapPoint.pos().toPoint(), tapPoint.screenPos().toPoint(), WebEvent::Modifiers(0), 0);
    if (m_webPageProxy)
        m_webPageProxy->handleGestureEvent(gesture);
#endif
    m_touchBeginEventForTap.clear();
    m_tapState = TapAndHold;

    ASSERT(!m_doubleTapTimer.isActive());
    m_doubleTapTimer.stop();
}

void QtTapGestureRecognizer::reset()
{
    m_tapState = NoTap;
    m_touchBeginEventForTap.clear();
    m_tapAndHoldTimer.stop();

    QtGestureRecognizer::reset();
}

void QtTapGestureRecognizer::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_doubleTapTimer.timerId())
        tapTimeout();
    else if (timerId == m_tapAndHoldTimer.timerId())
        tapAndHoldTimeout();
    else
        QObject::timerEvent(ev);
}

} // namespace WebKit
