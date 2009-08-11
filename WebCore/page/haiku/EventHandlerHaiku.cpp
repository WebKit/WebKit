/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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

#include "config.h"
#include "EventHandler.h"

#include "ClipboardHaiku.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformScrollBar.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"

#include <interface/View.h>


namespace WebCore {

const double EventHandler::TextDragDelay = 0.0;

static bool isKeyboardOptionTab(KeyboardEvent* event)
{
    return event
        && (event->type() == eventNames().keydownEvent
            || event->type() == eventNames().keypressEvent)
        && event->altKey()
        && event->keyIdentifier() == "U+000009";
}

bool EventHandler::invertSenseOfTabsToLinks(KeyboardEvent* event) const
{
    return isKeyboardOptionTab(event);
}

bool EventHandler::tabsToAllControls(KeyboardEvent* event) const
{
    bool handlingOptionTab = isKeyboardOptionTab(event);

    return handlingOptionTab;
}

void EventHandler::focusDocumentView()
{
    BView* view = m_frame->view()->platformWidget();
    if (view)
        view->MakeFocus();

    Page* page = m_frame->page();
    if (page)
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

bool EventHandler::passMouseDownEventToWidget(Widget* widget)
{
    notImplemented();
    return false;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent&) const
{
    notImplemented();
    return false;
}

bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults& event, Frame* subframe, HitTestResult*)
{
    notImplemented();
    return true;
}

bool EventHandler::passWheelEventToWidget(PlatformWheelEvent& event, Widget* widget)
{
    if (!widget->isFrameView())
        return false;

    return static_cast<FrameView*>(widget)->frame()->eventHandler()->handleWheelEvent(event);
}

PassRefPtr<Clipboard> EventHandler::createDraggingClipboard() const
{
    return ClipboardHaiku::create(ClipboardWritable, true);
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe, HitTestResult* hoveredNode)
{
    return passSubframeEventToSubframe(mev, subframe, hoveredNode);
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

unsigned EventHandler::accessKeyModifiers()
{
    return PlatformKeyboardEvent::AltKey;
}

} // namespace WebCore

