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
    , m_candidate(Invalid)
{
}

bool QtTapGestureRecognizer::withinDistance(const QTouchEvent::TouchPoint& touchPoint, int distance)
{
    return QLineF(touchPoint.screenPos(), m_lastTouchPoint.screenPos()).length() < distance;
}

bool QtTapGestureRecognizer::update(QEvent::Type eventType, const QTouchEvent::TouchPoint& touchPoint)
{
    ASSERT(m_eventHandler);

    switch (eventType) {
    case QEvent::TouchBegin:
        m_doubleTapTimer.stop(); // Cancel other pending single tap event.

        ASSERT(!m_tapAndHoldTimer.isActive());
        m_tapAndHoldTimer.start(tapAndHoldTime, this);

        if (m_lastTouchPoint.id() != -1 && withinDistance(touchPoint, maxDoubleTapDistance))
            m_candidate = DoubleTapCandidate;
        else {
            m_candidate = SingleTapCandidate;
            // The below in facts resets any previous single tap event.
            m_highlightTimer.start(highlightDelay, this);
            m_lastTouchPoint = touchPoint;
            m_doubleTapTimer.start(maxDoubleTapInterval, this);
        }
        break;

    case QEvent::TouchUpdate:
        // If the touch point moves further than the threshold, we cancel the tap gesture.
        if (m_candidate != Invalid && !withinDistance(touchPoint, maxPanDistance))
            reset();
        break;

    case QEvent::TouchEnd:
        m_tapAndHoldTimer.stop();

        if (m_candidate == Invalid)
            break;

        if (m_candidate == DoubleTapCandidate) {
            m_eventHandler->handlePotentialSingleTapEvent(QTouchEvent::TouchPoint());
            m_eventHandler->handleDoubleTapEvent(touchPoint);
            reset();
        }

        break;

    default:
        break;
    }

    return false;
}

void QtTapGestureRecognizer::cancel()
{
    if (m_candidate == Invalid)
        return;

    reset();
}

void QtTapGestureRecognizer::highlightTimeout()
{
    m_highlightTimer.stop();

    if (m_candidate != SingleTapCandidate)
        return;

    ASSERT(m_lastTouchPoint.id() != -1);
    m_eventHandler->handlePotentialSingleTapEvent(m_lastTouchPoint);
}

void QtTapGestureRecognizer::singleTapTimeout()
{
    m_doubleTapTimer.stop();

    // Finger is still pressed, ignore.
    if (m_tapAndHoldTimer.isActive())
        return;

    ASSERT(m_lastTouchPoint.id() != -1);

    if (m_candidate == SingleTapCandidate) {
        m_eventHandler->handlePotentialSingleTapEvent(QTouchEvent::TouchPoint());
        m_eventHandler->handleSingleTapEvent(m_lastTouchPoint);
    }
    reset();
}

void QtTapGestureRecognizer::tapAndHoldTimeout()
{
    m_tapAndHoldTimer.stop();

    ASSERT(m_lastTouchPoint.id() != -1);
#if 0 // No support for synthetic context menus in WK2 yet.
    m_eventHandler->handlePotentialSingleTapEvent(QTouchEvent::TouchPoint());
    m_eventHandler->handleTapAndHoldEvent(m_lastTouchPoint);
#endif
    reset();
}

void QtTapGestureRecognizer::reset()
{
    if (m_candidate != Invalid)
        m_eventHandler->handlePotentialSingleTapEvent(QTouchEvent::TouchPoint());

    m_candidate = Invalid;
    m_lastTouchPoint.setId(-1);
    m_highlightTimer.stop();
    m_doubleTapTimer.stop();
    m_tapAndHoldTimer.stop();

    QtGestureRecognizer::reset();
}

void QtTapGestureRecognizer::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == m_highlightTimer.timerId())
        highlightTimeout();
    else if (timerId == m_doubleTapTimer.timerId())
        singleTapTimeout();
    else if (timerId == m_tapAndHoldTimer.timerId())
        tapAndHoldTimeout();
    else
        QObject::timerEvent(ev);
}

} // namespace WebKit
