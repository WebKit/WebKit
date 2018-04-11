/*
 * Copyright (C) 2006-2018 Apple Inc.  All rights reserved.
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

#if PLATFORM(MAC)

#import "FloatRect.h"
#import "FrameView.h"
#import "HostWindow.h"
#import "ScreenProperties.h"
#import <ColorSync/ColorSync.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/ProcessPrivilege.h>

extern "C" {
bool CGDisplayUsesInvertedPolarity(void);
bool CGDisplayUsesForceToGray(void);
}

namespace WebCore {

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.

static PlatformDisplayID displayID(NSScreen *screen)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
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
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    NSArray *screens = [NSScreen screens];
    if (![screens count])
        return nil;
    return [screens objectAtIndex:0];
}

static NSWindow *window(Widget* widget)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    if (!widget)
        return nil;
    return widget->platformWidget().window;
}

static NSScreen *screen(Widget* widget)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    // If the widget is in a window, use that, otherwise use the display ID from the host window.
    // First case is for when the NSWindow is in the same process, second case for when it's not.
    if (auto screenFromWindow = window(widget).screen)
        return screenFromWindow;
    return screen(displayID(widget));
}

static HashMap<PlatformDisplayID, ScreenProperties>& screenProperties()
{
    static NeverDestroyed<HashMap<PlatformDisplayID, ScreenProperties>> screenProperties;
    return screenProperties;
}

static PlatformDisplayID& primaryScreenDisplayID()
{
    static PlatformDisplayID primaryScreenDisplayID = 0;
    return primaryScreenDisplayID;
}

std::pair<PlatformDisplayID, HashMap<PlatformDisplayID, ScreenProperties>> getScreenProperties()
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    HashMap<PlatformDisplayID, ScreenProperties> screenProperties;
    std::optional<PlatformDisplayID> firstScreen;

    for (NSScreen *screen in [NSScreen screens]) {
        auto displayID = WebCore::displayID(screen);
        FloatRect screenAvailableRect = screen.visibleFrame;
        screenAvailableRect.setY(NSMaxY(screen.frame) - (screenAvailableRect.y() + screenAvailableRect.height())); // flip
        FloatRect screenRect = screen.frame;

        RetainPtr<CGColorSpaceRef> colorSpace = screen.colorSpace.CGColorSpace;

        int screenDepth = NSBitsPerPixelFromDepth(screen.depth);
        int screenDepthPerComponent = NSBitsPerSampleFromDepth(screen.depth);
        bool screenSupportsExtendedColor = [screen canRepresentDisplayGamut:NSDisplayGamutP3];
        bool screenHasInvertedColors = CGDisplayUsesInvertedPolarity();
        bool screenIsMonochrome = CGDisplayUsesForceToGray();

        screenProperties.set(displayID, ScreenProperties { screenAvailableRect, screenRect, colorSpace, screenDepth, screenDepthPerComponent, screenSupportsExtendedColor, screenHasInvertedColors, screenIsMonochrome });

        if (!firstScreen)
            firstScreen = displayID;
    }

    return { WTFMove(*firstScreen), WTFMove(screenProperties) };
}

void setScreenProperties(PlatformDisplayID primaryScreenID, const HashMap<PlatformDisplayID, ScreenProperties>& properties)
{
    primaryScreenDisplayID() = primaryScreenID;
    screenProperties() = properties;
}

ScreenProperties screenProperties(PlatformDisplayID screendisplayID)
{
    RELEASE_ASSERT(!screenProperties().isEmpty());

    // Return property of the first screen if the screen is not found in the map.
    auto displayID = screendisplayID ? screendisplayID : primaryScreenDisplayID();
    if (displayID) {
        auto screenPropertiesForDisplay = screenProperties().find(displayID);
        if (screenPropertiesForDisplay != screenProperties().end())
            return screenPropertiesForDisplay->value;
    }

    // Last resort: use the first item in the screen list.
    return screenProperties().begin()->value;
}

static ScreenProperties getScreenProperties(Widget* widget)
{
    return screenProperties(displayID(widget));
}

bool screenIsMonochrome(Widget* widget)
{
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).screenIsMonochrome;

    // This is a system-wide accessibility setting, same on all screens.
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    if (!screenProperties().isEmpty())
        return screenProperties(primaryScreenDisplayID()).screenHasInvertedColors;

    // This is a system-wide accessibility setting, same on all screens.
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return CGDisplayUsesInvertedPolarity();
}

int screenDepth(Widget* widget)
{
    if (!screenProperties().isEmpty()) {
        auto screenDepth = getScreenProperties(widget).screenDepth;
        ASSERT(screenDepth);
        return screenDepth;
    }

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerPixelFromDepth(screen(widget).depth);
}

int screenDepthPerComponent(Widget* widget)
{
    if (!screenProperties().isEmpty()) {
        auto depthPerComponent = getScreenProperties(widget).screenDepthPerComponent;
        ASSERT(depthPerComponent);
        return depthPerComponent;
    }

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerSampleFromDepth(screen(widget).depth);
}

FloatRect screenRectForDisplay(PlatformDisplayID displayID)
{
    if (!screenProperties().isEmpty()) {
        auto screenRect = screenProperties(displayID).screenRect;
        ASSERT(!screenRect.isEmpty());
        return screenRect;
    }

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return screen(displayID).frame;
}

FloatRect screenRect(Widget* widget)
{
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).screenRect;

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return toUserSpace([screen(widget) frame], window(widget));
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).screenAvailableRect;

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return toUserSpace([screen(widget) visibleFrame], window(widget));
}

NSScreen *screen(NSWindow *window)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [window screen] ?: firstScreen();
}

NSScreen *screen(PlatformDisplayID displayID)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    for (NSScreen *screen in [NSScreen screens]) {
        if (WebCore::displayID(screen) == displayID)
            return screen;
    }
    return firstScreen();
}

CGColorSpaceRef screenColorSpace(Widget* widget)
{
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).colorSpace.get();

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return screen(widget).colorSpace.CGColorSpace;
}

bool screenSupportsExtendedColor(Widget* widget)
{
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).screenSupportsExtendedColor;

    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [screen(widget) canRepresentDisplayGamut:NSDisplayGamutP3];
}

FloatRect toUserSpace(const NSRect& rect, NSWindow *destination)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY([screen(destination) frame]) - (userRect.y() + userRect.height())); // flip
    return userRect;
}

FloatRect toUserSpaceForPrimaryScreen(const NSRect& rect)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY(screenRectForDisplay(primaryScreenDisplayID())) - (userRect.y() + userRect.height())); // flip
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

#endif // PLATFORM(MAC)
