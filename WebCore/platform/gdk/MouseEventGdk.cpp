/*
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
#include "PlatformMouseEvent.h"

#include <assert.h>
#include <gdk/gdk.h>

namespace WebCore {

const PlatformMouseEvent::CurrentEventTag PlatformMouseEvent::currentEvent = {};

// FIXME: Would be even better to figure out which modifier is Alt instead of always using GDK_MOD1_MASK.

PlatformMouseEvent::PlatformMouseEvent(GdkEvent* event)
{
    switch (event->type) {
        case GDK_MOTION_NOTIFY:
            m_position = IntPoint((int)event->motion.x, (int)event->motion.y);
            m_globalPosition = IntPoint((int)event->motion.x_root, (int)event->motion.y_root);
            m_button = (MouseButton)(-1);
            m_clickCount = 0;
            m_shiftKey =  event->motion.state & GDK_SHIFT_MASK != 0;
            m_ctrlKey = event->motion.state & GDK_CONTROL_MASK != 0;
            m_altKey =  event->motion.state & GDK_MOD1_MASK != 0;
            m_metaKey = event->motion.state & GDK_MOD2_MASK != 0;
            break;

        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
            if (event->type == GDK_BUTTON_PRESS)
                m_clickCount = 1;
            else if (event->type == GDK_2BUTTON_PRESS)
                m_clickCount = 2;
            else if (event->type == GDK_3BUTTON_PRESS)
                m_clickCount = 3;

            if (event->button.button == 1)
                m_button = LeftButton;
            else if (event->button.button == 2)
                m_button = MiddleButton;
            else if (event->button.button == 3)
                m_button = RightButton;

            m_position = IntPoint((int)event->button.x, (int)event->button.y);
            m_globalPosition = IntPoint((int)event->button.x_root, (int)event->button.y_root);
            m_shiftKey = event->button.state & GDK_SHIFT_MASK != 0;
            m_ctrlKey = event->button.state & GDK_CONTROL_MASK != 0;
            m_altKey = event->button.state & GDK_MOD1_MASK != 0;
            m_metaKey = event->button.state & GDK_MOD2_MASK != 0;
            break;

        default:
            fprintf(stderr, "Unknown mouse event\n");
            assert(0);
    }
}

}
