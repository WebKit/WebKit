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

#ifndef QtPinchGestureRecognizer_h
#define QtPinchGestureRecognizer_h

#include "QtGestureRecognizer.h"

#include <QTouchEvent>
#include <QtCore/QList>
#include <QtCore/QPointF>

QT_BEGIN_NAMESPACE
class QTouchEvent;
QT_END_NAMESPACE

namespace WebKit {

class TouchViewInterface;

class QtPinchGestureRecognizer : private QtGestureRecognizer {
public:
    struct TouchPointInformation {
        inline TouchPointInformation();
        inline TouchPointInformation(const QTouchEvent::TouchPoint&);
        inline bool isValid() const;

        int id;
        QPointF initialScreenPosition;
        QPointF initialPosition;
    };

    QtPinchGestureRecognizer(TouchViewInterface*);
    bool recognize(const QTouchEvent*);
    void reset();

private:
    void initializeGesture(const QList<QTouchEvent::TouchPoint>& touchPoints);

    TouchPointInformation m_point1;
    TouchPointInformation m_point2;
};

inline QtPinchGestureRecognizer::TouchPointInformation::TouchPointInformation()
    : id(-1)
{
}

inline QtPinchGestureRecognizer::TouchPointInformation::TouchPointInformation(const QTouchEvent::TouchPoint& touchPoint)
    : id(touchPoint.id())
    , initialScreenPosition(touchPoint.screenPos())
    , initialPosition(touchPoint.pos())
{
}

inline bool QtPinchGestureRecognizer::TouchPointInformation::isValid() const
{
    return id >= 0;
}

}

#endif /* QtPinchGestureRecognizer_h */
