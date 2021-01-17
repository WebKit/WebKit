/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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

#include "DataTransfer.h"
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
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"

#include <interface/View.h>


namespace WebCore {

const WTF::Seconds EventHandler::TextDragDelay = WTF::Seconds(0.0);

bool EventHandler::tabsToAllFormControls(KeyboardEvent*) const
{
    return true;
}

void EventHandler::focusDocumentView()
{
    BView* view = m_frame.view()->platformWidget();
    if (view && view->LockLooperWithTimeout(5000) == B_OK) {
        view->MakeFocus(true);
        view->UnlockLooper();
    }

    Page* page = m_frame.page();
    if (page)
        page->focusController().setFocusedFrame(&m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    // Figure out which view to send the event to.
    RenderObject* target = event.targetNode() ? event.targetNode()->renderer() : 0;
    if (!target || !target->isWidget())
        return false;
    return passMouseDownEventToWidget(downcast<RenderWidget>(target)->widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

bool EventHandler::passMouseDownEventToWidget(Widget*)
{
    notImplemented();
    return false;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent&) const
{
    // On Haiku, clicks which activate the window in non focus-follows-mouse mode
    // are not passed to the window, so any event we generate is not the activation
    // event.
    return false;
}

bool EventHandler::passWheelEventToWidget(const PlatformWheelEvent& event, Widget& widget, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    if (!widget.isFrameView())
        return false;

    return static_cast<FrameView&>(widget).frame().eventHandler().handleWheelEvent(event, processingSteps);
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame& subframe)
{
    subframe.eventHandler().handleMousePressEvent(mev.event());
    return true;
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame& subframe, HitTestResult* hoveredNode)
{
    subframe.eventHandler().handleMouseMoveEvent(mev.event(), hoveredNode);
    return true;
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame& subframe)
{
    subframe.eventHandler().handleMouseReleaseEvent(mev.event());
    return true;
}

WTF::OptionSet<WebCore::PlatformEvent::Modifier> EventHandler::accessKeyModifiers()
{
    // NOTE: On Haiku, the user can choose Alt or Ctrl as access key, but
    // the PlatformKeyboardEvent already takes care of this, internally,
    // we always use Alt.
    return PlatformEvent::Modifier::AltKey;
}

} // namespace WebCore

