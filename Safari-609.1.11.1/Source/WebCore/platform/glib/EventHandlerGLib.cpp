/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2014-2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"

#if ENABLE(DRAG_SUPPORT)
#include "DataTransfer.h"
#endif

#if PLATFORM(GTK)
#include "Scrollbar.h"
#endif

namespace WebCore {

// GTK+ must scroll horizontally if the mouse pointer is on top of the
// horizontal scrollbar while scrolling with the wheel; we need to
// add the deltas and ticks here so that this behavior is consistent
// for styled scrollbars.
bool EventHandler::shouldSwapScrollDirection(const HitTestResult& result, const PlatformWheelEvent& event) const
{
#if PLATFORM(GTK)
    FrameView* view = m_frame.view();
    Scrollbar* scrollbar = view ? view->scrollbarAtPoint(event.position()) : nullptr;
    if (!scrollbar)
        scrollbar = result.scrollbar();
    if (!scrollbar)
        return false;

    // The directions are already swapped when shift key is pressed, but when scrolling
    // over scrollbars we always want to follow the scrollbar direction.
    return scrollbar->orientation() == HorizontalScrollbar ? !event.shiftKey() : event.shiftKey();
#else
    UNUSED_PARAM(result);
    UNUSED_PARAM(event);
    notImplemented();
    return false;
#endif
}

}
