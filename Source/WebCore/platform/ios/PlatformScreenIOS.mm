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

#import "Device.h"
#import "FloatRect.h"
#import "FloatSize.h"
#import "FrameView.h"
#import "HostWindow.h"
#import "IntRect.h"
#import "MobileGestaltSPI.h"
#import "SoftLinking.h"
#import "UIKitSPI.h"
#import "WAKWindow.h"
#import "WebCoreSystemInterface.h"
#import "Widget.h"

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIApplication)
SOFT_LINK_CLASS(UIKit, UIScreen)
SOFT_LINK(UIKit, UIAccessibilityIsGrayscaleEnabled, BOOL, (void), ())
SOFT_LINK(UIKit, UIAccessibilityIsInvertColorsEnabled, BOOL, (void), ())

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
    return UIAccessibilityIsGrayscaleEnabled();
}

bool screenHasInvertedColors()
{
    return UIAccessibilityIsInvertColorsEnabled();
}

bool screenSupportsExtendedColor()
{
#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 90300
    return MGGetBoolAnswer(kMGQHasExtendedColorDisplay);
#else
    return false;
#endif
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
    return enclosingIntRect(FloatRect(FloatPoint(), widget->root()->hostWindow()->screenSize()));
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
    if (deviceHasIPadCapability() && [[getUIApplicationClass() sharedApplication] _isClassic])
        return { 320, 480 };
    return FloatSize([[getUIScreenClass() mainScreen] _referenceBounds].size);
}

FloatSize availableScreenSize()
{
    if (deviceHasIPadCapability() && [[getUIApplicationClass() sharedApplication] _isClassic])
        return { 320, 480 };
    return FloatSize([getUIScreenClass() mainScreen].bounds.size);
}

float screenScaleFactor(UIScreen *screen)
{
    if (!screen)
        screen = [getUIScreenClass() mainScreen];

    CGFloat scale = screen.scale;

    // We can remove this clamping once <rdar://problem/16395475> is fixed.
    const CGFloat maximumClassicScreenScaleFactor = 2;
    if ([[getUIApplicationClass() sharedApplication] _isClassic])
        return std::min(scale, maximumClassicScreenScaleFactor);

    return scale;
}

} // namespace WebCore
