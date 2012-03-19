/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include <wtf/Assertions.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

static PlatformEvent::Type typeFromMouseEvent(const wxMouseEvent& event)
{
    wxEventType type = event.GetEventType();

    if (type == wxEVT_LEFT_DOWN || type == wxEVT_MIDDLE_DOWN || type == wxEVT_RIGHT_DOWN)
        return PlatformEvent::MousePressed;
    if (type == wxEVT_LEFT_UP || type == wxEVT_MIDDLE_UP || type == wxEVT_RIGHT_UP || type == wxEVT_LEFT_DCLICK || type == wxEVT_MIDDLE_DCLICK || type == wxEVT_RIGHT_DCLICK)
        return PlatformEvent::MouseReleased;
    if (type == wxEVT_MOTION)
        return PlatformEvent::MouseMoved;

    return PlatformEvent::MouseMoved;
}

PlatformMouseEvent::PlatformMouseEvent(const wxMouseEvent& event, const wxPoint& globalPoint, int clickCount)
    : PlatformEvent(typeFromMouseEvent(event), event.ShiftDown(), event.CmdDown() || event.ControlDown(), event.AltDown(), event.MetaDown(), WTF::currentTime())
    , m_position(event.GetPosition())
    , m_globalPosition(globalPoint)
{

    if (event.LeftIsDown() || event.Button(wxMOUSE_BTN_LEFT))
        m_button = LeftButton;
    else if (event.RightIsDown() || event.Button(wxMOUSE_BTN_RIGHT))
        m_button = RightButton;
    else if (event.MiddleIsDown() || event.Button(wxMOUSE_BTN_MIDDLE))
        m_button = MiddleButton;

    if (m_type == PlatformEvent::MouseMoved)
        m_clickCount = 0;
    else
        m_clickCount = clickCount;
}

}
