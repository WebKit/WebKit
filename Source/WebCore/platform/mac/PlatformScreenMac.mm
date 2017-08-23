/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "PlatformScreen.h"

#import "FloatRect.h"
#import "FrameView.h"
#import "HostWindow.h"
#import <ColorSync/ColorSync.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>

extern "C" {
bool CGDisplayUsesInvertedPolarity(void);
bool CGDisplayUsesForceToGray(void);
}

namespace WebCore {

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.

static PlatformDisplayID displayID(NSScreen *screen)
{
    return [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
}

static PlatformDisplayID displayID(Widget* widget)
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

// Screen containing the menubar.
static NSScreen *firstScreen()
{
    NSArray *screens = [NSScreen screens];
    if (![screens count])
        return nil;
    return [screens objectAtIndex:0];
}

static NSWindow *window(Widget* widget)
{
    if (!widget)
        return nil;
    return widget->platformWidget().window;
}

static NSScreen *screen(Widget* widget)
{
    // If the widget is in a window, use that, otherwise use the display ID from the host window.
    // First case is for when the NSWindow is in the same process, second case for when it's not.
    if (auto screenFromWindow = window(widget).screen)
        return screenFromWindow;
    return screen(displayID(widget));
}

int screenDepth(Widget* widget)
{
    return NSBitsPerPixelFromDepth(screen(widget).depth);
}

int screenDepthPerComponent(Widget* widget)
{
    return NSBitsPerSampleFromDepth(screen(widget).depth);
}

bool screenIsMonochrome(Widget*)
{
    // This is a system-wide accessibility setting, same on all screens.
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    // This is a system-wide accessibility setting, same on all screens.
    return CGDisplayUsesInvertedPolarity();
}

FloatRect screenRect(Widget* widget)
{
    return toUserSpace([screen(widget) frame], window(widget));
}

FloatRect screenAvailableRect(Widget* widget)
{
    return toUserSpace([screen(widget) visibleFrame], window(widget));
}

NSScreen *screen(NSWindow *window)
{
    return [window screen] ?: firstScreen();
}

NSScreen *screen(PlatformDisplayID displayID)
{
    for (NSScreen *screen in [NSScreen screens]) {
        if (WebCore::displayID(screen) == displayID)
            return screen;
    }
    return firstScreen();
}

CGColorSpaceRef screenColorSpace(Widget* widget)
{
    return screen(widget).colorSpace.CGColorSpace;
}

bool screenSupportsExtendedColor(Widget* widget)
{
    if (!widget)
        return false;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    return [screen(widget) canRepresentDisplayGamut:NSDisplayGamutP3];
#else
    auto colorSpace = screenColorSpace(widget);
    auto iccData = adoptCF(CGColorSpaceCopyICCProfile(colorSpace));
    auto profile = adoptCF(ColorSyncProfileCreate(iccData.get(), nullptr));
    return profile && ColorSyncProfileIsWideGamut(profile.get());
#endif
}

FloatRect toUserSpace(const NSRect& rect, NSWindow *destination)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY([screen(destination) frame]) - (userRect.y() + userRect.height())); // flip
    return userRect;
}

NSRect toDeviceSpace(const FloatRect& rect, NSWindow *source)
{
    FloatRect deviceRect = rect;
    deviceRect.setY(NSMaxY([screen(source) frame]) - (deviceRect.y() + deviceRect.height())); // flip
    return deviceRect;
}

NSPoint flipScreenPoint(const NSPoint& screenPoint, NSScreen *screen)
{
    NSPoint flippedPoint = screenPoint;
    flippedPoint.y = NSMaxY([screen frame]) - flippedPoint.y;
    return flippedPoint;
}

} // namespace WebCore
