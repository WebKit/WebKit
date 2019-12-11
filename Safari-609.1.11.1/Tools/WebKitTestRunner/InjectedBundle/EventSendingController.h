/*
 * Copyright (C) 2010, 2011, 2014-2015 Apple Inc. All rights reserved.
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

#pragma once

#include "JSWrappable.h"
#include <WebKit/WKEvent.h>
#include <WebKit/WKGeometry.h>
#include <wtf/Ref.h>

namespace WTR {

class EventSendingController : public JSWrappable {
public:
    static Ref<EventSendingController> create();
    virtual ~EventSendingController();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void mouseDown(int button, JSValueRef modifierArray);
    void mouseUp(int button, JSValueRef modifierArray);
    void mouseMoveTo(int x, int y);
    void mouseForceClick();
    void startAndCancelMouseForceClick();
    void mouseForceDown();
    void mouseForceUp();
    void mouseForceChanged(double force);
    void mouseScrollBy(int x, int y);
    void mouseScrollByWithWheelAndMomentumPhases(int x, int y, JSStringRef phase, JSStringRef momentum);
    void continuousMouseScrollBy(int x, int y, bool paged);
    JSValueRef contextClick();
    void leapForward(int milliseconds);
    void scheduleAsynchronousClick();
    void monitorWheelEvents();
    void callAfterScrollingCompletes(JSValueRef functionCallback);

    void keyDown(JSStringRef key, JSValueRef modifierArray, int location);
    void scheduleAsynchronousKeyDown(JSStringRef key);

    // Zoom functions.
    void textZoomIn();
    void textZoomOut();
    void zoomPageIn();
    void zoomPageOut();
    void scalePageBy(double scale, double x, double y);

#if ENABLE(TOUCH_EVENTS)
    // Touch events.
    void addTouchPoint(int x, int y);
    void updateTouchPoint(int index, int x, int y);
    void setTouchModifier(const JSStringRef &modifier, bool enable);
    void setTouchPointRadius(int radiusX, int radiusY);
    void touchStart();
    void touchMove();
    void touchEnd();
    void touchCancel();
    void clearTouchPoints();
    void releaseTouchPoint(int index);
    void cancelTouchPoint(int index);
#endif

private:
    EventSendingController();
    WKPoint m_position;
};

} // namespace WTR
