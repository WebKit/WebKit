/*
 * Copyright (C) 2006, 2014 Apple Inc.  All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "DeprecatedGlobalSettings.h"
#import "Device.h"
#import "FloatRect.h"
#import "FloatSize.h"
#import "FrameView.h"
#import "GraphicsContextCG.h"
#import "HostWindow.h"
#import "IntRect.h"
#import "WAKWindow.h"
#import "Widget.h"
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <pal/spi/ios/UIKitSPI.h>

namespace WebCore {

int screenDepth(Widget*)
{
    // Assume 32 bits per pixel. See <rdar://problem/9378829>.
    return 32;
}

int screenDepthPerComponent(Widget*)
{
    // Assume the screen depth is evenly divided into four color components. See <rdar://problem/9378829>.
    return screenDepth(nullptr) / 4;
}

bool screenIsMonochrome(Widget*)
{
    return PAL::softLinkUIKitUIAccessibilityIsGrayscaleEnabled();
}

bool screenHasInvertedColors()
{
    return PAL::softLinkUIKitUIAccessibilityIsInvertColorsEnabled();
}

bool screenSupportsExtendedColor(Widget*)
{
    return MGGetBoolAnswer(kMGQHasExtendedColorDisplay);
}

CGColorSpaceRef screenColorSpace(Widget* widget)
{
    return screenSupportsExtendedColor(widget) ? extendedSRGBColorSpaceRef() : sRGBColorSpaceRef();
}

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.
FloatRect screenRect(Widget* widget)
{
    if (!widget)
        return FloatRect();

    if (NSView *platformWidget = widget->platformWidget()) {
        // WebKit1
        WAKWindow *window = [platformWidget window];
        if (!window)
            return [platformWidget frame];
        CGRect screenRect = { CGPointZero, [window screenSize] };
        return enclosingIntRect(screenRect);
    }
    return enclosingIntRect(FloatRect(FloatPoint(), widget->root()->hostWindow()->overrideScreenSize()));
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!widget)
        return FloatRect();

    if (NSView *platformWidget = widget->platformWidget()) {
        // WebKit1
        WAKWindow *window = [platformWidget window];
        if (!window)
            return FloatRect();
        CGRect screenRect = { CGPointZero, [window availableScreenSize] };
        return enclosingIntRect(screenRect);
    }
    return enclosingIntRect(FloatRect(FloatPoint(), widget->root()->hostWindow()->availableScreenSize()));
}

float screenPPIFactor()
{
    static float ppiFactor;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        int pitch = MGGetSInt32Answer(kMGQMainScreenPitch, 0);
        float scale = MGGetFloat32Answer(kMGQMainScreenScale, 0);

        static const float originalIPhonePPI = 163;
        float mainScreenPPI = (pitch && scale) ? pitch / scale : originalIPhonePPI;
        ppiFactor = mainScreenPPI / originalIPhonePPI;
    });
    
    return ppiFactor;
}

FloatSize screenSize()
{
    if (deviceHasIPadCapability() && [[PAL::get_UIKit_UIApplicationClass() sharedApplication] _isClassic])
        return { 320, 480 };
    return FloatSize([[PAL::get_UIKit_UIScreenClass() mainScreen] _referenceBounds].size);
}

FloatSize availableScreenSize()
{
    if (deviceHasIPadCapability() && [[PAL::get_UIKit_UIApplicationClass() sharedApplication] _isClassic])
        return { 320, 480 };
    return FloatSize([PAL::get_UIKit_UIScreenClass() mainScreen].bounds.size);
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/PlatformScreenIOS.mm>)
#import <WebKitAdditions/PlatformScreenIOS.mm>
#else
FloatSize overrideScreenSize()
{
    return screenSize();
}
#endif

float screenScaleFactor(UIScreen *screen)
{
    if (!screen)
        screen = [PAL::get_UIKit_UIScreenClass() mainScreen];

    return screen.scale;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
