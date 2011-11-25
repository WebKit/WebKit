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

#ifndef QtTapGestureRecognizer_h
#define QtTapGestureRecognizer_h

#include "QtGestureRecognizer.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QObject>
#include <QtCore/QtGlobal>
#include <wtf/OwnPtr.h>

QT_BEGIN_NAMESPACE
class QTouchEvent;
QT_END_NAMESPACE

// FIXME: These constants should possibly depend on DPI.
const qreal initialTriggerDistanceThreshold = 5;
const qreal maxDoubleTapDistance = 120;
const int tapAndHoldTime = 800;
const int doubleClickInterval = 400;

class QtWebPageEventHandler;

namespace WebKit {

class QtTapGestureRecognizer : public QObject, private QtGestureRecognizer {
public:
    QtTapGestureRecognizer(QtWebPageEventHandler*);
    bool recognize(const QTouchEvent*, qint64 eventTimestampMillis);
    void reset();

protected:
    void timerEvent(QTimerEvent*);
    void tapTimeout();
    void tapAndHoldTimeout();

private:
    QBasicTimer m_doubleTapTimer;
    QBasicTimer m_tapAndHoldTimer;
    OwnPtr<QTouchEvent> m_touchBeginEventForTap;

    enum {
        NoTap,
        SingleTapStarted,
        DoubleTapCandidate,
        TapAndHold
    } m_tapState;
};

} // namespace WebKit

#endif /* QtTapGestureRecognizer_h */
