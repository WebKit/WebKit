/*
 * Copyright (C) 2013 Carlos Garnacho <carlosg@gnome.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GtkTouchContextHelper.h"

#include <gtk/gtk.h>

namespace WebCore {

bool GtkTouchContextHelper::handleEvent(GdkEvent* touchEvent)
{
#ifndef GTK_API_VERSION_2
    uint32_t sequence = GPOINTER_TO_UINT(gdk_event_get_event_sequence(touchEvent));

    switch (touchEvent->type) {
    case GDK_TOUCH_BEGIN: {
        ASSERT(!m_touchEvents.contains(sequence));
        GUniquePtr<GdkEvent> event(gdk_event_copy(touchEvent));
        m_touchEvents.add(sequence, WTF::move(event));
        break;
    }
    case GDK_TOUCH_UPDATE: {
        auto it = m_touchEvents.find(sequence);
        ASSERT(it != m_touchEvents.end());
        it->value.reset(gdk_event_copy(touchEvent));
        break;
    }
    case GDK_TOUCH_END:
        ASSERT(m_touchEvents.contains(sequence));
        m_touchEvents.remove(sequence);
        break;
    default:
        return false;
    }
#else
    UNUSED_PARAM(touchEvent);
#endif // GTK_API_VERSION_2

    return true;
}

} // namespace WebCore
