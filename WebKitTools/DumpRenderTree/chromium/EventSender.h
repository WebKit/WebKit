/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
  EventSender class:
  Bound to a JavaScript window.eventSender object using
  CppBoundClass::bindToJavascript(), this allows layout tests to fire DOM events.
*/

#ifndef EventSender_h
#define EventSender_h

#include "CppBoundClass.h"
#include "Task.h"
#include "public/WebDragOperation.h"
#include "public/WebInputEvent.h"
#include "public/WebPoint.h"

class TestShell;

namespace WebKit {
class WebDragData;
class WebView;
}

class EventSender : public CppBoundClass {
public:
    // Builds the property and method lists needed to bind this class to a JS
    // object.
    EventSender(TestShell*);

    // Resets some static variable state.
    void reset();

    // Simulate drag&drop system call.
    void doDragDrop(const WebKit::WebDragData&, WebKit::WebDragOperationsMask);

    // JS callback methods.
    void mouseDown(const CppArgumentList&, CppVariant*);
    void mouseUp(const CppArgumentList&, CppVariant*);
    void mouseMoveTo(const CppArgumentList&, CppVariant*);
    void leapForward(const CppArgumentList&, CppVariant*);
    void keyDown(const CppArgumentList&, CppVariant*);
    void dispatchMessage(const CppArgumentList&, CppVariant*);
    void textZoomIn(const CppArgumentList&, CppVariant*);
    void textZoomOut(const CppArgumentList&, CppVariant*);
    void zoomPageIn(const CppArgumentList&, CppVariant*);
    void zoomPageOut(const CppArgumentList&, CppVariant*);
    void mouseScrollBy(const CppArgumentList&, CppVariant*);
    void continuousMouseScrollBy(const CppArgumentList&, CppVariant*);
    void scheduleAsynchronousClick(const CppArgumentList&, CppVariant*);
    void beginDragWithFiles(const CppArgumentList&, CppVariant*);
    CppVariant dragMode;

    void addTouchPoint(const CppArgumentList&, CppVariant*);
    void cancelTouchPoint(const CppArgumentList&, CppVariant*);
    void clearTouchPoints(const CppArgumentList&, CppVariant*);
    void releaseTouchPoint(const CppArgumentList&, CppVariant*);
    void setTouchModifier(const CppArgumentList&, CppVariant*);
    void touchCancel(const CppArgumentList&, CppVariant*);
    void touchEnd(const CppArgumentList&, CppVariant*);
    void touchMove(const CppArgumentList&, CppVariant*);
    void touchStart(const CppArgumentList&, CppVariant*);
    void updateTouchPoint(const CppArgumentList&, CppVariant*);

    // Unimplemented stubs
    void contextClick(const CppArgumentList&, CppVariant*);
    void enableDOMUIEventLogging(const CppArgumentList&, CppVariant*);
    void fireKeyboardEventsToElement(const CppArgumentList&, CppVariant*);
    void clearKillRing(const CppArgumentList&, CppVariant*);

    // Properties used in layout tests.
#if defined(OS_WIN)
    CppVariant wmKeyDown;
    CppVariant wmKeyUp;
    CppVariant wmChar;
    CppVariant wmDeadChar;
    CppVariant wmSysKeyDown;
    CppVariant wmSysKeyUp;
    CppVariant wmSysChar;
    CppVariant wmSysDeadChar;
#endif

    TaskList* taskList() { return &m_taskList; }

private:
    // Returns the test shell's webview.
    WebKit::WebView* webview();

    // Returns true if dragMode is true.
    bool isDragMode() { return dragMode.isBool() && dragMode.toBoolean(); }

    // Sometimes we queue up mouse move and mouse up events for drag drop
    // handling purposes.  These methods dispatch the event.
    void doMouseMove(const WebKit::WebMouseEvent&);
    void doMouseUp(const WebKit::WebMouseEvent&);
    static void doLeapForward(int milliseconds);
    void replaySavedEvents();

    // Helper to return the button type given a button code
    static WebKit::WebMouseEvent::Button getButtonTypeFromButtonNumber(int);

    // Helper to extract the button number from the optional argument in
    // mouseDown and mouseUp
    static int getButtonNumberFromSingleArg(const CppArgumentList&);

    // Returns true if the specified key code passed in needs a shift key
    // modifier to be passed into the generated event.
    bool needsShiftModifier(int);

    void updateClickCountForButton(WebKit::WebMouseEvent::Button);

    // Compose a touch event from the current touch points and send it.
    void sendCurrentTouchEvent(const WebKit::WebInputEvent::Type);

    // Handle a request to send a wheel event.
    void handleMouseWheel(const CppArgumentList&, CppVariant*, bool continuous);

    TaskList m_taskList;

    // Non-owning pointer.  The EventSender is owned by the TestShell.
    TestShell* m_shell;

    // Location of last mouseMoveTo event.
    static WebKit::WebPoint lastMousePos;

    // Currently pressed mouse button (Left/Right/Middle or None)
    static WebKit::WebMouseEvent::Button pressedButton;

    // The last button number passed to mouseDown and mouseUp.
    // Used to determine whether the click count continues to
    // increment or not.
    static WebKit::WebMouseEvent::Button lastButtonType;
};

#endif // EventSender_h
