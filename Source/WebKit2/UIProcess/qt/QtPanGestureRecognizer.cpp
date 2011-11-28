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

#include "QtPanGestureRecognizer.h"

#include "QtWebPageEventHandler.h"
#include <QTouchEvent>

namespace WebKit {

QtPanGestureRecognizer::QtPanGestureRecognizer(QtWebPageEventHandler* eventHandler)
    : QtGestureRecognizer(eventHandler)
{
    reset();
}

bool QtPanGestureRecognizer::recognize(const QTouchEvent* event, qint64 eventTimestampMillis)
{
    if (!interactionEngine())
        return false;

    // Pan gesture always starts on TouchBegin unless the engine is suspended, or
    // we ignored the event.
    if (m_state == NoGesture && event->type() != QEvent::TouchBegin)
        return false;

    // Having multiple touch points cancel the panning gesture.
    if (event->touchPoints().size() > 1) {
        if (m_state == GestureRecognized)
            interactionEngine()->panGestureCancelled();
        reset();
        return false;
    }

    const QTouchEvent::TouchPoint& touchPoint = event->touchPoints().first();

    switch (event->type()) {
    case QEvent::TouchBegin:
        ASSERT(m_state == NoGesture);
        m_state = GestureRecognitionStarted;
        m_firstPosition = touchPoint.screenPos();
        return false;
    case QEvent::TouchUpdate: {
        ASSERT(m_state != NoGesture);
        if (m_state == GestureRecognitionStarted) {
            // To start the gesture, the delta from start in screen coordinates
            // must be bigger than the trigger threshold.
            QPointF totalOffsetFromStart(touchPoint.screenPos() - m_firstPosition);
            if (qAbs(totalOffsetFromStart.x()) < panningInitialTriggerDistanceThreshold && qAbs(totalOffsetFromStart.y()) < panningInitialTriggerDistanceThreshold)
                return false;

            m_state = GestureRecognized;
            interactionEngine()->panGestureStarted(touchPoint.pos(), eventTimestampMillis);
        }

        ASSERT(m_state == GestureRecognized);
        interactionEngine()->panGestureRequestUpdate(touchPoint.pos(), eventTimestampMillis);
        return true;
    }
    case QEvent::TouchEnd:
        if (m_state == GestureRecognized) {
            interactionEngine()->panGestureEnded(touchPoint.pos(), eventTimestampMillis);
            reset();
            return true;
        }
        ASSERT(m_state == GestureRecognitionStarted);
        reset();
        return false;
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void QtPanGestureRecognizer::reset()
{
    QtGestureRecognizer::reset();
    m_firstPosition = QPointF();
}

} // namespace WebKit
