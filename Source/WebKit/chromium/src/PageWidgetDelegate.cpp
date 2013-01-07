/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "PageWidgetDelegate.h"

#include "Frame.h"
#include "FrameView.h"
#include "PageOverlayList.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "painting/GraphicsContextBuilder.h"
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

static inline FrameView* mainFrameView(Page* page)
{
    if (!page)
        return 0;
    // FIXME: Can we remove this check?
    if (!page->mainFrame())
        return 0;
    return page->mainFrame()->view();
}

void PageWidgetDelegate::animate(Page* page, double monotonicFrameBeginTime)
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    FrameView* view = mainFrameView(page);
    if (!view)
        return;
    view->serviceScriptedAnimations(monotonicFrameBeginTime);
#endif
}

void PageWidgetDelegate::layout(Page* page)
{
    FrameView* view = mainFrameView(page);
    if (!view)
        return;
    // In order for our child HWNDs (NativeWindowWidgets) to update properly,
    // they need to be told that we are updating the screen. The problem is that
    // the native widgets need to recalculate their clip region and not overlap
    // any of our non-native widgets. To force the resizing, call
    // setFrameRect(). This will be a quick operation for most frames, but the
    // NativeWindowWidgets will update a proper clipping region.
    view->setFrameRect(view->frameRect());

    // setFrameRect may have the side-effect of causing existing page layout to
    // be invalidated, so layout needs to be called last.
    view->updateLayoutAndStyleIfNeededRecursive();
}

void PageWidgetDelegate::paint(Page* page, PageOverlayList* overlays, WebCanvas* canvas, const WebRect& rect, CanvasBackground background, bool applyDeviceScale)
{
    if (rect.isEmpty())
        return;
    GraphicsContextBuilder builder(canvas);
    GraphicsContext& gc = builder.context();
    gc.setShouldSmoothFonts(background == Opaque);
    if (applyDeviceScale) {
        gc.applyDeviceScaleFactor(page->deviceScaleFactor());
        gc.platformContext()->setDeviceScaleFactor(page->deviceScaleFactor());
    }
    IntRect dirtyRect(rect);
    gc.save();
    FrameView* view = mainFrameView(page);
    // FIXME: Can we remove the mainFrame()->document() check?
    if (view && page->mainFrame()->document()) {
        gc.clip(dirtyRect);
        view->paint(&gc, dirtyRect);
        if (overlays)
            overlays->paintWebFrame(gc);
    } else
        gc.fillRect(dirtyRect, Color::white, ColorSpaceDeviceRGB);
    gc.restore();
}

bool PageWidgetDelegate::handleInputEvent(Page* page, PageWidgetEventHandler& handler, const WebInputEvent& event)
{
    Frame* frame = page ? page->mainFrame() : 0;
    switch (event.type) {

    // FIXME: WebKit seems to always return false on mouse events processing
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    case WebInputEvent::MouseMove:
        if (!frame || !frame->view())
            return true;
        handler.handleMouseMove(*frame, *static_cast<const WebMouseEvent*>(&event));
        return true;
    case WebInputEvent::MouseLeave:
        if (!frame || !frame->view())
            return true;
        handler.handleMouseLeave(*frame, *static_cast<const WebMouseEvent*>(&event));
        return true;
    case WebInputEvent::MouseDown:
        if (!frame || !frame->view())
            return true;
        handler.handleMouseDown(*frame, *static_cast<const WebMouseEvent*>(&event));
        return true;
    case WebInputEvent::MouseUp:
        if (!frame || !frame->view())
            return true;
        handler.handleMouseUp(*frame, *static_cast<const WebMouseEvent*>(&event));
        return true;

    case WebInputEvent::MouseWheel:
        if (!frame || !frame->view())
            return false;
        return handler.handleMouseWheel(*frame, *static_cast<const WebMouseWheelEvent*>(&event));

    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
        return handler.handleKeyEvent(*static_cast<const WebKeyboardEvent*>(&event));

    case WebInputEvent::Char:
        return handler.handleCharEvent(*static_cast<const WebKeyboardEvent*>(&event));

#if ENABLE(GESTURE_EVENTS)
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
        return handler.handleGestureEvent(*static_cast<const WebGestureEvent*>(&event));
#endif

#if ENABLE(TOUCH_EVENTS)
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
        if (!frame || !frame->view())
            return false;
        return handler.handleTouchEvent(*frame, *static_cast<const WebTouchEvent*>(&event));
#endif

#if ENABLE(GESTURE_EVENTS)
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
        // FIXME: Once PlatformGestureEvent is updated to support pinch, this
        // should call handleGestureEvent, just like it currently does for
        // gesture scroll.
        return false;
#endif

    default:
        return false;
    }
}

// ----------------------------------------------------------------
// Default handlers for PageWidgetEventHandler

void PageWidgetEventHandler::handleMouseMove(Frame& mainFrame, const WebMouseEvent& event)
{
    // We call mouseMoved here instead of handleMouseMovedEvent because we need
    // our ChromeClientImpl to receive changes to the mouse position and tooltip
    // text, and mouseMoved handles all of that.
    mainFrame.eventHandler()->mouseMoved(PlatformMouseEventBuilder(mainFrame.view(), event));
}

void PageWidgetEventHandler::handleMouseLeave(Frame& mainFrame, const WebMouseEvent& event)
{
    mainFrame.eventHandler()->handleMouseMoveEvent(PlatformMouseEventBuilder(mainFrame.view(), event));
}

void PageWidgetEventHandler::handleMouseDown(Frame& mainFrame, const WebMouseEvent& event)
{
    mainFrame.eventHandler()->handleMousePressEvent(PlatformMouseEventBuilder(mainFrame.view(), event));
}

void PageWidgetEventHandler::handleMouseUp(Frame& mainFrame, const WebMouseEvent& event)
{
    mainFrame.eventHandler()->handleMouseReleaseEvent(PlatformMouseEventBuilder(mainFrame.view(), event));
}

bool PageWidgetEventHandler::handleMouseWheel(Frame& mainFrame, const WebMouseWheelEvent& event)
{
    return mainFrame.eventHandler()->handleWheelEvent(PlatformWheelEventBuilder(mainFrame.view(), event));
}

#if ENABLE(TOUCH_EVENTS)
bool PageWidgetEventHandler::handleTouchEvent(Frame& mainFrame, const WebTouchEvent& event)
{
    return mainFrame.eventHandler()->handleTouchEvent(PlatformTouchEventBuilder(mainFrame.view(), event));
}
#endif

}
