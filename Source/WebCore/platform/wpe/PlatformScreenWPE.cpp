/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "PlatformScreen.h"

#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "HostWindow.h"
#include "LocalFrameView.h"
#include "NotImplemented.h"
#include "ScreenProperties.h"
#include "Widget.h"

namespace WebCore {

#if ENABLE(WPE_PLATFORM)
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
#endif

int screenDepth(Widget* widget)
{
#if ENABLE(WPE_PLATFORM)
    auto* data = screenData(widgetDisplayID(widget));
    return data ? data->screenDepth : 24;
#else
    UNUSED_PARAM(widget);
    notImplemented();
    return 24;
#endif
}

int screenDepthPerComponent(Widget* widget)
{
#if ENABLE(WPE_PLATFORM)
    auto* data = screenData(widgetDisplayID(widget));
    return data ? data->screenDepthPerComponent : 8;
#else
    UNUSED_PARAM(widget);
    notImplemented();
    return 8;
#endif
}

bool screenIsMonochrome(Widget* widget)
{
#if ENABLE(WPE_PLATFORM)
    return screenDepth(widget) < 2;
#else
    UNUSED_PARAM(widget);
    notImplemented();
    return false;
#endif
}

bool screenHasInvertedColors()
{
    return false;
}

double screenDPI(PlatformDisplayID screendisplayID)
{
#if ENABLE(WPE_PLATFORM)
    auto* data = screenData(screendisplayID);
    return data ? data->dpi : 96.;
#else
    UNUSED_PARAM(screendisplayID);
    notImplemented();
    return 96;
#endif
}

double fontDPI()
{
#if ENABLE(WPE_PLATFORM)
    // In WPE, there is no notion of font scaling separate from device DPI.
    return screenDPI(primaryScreenDisplayID());
#else
    notImplemented();
    return 96.;
#endif
}

FloatRect screenRect(Widget* widget)
{
#if ENABLE(WPE_PLATFORM)
    if (auto* data = screenData(widgetDisplayID(widget)))
        return data->screenRect;
#endif
    // WPE can't offer any more useful information about the screen size,
    // so we use the Widget's bounds rectangle (size of which equals the WPE view size).

    if (!widget)
        return { };
    return widget->boundsRect();
}

FloatRect screenAvailableRect(Widget* widget)
{
#if ENABLE(WPE_PLATFORM)
    if (auto* data = screenData(widgetDisplayID(widget)))
        return data->screenAvailableRect;
#endif
    return screenRect(widget);
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

#if ENABLE(TOUCH_EVENTS)
bool screenHasTouchDevice()
{
    return true;
}

bool screenIsTouchPrimaryInputDevice()
{
    return true;
}
#endif

} // namespace WebCore
