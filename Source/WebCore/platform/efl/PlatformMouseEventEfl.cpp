/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2008, 2009 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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

#include <Evas.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

void PlatformMouseEvent::setClickCount(unsigned int flags)
{
    if (flags & EVAS_BUTTON_TRIPLE_CLICK)
        m_clickCount = 3;
    else if (flags & EVAS_BUTTON_DOUBLE_CLICK)
        m_clickCount = 2;
    else
        m_clickCount = 1;
}

PlatformMouseEvent::PlatformMouseEvent(const Evas_Event_Mouse_Down* ev, IntPoint pos)
    : m_position(IntPoint(ev->canvas.x - pos.x(), ev->canvas.y - pos.y()))
    , m_globalPosition(IntPoint(ev->canvas.x, ev->canvas.y))
    , m_button(MouseButton(ev->button - 1))
    , m_eventType(MouseEventPressed)
    , m_shiftKey(evas_key_modifier_is_set(ev->modifiers, "Shift"))
    , m_ctrlKey(evas_key_modifier_is_set(ev->modifiers, "Control"))
    , m_altKey(evas_key_modifier_is_set(ev->modifiers, "Alt"))
    , m_metaKey(evas_key_modifier_is_set(ev->modifiers, "Meta"))
    , m_timestamp(currentTime())
{
    setClickCount(ev->flags);
}

PlatformMouseEvent::PlatformMouseEvent(const Evas_Event_Mouse_Up* ev, IntPoint pos)
    : m_position(IntPoint(ev->canvas.x - pos.x(), ev->canvas.y - pos.y()))
    , m_globalPosition(IntPoint(ev->canvas.x, ev->canvas.y))
    , m_button(MouseButton(ev->button - 1))
    , m_eventType(MouseEventReleased)
    , m_shiftKey(evas_key_modifier_is_set(ev->modifiers, "Shift"))
    , m_ctrlKey(evas_key_modifier_is_set(ev->modifiers, "Control"))
    , m_altKey(evas_key_modifier_is_set(ev->modifiers, "Alt"))
    , m_metaKey(evas_key_modifier_is_set(ev->modifiers, "Meta"))
    , m_timestamp(currentTime())
{
    setClickCount(ev->flags);
}

PlatformMouseEvent::PlatformMouseEvent(const Evas_Event_Mouse_Move* ev, IntPoint pos)
    : m_position(IntPoint(ev->cur.canvas.x - pos.x(), ev->cur.canvas.y - pos.y()))
    , m_globalPosition(IntPoint(ev->cur.canvas.x, ev->cur.canvas.y))
    , m_button(MouseButton(ev->buttons - 1))
    , m_eventType(MouseEventMoved)
    , m_clickCount(0)
    , m_shiftKey(evas_key_modifier_is_set(ev->modifiers, "Shift"))
    , m_ctrlKey(evas_key_modifier_is_set(ev->modifiers, "Control"))
    , m_altKey(evas_key_modifier_is_set(ev->modifiers, "Alt"))
    , m_metaKey(evas_key_modifier_is_set(ev->modifiers, "Meta"))
    , m_timestamp(currentTime())
{
}

}
