/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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
#include "WebExtensionUtilities.h"

#include <gdk/gdk.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

Vector<double> availableScreenScales()
{
    Vector<double> screenScales;

#if USE(GTK4)
    auto* display = gdk_display_get_default();
    auto* monitors = gdk_display_get_monitors(display);
    unsigned currentMonitor = 0;
    while (auto* item = g_list_model_get_item(monitors, currentMonitor++)) {
        auto* monitor = GDK_MONITOR(item);
        screenScales.append(gdk_monitor_get_scale_factor(monitor));
    }
#endif

    if (!screenScales.isEmpty())
        return screenScales;

    // Assume 1x if we got no results. This can happen on headless devices (bots).
    return { 1.0 };
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
