/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebScreenInfoFactory.h"

#include "WebScreenInfo.h"
#include <X11/Xlib.h>

namespace WebKit {

// FIXME: Take an X window and use XRandR to find the dimensions of the monitor
// that it's on (probably using XRRGetScreenInfo() and XRRConfigSizes() from
// X11/extensions/Xrandr.h). GDK provides a gdk_screen_get_monitor_geometry()
// function, but it appears to return stale data after the screen is resized.
WebScreenInfo WebScreenInfoFactory::screenInfo(Display* display, int screenNumber)
{
    // XDisplayWidth() and XDisplayHeight() return cached values. To ensure that
    // we return the correct dimensions after the screen is resized, query the
    // root window's geometry each time.
    Window root = RootWindow(display, screenNumber);
    Window rootRet;
    int x, y;
    unsigned int width, height, border, depth;
    XGetGeometry(
        display, root, &rootRet, &x, &y, &width, &height, &border, &depth);

    WebScreenInfo results;

    // FIXME: Initialize the device scale factor.
    // FIXME: Not all screens use 8bpp.
    results.depthPerComponent = 8;
    results.depth = depth;
    results.isMonochrome = depth == 1;
    results.rect = WebRect(x, y, width, height);
    // FIXME: Query the _NET_WORKAREA property from EWMH.
    results.availableRect = results.rect;

    return results;
}

} // namespace WebKit
