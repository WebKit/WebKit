/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "PointerLockManagerX11.h"

#if PLATFORM(X11)

#include "WebPageProxy.h"
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

PointerLockManagerX11::PointerLockManagerX11(WebPageProxy& webPage, const GdkEvent* event)
    : PointerLockManager(webPage, event)
{
}

void PointerLockManagerX11::didReceiveMotionEvent(const GdkEvent* event)
{
    double currentX, currentY;
    gdk_event_get_root_coords(event, &currentX, &currentY);
    double initialX, initialY;
    gdk_event_get_root_coords(m_event, &initialX, &initialY);
    if (currentX == initialX && currentY == initialY)
        return;

    handleMotion(IntPoint(currentX - initialX, currentY - initialY));
    gdk_device_warp(m_device, gtk_widget_get_screen(m_webPage.viewWidget()), initialX, initialY);
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
