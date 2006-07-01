/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

#include <gdk/gdk.h>

namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(GdkEvent* event)
{
    if (event->scroll.direction == GDK_SCROLL_UP) {
        m_delta = -120;
        m_isHorizontal = false;
    } else if (event->scroll.direction == GDK_SCROLL_LEFT) {
        m_delta = -120;
        m_isHorizontal = true;
    } else if (event->scroll.direction == GDK_SCROLL_RIGHT) {
        m_delta = 120;
        m_isHorizontal = true;
    } else {
        m_delta = 120;
        m_isHorizontal = false;
    }

    m_position = IntPoint((int)event->scroll.x, (int)event->scroll.y);
    m_globalPosition = IntPoint((int)event->scroll.x_root, (int)event->scroll.y_root);
    m_isAccepted = false;
    m_shiftKey = event->button.state & GDK_SHIFT_MASK;
    m_ctrlKey = event->button.state & GDK_CONTROL_MASK;
    m_altKey = event->button.state & GDK_MOD1_MASK;
    m_metaKey = event->button.state & GDK_MOD2_MASK;
}

}
