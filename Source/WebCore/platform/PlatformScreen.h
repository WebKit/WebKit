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

#pragma once

#if USE(GLIB)
#include <wtf/Function.h>
#endif

#if PLATFORM(MAC)
#include <wtf/HashMap.h>

OBJC_CLASS NSScreen;
OBJC_CLASS NSWindow;
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGRect NSRect;
typedef struct CGPoint NSPoint;
#else
typedef struct _NSRect NSRect;
typedef struct _NSPoint NSPoint;
#endif
#endif

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIScreen;
#endif

#if USE(CG)
typedef struct CGColorSpace *CGColorSpaceRef;
#endif

namespace WebCore {

class FloatRect;
class FloatSize;
class Widget;

using PlatformDisplayID = uint32_t;
using IORegistryGPUID = int64_t; // Global IOKit I/O registryID that can match a GPU across process boundaries.

int screenDepth(Widget*);
int screenDepthPerComponent(Widget*);
bool screenIsMonochrome(Widget*);

bool screenHasInvertedColors();
#if USE(GLIB)
double screenDPI();
void setScreenDPIObserverHandler(Function<void()>&&, void*);
#endif

FloatRect screenRect(Widget*);
FloatRect screenAvailableRect(Widget*);

WEBCORE_EXPORT bool screenSupportsExtendedColor(Widget* = nullptr);

#if USE(CG)
WEBCORE_EXPORT CGColorSpaceRef screenColorSpace(Widget* = nullptr);
#endif

#if PLATFORM(MAC)
struct ScreenProperties;

WEBCORE_EXPORT PlatformDisplayID displayID(NSScreen *);

WEBCORE_EXPORT NSScreen *screen(NSWindow *);
NSScreen *screen(PlatformDisplayID);

FloatRect screenRectForDisplay(PlatformDisplayID);
WEBCORE_EXPORT FloatRect screenRectForPrimaryScreen();

WEBCORE_EXPORT FloatRect toUserSpace(const NSRect&, NSWindow *destination);
FloatRect toUserSpaceForPrimaryScreen(const NSRect&);
WEBCORE_EXPORT NSRect toDeviceSpace(const FloatRect&, NSWindow *source);

NSPoint flipScreenPoint(const NSPoint&, NSScreen *);

WEBCORE_EXPORT ScreenProperties collectScreenProperties();
WEBCORE_EXPORT void setScreenProperties(const ScreenProperties&);

WEBCORE_EXPORT PlatformDisplayID primaryScreenDisplayID();

uint32_t primaryOpenGLDisplayMask();
uint32_t displayMaskForDisplay(PlatformDisplayID);

IORegistryGPUID primaryGPUID();
IORegistryGPUID gpuIDForDisplay(PlatformDisplayID);
IORegistryGPUID gpuIDForDisplayMask(uint32_t);

#endif // !PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

float screenPPIFactor();
WEBCORE_EXPORT FloatSize screenSize();
WEBCORE_EXPORT FloatSize availableScreenSize();
WEBCORE_EXPORT FloatSize overrideScreenSize();
WEBCORE_EXPORT float screenScaleFactor(UIScreen * = nullptr);

#endif

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(GTK) || PLATFORM(WPE)
bool screenHasTouchDevice();
bool screenIsTouchPrimaryInputDevice();
#else
constexpr bool screenHasTouchDevice() { return true; }
constexpr bool screenIsTouchPrimaryInputDevice() { return true; }
#endif
#endif

} // namespace WebCore
