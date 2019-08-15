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

PlatformDisplayID displayID(NSScreen *screen)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
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
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    NSArray *screens = [NSScreen screens];
    if (![screens count])
        return nil;
    return [screens objectAtIndex:0];
}

static NSWindow *window(Widget* widget)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    if (!widget)
        return nil;
    return widget->platformWidget().window;
}

static NSScreen *screen(Widget* widget)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    // If the widget is in a window, use that, otherwise use the display ID from the host window.
    // First case is for when the NSWindow is in the same process, second case for when it's not.
    if (auto screenFromWindow = window(widget).screen)
        return screenFromWindow;
    return screen(displayID(widget));
}

static ScreenProperties& screenProperties()
{
    static NeverDestroyed<ScreenProperties> screenProperties;
    return screenProperties;
}

PlatformDisplayID primaryScreenDisplayID()
{
    return screenProperties().primaryDisplayID;
}

ScreenProperties collectScreenProperties()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    ScreenProperties screenProperties;
    bool screenHasInvertedColors = [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldInvertColors];

    for (NSScreen *screen in [NSScreen screens]) {
        auto displayID = WebCore::displayID(screen);
        FloatRect screenAvailableRect = screen.visibleFrame;
        screenAvailableRect.setY(NSMaxY(screen.frame) - (screenAvailableRect.y() + screenAvailableRect.height())); // flip
        FloatRect screenRect = screen.frame;

        RetainPtr<CGColorSpaceRef> colorSpace = screen.colorSpace.CGColorSpace;

        int screenDepth = NSBitsPerPixelFromDepth(screen.depth);
        int screenDepthPerComponent = NSBitsPerSampleFromDepth(screen.depth);
        bool screenSupportsExtendedColor = [screen canRepresentDisplayGamut:NSDisplayGamutP3];
        bool screenIsMonochrome = CGDisplayUsesForceToGray();
        uint32_t displayMask = CGDisplayIDToOpenGLDisplayMask(displayID);
        IORegistryGPUID gpuID = 0;

        if (displayMask)
            gpuID = gpuIDForDisplayMask(displayMask);

        screenProperties.screenDataMap.set(displayID, ScreenData { screenAvailableRect, screenRect, colorSpace, screenDepth, screenDepthPerComponent, screenSupportsExtendedColor, screenHasInvertedColors, screenIsMonochrome, displayMask, gpuID });

        if (!screenProperties.primaryDisplayID)
            screenProperties.primaryDisplayID = displayID;
    }

    return screenProperties;
}

void setScreenProperties(const ScreenProperties& properties)
{
    screenProperties() = properties;
}

static ScreenData screenData(PlatformDisplayID screendisplayID)
{
    RELEASE_ASSERT(!screenProperties().screenDataMap.isEmpty());

    // Return property of the first screen if the screen is not found in the map.
    auto displayID = screendisplayID ? screendisplayID : primaryScreenDisplayID();
    if (displayID) {
        auto screenPropertiesForDisplay = screenProperties().screenDataMap.find(displayID);
        if (screenPropertiesForDisplay != screenProperties().screenDataMap.end())
            return screenPropertiesForDisplay->value;
    }

    // Last resort: use the first item in the screen list.
    return screenProperties().screenDataMap.begin()->value;
}

uint32_t primaryOpenGLDisplayMask()
{
    if (!screenProperties().screenDataMap.isEmpty())
        return screenData(primaryScreenDisplayID()).displayMask;
    
    return 0;
}

uint32_t displayMaskForDisplay(PlatformDisplayID displayID)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return screenData(displayID).displayMask;
    
    ASSERT_NOT_REACHED();
    return 0;
}

IORegistryGPUID primaryGPUID()
{
    return gpuIDForDisplay(screenProperties().primaryDisplayID);
}

IORegistryGPUID gpuIDForDisplay(PlatformDisplayID displayID)
{
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if (!screenProperties().screenDataMap.isEmpty())
        return screenData(displayID).gpuID;
#else
    return gpuIDForDisplayMask(CGDisplayIDToOpenGLDisplayMask(displayID));
#endif
    return 0;
}

IORegistryGPUID gpuIDForDisplayMask(GLuint displayMask)
{
    GLint numRenderers = 0;
    CGLRendererInfoObj rendererInfo = nullptr;
    CGLError error = CGLQueryRendererInfo(displayMask, &rendererInfo, &numRenderers);
    if (!numRenderers || !rendererInfo || error != kCGLNoError)
        return 0;

    // The 0th renderer should not be the software renderer.
    GLint isAccelerated;
    error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPAccelerated, &isAccelerated);
    if (!isAccelerated || error != kCGLNoError) {
        CGLDestroyRendererInfo(rendererInfo);
        return 0;
    }

    GLint gpuIDLow = 0;
    GLint gpuIDHigh = 0;

    error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPRegistryIDLow, &gpuIDLow);
    if (error != kCGLNoError) {
        CGLDestroyRendererInfo(rendererInfo);
        return 0;
    }

    error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPRegistryIDHigh, &gpuIDHigh);
    if (error != kCGLNoError) {
        CGLDestroyRendererInfo(rendererInfo);
        return 0;
    }

    CGLDestroyRendererInfo(rendererInfo);
    return (IORegistryGPUID) gpuIDHigh << 32 | gpuIDLow;
}

static ScreenData getScreenProperties(Widget* widget)
{
    return screenData(displayID(widget));
}

bool screenIsMonochrome(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return getScreenProperties(widget).screenIsMonochrome;

    // This is a system-wide accessibility setting, same on all screens.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    if (!screenProperties().screenDataMap.isEmpty())
        return screenData(primaryScreenDisplayID()).screenHasInvertedColors;

    // This is a system-wide accessibility setting, same on all screens.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldInvertColors];
}

int screenDepth(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty()) {
        auto screenDepth = getScreenProperties(widget).screenDepth;
        ASSERT(screenDepth);
        return screenDepth;
    }

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerPixelFromDepth(screen(widget).depth);
}

int screenDepthPerComponent(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty()) {
        auto depthPerComponent = getScreenProperties(widget).screenDepthPerComponent;
        ASSERT(depthPerComponent);
        return depthPerComponent;
    }

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerSampleFromDepth(screen(widget).depth);
}

FloatRect screenRectForDisplay(PlatformDisplayID displayID)
{
    if (!screenProperties().screenDataMap.isEmpty()) {
        auto screenRect = screenData(displayID).screenRect;
        ASSERT(!screenRect.isEmpty());
        return screenRect;
    }

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return screen(displayID).frame;
}

FloatRect screenRectForPrimaryScreen()
{
    return screenRectForDisplay(primaryScreenDisplayID());
}

FloatRect screenRect(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return getScreenProperties(widget).screenRect;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return toUserSpace([screen(widget) frame], window(widget));
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return getScreenProperties(widget).screenAvailableRect;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return toUserSpace([screen(widget) visibleFrame], window(widget));
}

NSScreen *screen(NSWindow *window)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [window screen] ?: firstScreen();
}

NSScreen *screen(PlatformDisplayID displayID)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    for (NSScreen *screen in [NSScreen screens]) {
        if (WebCore::displayID(screen) == displayID)
            return screen;
    }
    return firstScreen();
}

CGColorSpaceRef screenColorSpace(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return getScreenProperties(widget).colorSpace.get();

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return screen(widget).colorSpace.CGColorSpace;
}

bool screenSupportsExtendedColor(Widget* widget)
{
    if (!screenProperties().screenDataMap.isEmpty())
        return getScreenProperties(widget).screenSupportsExtendedColor;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
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
