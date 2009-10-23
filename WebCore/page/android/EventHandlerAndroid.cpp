/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#define LOG_TAG "WebCore"

#include "config.h"
#include "EventHandler.h"

#include "FocusController.h"
#include "Frame.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"

namespace WebCore {

unsigned EventHandler::s_accessKeyModifiers = PlatformKeyboardEvent::AltKey;

bool EventHandler::tabsToAllControls(KeyboardEvent*) const
{
    return true;
}

void EventHandler::focusDocumentView()
{
    if (Page* page = m_frame->page())
        page->focusController()->setFocusedFrame(m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    // Figure out which view to send the event to.
    RenderObject* target = event.targetNode() ? event.targetNode()->renderer() : 0;
    if (!target || !target->isWidget())
        return false;
    return passMouseDownEventToWidget(toRenderWidget(target)->widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

// This function is used to route the mouse down event to the native widgets, it seems like a
// work around for the Mac platform which does not support double clicks, but browsers do.
bool EventHandler::passMouseDownEventToWidget(Widget*)
{
    // return false so the normal propogation handles the event
    return false;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent&) const
{
    notImplemented();
    return false;
}

// This function is called for mouse events by FrameView::handleMousePressEvent().
// It is used to ensure that events are sync'ed correctly between frames. For example
// if the user presses down in one frame and up in another frame, this function will
// returns true, and pass the event to the correct frame.
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame*, HitTestResult*)
{
    notImplemented();
    return false;
}

// This is called to route wheel events to child widgets when they are RenderWidget
// as the parent usually gets wheel event. Don't have a mouse with a wheel to confirm
// the operation of this function.
bool EventHandler::passWheelEventToWidget(PlatformWheelEvent&, Widget*)
{
    notImplemented();
    return false;
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, 
    Frame* subframe, HitTestResult*)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

class Clipboard : public RefCounted<Clipboard> {
};

PassRefPtr<Clipboard> EventHandler::createDraggingClipboard() const
{
    return PassRefPtr<Clipboard>(0);
}

const double EventHandler::TextDragDelay = 0.0;

}  // namespace WebCore
