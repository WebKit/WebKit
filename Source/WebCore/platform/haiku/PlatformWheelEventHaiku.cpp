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
#include "PlatformWheelEvent.h"

#include "Scrollbar.h"

#include <InterfaceDefs.h>
#include <Message.h>
#include <Point.h>


namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(BMessage* message)
    : m_position(message->FindPoint("be:view_where"))
    , m_globalPosition(message->FindPoint("screen_where"))
    , m_deltaX(message->FindFloat("be:wheel_delta_x"))
    , m_deltaY(message->FindFloat("be:wheel_delta_y"))
    , m_wheelTicksX(m_deltaX)
    , m_wheelTicksY(m_deltaY)
    , m_granularity(ScrollByPixelWheelEvent)
    , m_isAccepted(false)
{
    m_deltaX *= -Scrollbar::pixelsPerLineStep();
    m_deltaY *= -Scrollbar::pixelsPerLineStep();

    int32 modifiers = message->FindInt32("modifiers");
    m_shiftKey = modifiers & B_SHIFT_KEY;
    m_ctrlKey = modifiers & B_CONTROL_KEY;
    m_altKey = modifiers & B_COMMAND_KEY;
    m_metaKey = modifiers & B_OPTION_KEY;
}

} // namespace WebCore

