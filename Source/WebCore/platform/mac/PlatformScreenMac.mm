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

#import "ContentsFormat.h"
#import "FloatRect.h"
#import "HostWindow.h"
#import "LocalFrameView.h"
#import "PlatformCALayerClient.h"
#import "ScreenProperties.h"
#import "ThermalMitigationNotifier.h"
#import <ColorSync/ColorSync.h>
#import <pal/cocoa/OpenGLSoftLinkCocoa.h>
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

    auto screenSupportsHighDynamicRange = [](PlatformDisplayID displayID, DynamicRangeMode& dynamicRangeMode) {
        bool supportsHighDynamicRange = false;
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
        if (PAL::isAVFoundationFrameworkAvailable()) {
            dynamicRangeMode = convertAVVideoRangeToEnum([PAL::getAVPlayerClass() preferredVideoRangeForDisplays:@[ @(displayID) ]]);
            supportsHighDynamicRange = dynamicRangeMode > DynamicRangeMode::Standard;
        }
#endif
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE) && USE(MEDIATOOLBOX)
        else
#endif
#if USE(MEDIATOOLBOX)
        if (PAL::isMediaToolboxFrameworkAvailable() && PAL::canLoad_MediaToolbox_MTShouldPlayHDRVideo())
            supportsHighDynamicRange = PAL::softLink_MediaToolbox_MTShouldPlayHDRVideo((__bridge CFArrayRef)@[ @(displayID) ]);
#endif

        if (!supportsHighDynamicRange && dynamicRangeMode > DynamicRangeMode::Standard)
            dynamicRangeMode = DynamicRangeMode::Standard;

        if (supportsHighDynamicRange && WebCore::ThermalMitigationNotifier::isThermalMitigationEnabled())
            supportsHighDynamicRange = false;

        return supportsHighDynamicRange;
    };

    for (NSScreen *screen in [NSScreen screens]) {
        ScreenData screenData;
        auto displayID = WebCore::displayID(screen);

        auto screenAvailableRect = FloatRect { screen.visibleFrame };
        screenAvailableRect.setY(NSMaxY(screen.frame) - (screenAvailableRect.y() + screenAvailableRect.height())); // flip
        screenData.screenAvailableRect = screenAvailableRect;

        screenData.screenRect = screen.frame;
        screenData.colorSpace = DestinationColorSpace { screen.colorSpace.CGColorSpace };
        screenData.screenDepth = NSBitsPerPixelFromDepth(screen.depth);
        screenData.screenDepthPerComponent = NSBitsPerSampleFromDepth(screen.depth);
        screenData.screenSupportsExtendedColor = [screen canRepresentDisplayGamut:NSDisplayGamutP3];
        screenData.screenHasInvertedColors = screenHasInvertedColors;
        screenData.screenIsMonochrome = CGDisplayUsesForceToGray();
        screenData.displayMask = CGDisplayIDToOpenGLDisplayMask(displayID);
        if (screenData.displayMask)
            screenData.gpuID = gpuIDForDisplayMask(screenData.displayMask);

        screenData.screenSize = FloatSize { CGDisplayScreenSize(displayID) };
        screenData.scaleFactor = screen.backingScaleFactor;
        screenData.screenSupportsHighDynamicRange = screenSupportsHighDynamicRange(displayID, screenData.preferredDynamicRangeMode);

        screenProperties.screenDataMap.set(displayID, WTFMove(screenData));
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

PlatformGPUID primaryGPUID()
{
    return gpuIDForDisplay(primaryScreenDisplayID());
}

PlatformGPUID gpuIDForDisplay(PlatformDisplayID displayID)
{
    if (auto data = screenData(displayID))
        return data->gpuID;

    return 0;
}

PlatformGPUID gpuIDForDisplayMask(GLuint displayMask)
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

    // (kCGLRPRegistryIDHigh, kCGLRPRegistryIDLow) are defined as (upper, lower) 32-bits
    // of the uint64_t IORegistryGPUID, even though they're obtained through the signed GLint getter.
    // Thus care must be taken when converting them to unsigned PlatformGPUID.
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
    return static_cast<PlatformGPUID>(static_cast<uint32_t>(gpuIDHigh)) << 32 | static_cast<uint32_t>(gpuIDLow);
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

DestinationColorSpace screenColorSpace(Widget* widget)
{
    if (auto data = screenProperties(widget))
        return data->colorSpace;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    return DestinationColorSpace { screen(widget).colorSpace.CGColorSpace };
}

ContentsFormat screenContentsFormat(Widget* widget, PlatformCALayerClient* client)
{
#if HAVE(HDR_SUPPORT)
    if (client && client->hdrForImagesEnabled() && screenSupportsHighDynamicRange(widget))
        return ContentsFormat::RGBA16F;
#endif

#if HAVE(IOSURFACE_RGB10)
    if (screenSupportsExtendedColor(widget))
        return ContentsFormat::RGBA10;
#endif

    UNUSED_PARAM(widget);
    UNUSED_PARAM(client);
    return ContentsFormat::RGBA8;
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
    if (PAL::isAVFoundationFrameworkAvailable()) {
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

FloatRect safeScreenFrame(NSScreen* screen)
{
    FloatRect frame = screen.frame;
#if HAVE(NSSCREEN_SAFE_AREA)
    auto insets = screen.safeAreaInsets;
    frame.contract(insets.left + insets.right, insets.top + insets.bottom);
    frame.move(insets.left, insets.bottom);
#endif
    return frame;
}

double ScreenData::screenDPI() const
{
    constexpr double mmPerInch = 25.4;
    auto screenWidthInches = screenSize.width() / mmPerInch;
    return screenRect.width() / screenWidthInches;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
