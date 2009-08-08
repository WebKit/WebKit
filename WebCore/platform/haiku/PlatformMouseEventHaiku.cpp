/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 *
 * All rights reserved.
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
#include "PlatformMouseEvent.h"

#include <Message.h>
#include <SupportDefs.h>


namespace WebCore {

PlatformMouseEvent::PlatformMouseEvent(const BMessage* message)
    : m_timestamp(message->FindInt64("when"))
    , m_position(IntPoint(message->FindPoint("where")))
    , m_globalPosition(message->FindPoint("globalPosition"))
    , m_shiftKey(false)
    , m_ctrlKey(false)
    , m_altKey(false)
    , m_metaKey(false)
    , m_clickCount(message->FindInt32("clicks"))
{
    int32 buttons = message->FindInt32("buttons");
    switch (buttons) {
    case 1:
        m_button = LeftButton;
        break;
    case 2:
        m_button = RightButton;
        break;
    case 3:
        m_button = MiddleButton;
        break;
    default:
        m_button = NoButton;
        break;
    };

    switch (message->what) {
    case B_MOUSE_DOWN:
        m_eventType = MouseEventPressed;
        break;
    case B_MOUSE_UP:
        m_eventType = MouseEventReleased;
        m_button = LeftButton; // FIXME: Webcore wants to know the button released but we don't know.
        break;
    case B_MOUSE_MOVED:
        m_eventType = MouseEventMoved;
        break;
    default:
        m_eventType = MouseEventMoved;
        break;
    };
}

} // namespace WebCore

