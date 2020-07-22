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
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/ProcessPrivilege.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

#if USE(MEDIATOOLBOX)
#import <pal/cocoa/MediaToolboxSoftLink.h>
#endif

namespace WebCore {

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.

PlatformDisplayID displayID(NSScreen *screen)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [[screen.deviceDescription objectForKey:@"NSScreenNumber"] intValue];
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

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
static DynamicRangeMode convertAVVideoRangeToEnum(NSString* range)
{
    if (!range)
        return DynamicRangeMode::None;
    if (PAL::canLoad_AVFoundation_AVVideoRangeSDR() && [range isEqualTo:PAL::get_AVFoundation_AVVideoRangeSDR()])
        return DynamicRangeMode::Standard;
    if (PAL::canLoad_AVFoundation_AVVideoRangeHLG() && [range isEqualTo:PAL::get_AVFoundation_AVVideoRangeHLG()])
        return DynamicRangeMode::HLG;
    if (PAL::canLoad_AVFoundation_AVVideoRangeHDR10() && [range isEqualTo:PAL::get_AVFoundation_AVVideoRangeHDR10()])
        return DynamicRangeMode::HDR10;
    if (PAL::canLoad_AVFoundation_AVVideoRangeDolbyVisionPQ() && [range isEqualTo:PAL::get_AVFoundation_AVVideoRangeDolbyVisionPQ()])
        return DynamicRangeMode::DolbyVisionPQ;

    ASSERT_NOT_REACHED();
    return DynamicRangeMode::None;
}
#endif

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
        bool screenSupportsHighDynamicRange = false;
        float scaleFactor = screen.backingScaleFactor;
        DynamicRangeMode dynamicRangeMode = DynamicRangeMode::None;

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
        if (PAL::isAVFoundationFrameworkAvailable() && [PAL::getAVPlayerClass() respondsToSelector:@selector(preferredVideoRangeForDisplays:)]) {
            dynamicRangeMode = convertAVVideoRangeToEnum([PAL::getAVPlayerClass() preferredVideoRangeForDisplays:@[ @(displayID) ]]);
            screenSupportsHighDynamicRange = dynamicRangeMode > DynamicRangeMode::Standard;
        }
#endif
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE) && USE(MEDIATOOLBOX)
        else
#endif
#if USE(MEDIATOOLBOX)
        if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_MTShouldPlayHDRVideo())
            screenSupportsHighDynamicRange = PAL::softLink_MediaToolbox_MTShouldPlayHDRVideo((__bridge CFArrayRef)@[ @(displayID) ]);
#endif

        if (!screenSupportsHighDynamicRange && dynamicRangeMode > DynamicRangeMode::Standard)
            dynamicRangeMode = DynamicRangeMode::Standard;

        if (displayMask)
            gpuID = gpuIDForDisplayMask(displayMask);

        screenProperties.screenDataMap.set(displayID, ScreenData { screenAvailableRect, screenRect, colorSpace, screenDepth, screenDepthPerComponent, screenSupportsExtendedColor, screenHasInvertedColors, screenSupportsHighDynamicRange, screenIsMonochrome, displayMask, gpuID, dynamicRangeMode, scaleFactor });

        if (!screenProperties.primaryDisplayID)
            screenProperties.primaryDisplayID = displayID;
    }

    return screenProperties;
}

void setShouldOverrideScreenSupportsHighDynamicRange(bool shouldOverride, bool supportsHighDynamicRange)
{
    if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_MTOverrideShouldPlayHDRVideo())
        PAL::softLink_MediaToolbox_MTOverrideShouldPlayHDRVideo(shouldOverride, supportsHighDynamicRange);
}

uint32_t primaryOpenGLDisplayMask()
{
    if (auto data = screenData(primaryScreenDisplayID()))
        return data->displayMask;

    return 0;
}

uint32_t displayMaskForDisplay(PlatformDisplayID displayID)
{
    if (auto data = screenData(displayID))
        return data->displayMask;

    ASSERT_NOT_REACHED();
    return 0;
}

IORegistryGPUID primaryGPUID()
{
    return gpuIDForDisplay(primaryScreenDisplayID());
}

IORegistryGPUID gpuIDForDisplay(PlatformDisplayID displayID)
{
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if (auto data = screenData(displayID))
        return data->gpuID;
    return 0;
#else
    return gpuIDForDisplayMask(CGDisplayIDToOpenGLDisplayMask(displayID));
#endif
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

static const ScreenData* screenProperties(Widget* widget)
{
    return screenData(displayID(widget));
}

bool screenIsMonochrome(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->screenIsMonochrome;

    // This is a system-wide accessibility setting, same on all screens.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    if (auto data = screenData(primaryScreenDisplayID()))
        return data->screenHasInvertedColors;

    // This is a system-wide accessibility setting, same on all screens.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldInvertColors];
}

int screenDepth(Widget* widget)
{
    if (auto data = screenProperties(widget)) {
        ASSERT(data->screenDepth);
        return data->screenDepth;
    }

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerPixelFromDepth(screen(widget).depth);
}

int screenDepthPerComponent(Widget* widget)
{
    if (auto data = screenProperties(widget)) {
        ASSERT(data->screenDepthPerComponent);
        return data->screenDepthPerComponent;
    }

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return NSBitsPerSampleFromDepth(screen(widget).depth);
}

FloatRect screenRectForDisplay(PlatformDisplayID displayID)
{
    if (auto data = screenData(displayID)) {
        ASSERT(!data->screenRect.isEmpty());
        return data->screenRect;
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
    if (auto data = screenProperties(widget))
        return data->screenRect;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return toUserSpace([screen(widget) frame], window(widget));
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->screenAvailableRect;

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
    if (auto data = screenProperties(widget))
        return data->colorSpace.get();

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return screen(widget).colorSpace.CGColorSpace;
}

bool screenSupportsExtendedColor(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->screenSupportsExtendedColor;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return [screen(widget) canRepresentDisplayGamut:NSDisplayGamutP3];
}

bool screenSupportsHighDynamicRange(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->screenSupportsHighDynamicRange;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
#if USE(MEDIATOOLBOX)
    if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_MTShouldPlayHDRVideo()) {
        auto displayID = WebCore::displayID(screen(widget));
        return PAL::softLink_MediaToolbox_MTShouldPlayHDRVideo((__bridge CFArrayRef)@[ @(displayID) ]);
    }
#endif
    return false;
}

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
DynamicRangeMode preferredDynamicRangeMode(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->preferredDynamicRangeMode;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    if (PAL::isAVFoundationFrameworkAvailable() && [PAL::getAVPlayerClass() respondsToSelector:@selector(preferredVideoRangeForDisplays:)]) {
        auto displayID = WebCore::displayID(screen(widget));
        return convertAVVideoRangeToEnum([PAL::getAVPlayerClass() preferredVideoRangeForDisplays:@[ @(displayID) ]]);
    }

    return DynamicRangeMode::Standard;
}
#endif

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
