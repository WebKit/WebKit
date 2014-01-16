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

#ifndef GestureRecognizer_h
#define GestureRecognizer_h

#if ENABLE(TOUCH_EVENTS)

#include <Ecore.h>
#include <WebCore/IntPoint.h>
#include <WebKit2/WKEventEfl.h>
#include <wtf/Vector.h>

class EwkView;

namespace WebKit {

class GestureHandler;

class GestureRecognizer {
public:
    explicit GestureRecognizer(EwkView*);
    ~GestureRecognizer();

    void processTouchEvent(WKTouchEventRef);
    void reset();

private:
    static Eina_Bool doubleTapTimerCallback(void*);
    static Eina_Bool tapAndHoldTimerCallback(void*);

    bool exceedsPanThreshold(const WebCore::IntPoint& first, const WebCore::IntPoint& last) const;
    bool exceedsDoubleTapThreshold(const WebCore::IntPoint& first, const WebCore::IntPoint& last) const;

    // State functions.
    void noGesture(WKTouchEventRef);
    void singleTapGesture(WKTouchEventRef);
    void doubleTapGesture(WKTouchEventRef);
    void panGesture(WKTouchEventRef);
    void pinchGesture(WKTouchEventRef);

    void stopTapTimers();

    static const double s_doubleTapTimeoutInSeconds;
    static const double s_tapAndHoldTimeoutInSeconds;
    static const int s_squaredDoubleTapThreshold;
    static const int s_squaredPanThreshold;

    typedef void (GestureRecognizer::*RecognizerFunction)(WKTouchEventRef);
    RecognizerFunction m_recognizerFunction;

    std::unique_ptr<WebKit::GestureHandler> m_gestureHandler;
    Ecore_Timer* m_tapAndHoldTimer;
    Ecore_Timer* m_doubleTapTimer;
    WebCore::IntPoint m_firstPressedPoint;
};

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)

#endif // GestureRecognizer_h
