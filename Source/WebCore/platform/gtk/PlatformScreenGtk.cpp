/*
 * Copyright (C) 2006-2021 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Holger Hans Peter Freyther
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformScreen.h"

#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "HostWindow.h"
#include "LocalFrameView.h"
#include "ScreenProperties.h"
#include "Widget.h"
#include <gtk/gtk.h>

namespace WebCore {

static PlatformDisplayID widgetDisplayID(Widget* widget)
{
    if (!widget)
        return 0;

    auto* view = widget->root();
    if (!view)
        return 0;

    auto* hostWindow = view->hostWindow();
    if (!hostWindow)
        return 0;

    return hostWindow->displayID();
}

int screenDepth(Widget* widget)
{
    auto* data = screenData(widgetDisplayID(widget));
    return data ? data->screenDepth : 24;
}

int screenDepthPerComponent(Widget* widget)
{
    auto* data = screenData(widgetDisplayID(widget));
    return data ? data->screenDepthPerComponent : 8;
}

bool screenIsMonochrome(Widget* widget)
{
    return screenDepth(widget) < 2;
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

bool screenHasInvertedColors()
{
    return false;
}

double screenDPI()
{
    static GtkSettings* gtkSettings = gtk_settings_get_default();
    if (gtkSettings) {
        int gtkXftDpi;
        g_object_get(gtkSettings, "gtk-xft-dpi", &gtkXftDpi, nullptr);
        return gtkXftDpi / 1024.0;
    }

    auto* data = screenData(primaryScreenDisplayID());
    return data ? data->dpi : 96.;
}

FloatRect screenRect(Widget* widget)
{
    if (auto* data = screenData(widgetDisplayID(widget)))
        return data->screenRect;
    return { };
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (auto* data = screenData(widgetDisplayID(widget)))
        return data->screenAvailableRect;
    return { };
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

#if ENABLE(TOUCH_EVENTS)
bool screenHasTouchDevice()
{
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

    auto* seat = gdk_display_get_default_seat(display);
    return seat ? gdk_seat_get_capabilities(seat) & GDK_SEAT_CAPABILITY_TOUCH : true;
}

bool screenIsTouchPrimaryInputDevice()
{
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

    auto* seat = gdk_display_get_default_seat(display);
    if (!seat)
        return true;

    auto* device = gdk_seat_get_pointer(seat);
    return device ? gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN : true;
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebCore
