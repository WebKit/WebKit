/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/ContentsFormat.h>
#include <WebCore/ScreenProperties.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Platform.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(MAC)
OBJC_CLASS NSScreen;
OBJC_CLASS NSWindow;

typedef struct CGRect NSRect;
typedef struct CGPoint NSPoint;
#endif

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIScreen;
#endif

#if USE(CG)
typedef struct CGColorSpace *CGColorSpaceRef;
#endif

// X11 headers define a bunch of macros with common terms, interfering with WebCore and WTF enum values.
// As a workaround, we explicitly undef them here.
#if defined(None)
#undef None
#endif

namespace WebCore {

class DestinationColorSpace;
class FloatPoint;
class FloatRect;
class FloatSize;
class Widget;

int screenDepth(Widget*);
int screenDepthPerComponent(Widget*);
bool screenIsMonochrome(Widget*);
WEBCORE_EXPORT DestinationColorSpace screenColorSpace(Widget* = nullptr);

bool screenHasInvertedColors();

#if USE(GLIB)
double fontDPI(); // dpi to use for font scaling
double screenDPI(PlatformDisplayID); // dpi of the display device, corrected for device scaling
#endif

WEBCORE_EXPORT FloatRect screenRect(Widget*);
FloatRect screenAvailableRect(Widget*);

WEBCORE_EXPORT OptionSet<ContentsFormat> screenContentsFormats(Widget* = nullptr);
WEBCORE_EXPORT bool screenSupportsExtendedColor(Widget* = nullptr);

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
WEBCORE_EXPORT DynamicRangeMode preferredDynamicRangeMode(Widget* = nullptr);
#else
constexpr DynamicRangeMode preferredDynamicRangeMode(Widget* = nullptr) { return DynamicRangeMode::Standard; }
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
WEBCORE_EXPORT bool screenSupportsHighDynamicRange(Widget* = nullptr);
WEBCORE_EXPORT bool screenSupportsHighDynamicRange(PlatformDisplayID);
#else
constexpr bool screenSupportsHighDynamicRange(Widget* = nullptr) { return false; }
#endif

struct ScreenProperties;
struct ScreenData;

class PlatformScreen : public ThreadSafeRefCounted<PlatformScreen> {
public:
    WEBCORE_EXPORT static Ref<const PlatformScreen> singleton();
    WEBCORE_EXPORT static void updateSingletonProperties(ScreenProperties&&);

    WEBCORE_EXPORT const ScreenData* screenData(PlatformDisplayID) const LIFETIME_BOUND;
    WEBCORE_EXPORT PlatformDisplayID primaryScreenDisplayID() const;
    WEBCORE_EXPORT const ScreenProperties& screenProperties() const LIFETIME_BOUND;
    WEBCORE_EXPORT const ScreenDataMap& screenDatas() const LIFETIME_BOUND;

#if HAVE(SUPPORT_HDR_DISPLAY)
    WEBCORE_EXPORT OptionSet<ContentsFormat> screenContentsFormatsForTesting() const;
    WEBCORE_EXPORT static void updateSingletonContentsFormatsForTesting(OptionSet<ContentsFormat>);
#endif

private:
    static Ref<PlatformScreen> create(ScreenProperties&&);
    static Ref<PlatformScreen>& instance();

    explicit PlatformScreen(ScreenProperties&&);

    ScreenProperties m_properties;
};

WEBCORE_EXPORT ScreenProperties collectScreenProperties();

#if HAVE(SUPPORT_HDR_DISPLAY)
WEBCORE_EXPORT float currentEDRHeadroomForDisplay(PlatformDisplayID);
WEBCORE_EXPORT float maxEDRHeadroomForDisplay(PlatformDisplayID);
WEBCORE_EXPORT bool suppressEDRForDisplay(PlatformDisplayID);
#endif

#if PLATFORM(MAC)

WEBCORE_EXPORT PlatformDisplayID displayID(NSScreen *);

WEBCORE_EXPORT NSScreen *screen(NSWindow *);
NSScreen *screen(PlatformDisplayID);

WEBCORE_EXPORT FloatRect screenRectForDisplay(PlatformDisplayID);
WEBCORE_EXPORT FloatRect screenRectForPrimaryScreen();

WEBCORE_EXPORT FloatRect toUserSpace(const NSRect&, NSWindow *destination);
WEBCORE_EXPORT FloatRect toUserSpaceForPrimaryScreen(const NSRect&);
WEBCORE_EXPORT FloatPoint toUserSpaceForPrimaryScreen(const NSPoint&);
WEBCORE_EXPORT NSRect toDeviceSpace(const FloatRect&, NSWindow *source);

NSPoint flipScreenPoint(const NSPoint&, NSScreen *);

WEBCORE_EXPORT void setShouldOverrideScreenSupportsHighDynamicRange(bool shouldOverride, bool supportsHighDynamicRange);

uint32_t primaryOpenGLDisplayMask();
uint32_t displayMaskForDisplay(PlatformDisplayID);

PlatformGPUID primaryGPUID();
PlatformGPUID gpuIDForDisplay(PlatformDisplayID);
PlatformGPUID gpuIDForDisplayMask(uint32_t);

WEBCORE_EXPORT FloatRect safeScreenFrame(NSScreen *);

#endif // !PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

float screenPPIFactor();
WEBCORE_EXPORT FloatSize screenSize();
WEBCORE_EXPORT FloatSize availableScreenSize();
WEBCORE_EXPORT FloatSize overrideScreenSize();
WEBCORE_EXPORT FloatSize overrideAvailableScreenSize();
WEBCORE_EXPORT float screenScaleFactor(UIScreen * = nullptr);

#endif

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(GTK)
WEBCORE_EXPORT bool screenHasTouchDevice();
#else
constexpr bool screenHasTouchDevice() { return true; }
#endif
#endif

} // namespace WebCore
