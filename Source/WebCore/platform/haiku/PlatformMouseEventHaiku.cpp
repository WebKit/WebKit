/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include <View.h>

namespace WebCore {

PlatformMouseEvent::PlatformMouseEvent(const BMessage* message)
    : m_position(message->FindPoint("be:view_where"))
    , m_globalPosition(message->FindPoint("screen_where"))
    , m_clickCount(message->FindInt32("clicks"))
{
    m_timestamp = WallTime::fromRawSeconds(message->FindInt64("when") / 1000000.0);

    int32 buttons = 0;
    if (message->what == B_MOUSE_UP)
        message->FindInt32("previous buttons", &buttons);
    else
        message->FindInt32("buttons", &buttons);

    if (buttons & B_PRIMARY_MOUSE_BUTTON)
        m_button = LeftButton;
    else if (buttons & B_SECONDARY_MOUSE_BUTTON)
        m_button = RightButton;
    else if (buttons & B_TERTIARY_MOUSE_BUTTON)
        m_button = MiddleButton;
    else
        m_button = NoButton;

    switch (message->what) {
    case B_MOUSE_DOWN:
        m_type = PlatformEvent::MousePressed;
        break;
    case B_MOUSE_UP:
        m_type = PlatformEvent::MouseReleased;
        break;
    case B_MOUSE_MOVED:
    default:
        m_type = PlatformEvent::MouseMoved;
        break;
    };

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);
}

} // namespace WebCore

