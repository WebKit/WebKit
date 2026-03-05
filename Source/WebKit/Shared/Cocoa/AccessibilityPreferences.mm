/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AccessibilityPreferences.h"

#import "AccessibilitySupportSPI.h"
#import <wtf/SoftLinking.h>

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
SOFT_LINK_LIBRARY_OPTIONAL(libAccessibility)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSReduceMotionAutoplayAnimatedImagesEnabled, Boolean, (), ());
#endif

#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>

namespace AXPreferenceHelpers {

static bool shouldUseDefault()
{
    RetainPtr forceDefault = [[NSUserDefaults standardUserDefaults] objectForKey:@"ForceDefaultAccessibilitySettings"];
    return forceDefault && [forceDefault boolValue];
}

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
WebKit::WebKitAXValueState reduceMotionEnabled()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPerAppSettingsState;
    RetainPtr appId = applicationBundleIdentifier().createCFString();
    return WebKit::toWebKitAXValueState(_AXSReduceMotionEnabledApp(appId.get()));
}

WebKit::WebKitAXValueState increaseButtonLegibility()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPerAppSettingsState;
    RetainPtr appId = applicationBundleIdentifier().createCFString();
    return WebKit::toWebKitAXValueState(_AXSIncreaseButtonLegibilityApp(appId.get()));
}

WebKit::WebKitAXValueState enhanceTextLegibility()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPerAppSettingsState;
    RetainPtr appId = applicationBundleIdentifier().createCFString();
    return WebKit::toWebKitAXValueState(_AXSEnhanceTextLegibilityEnabledApp(appId.get()));
}

WebKit::WebKitAXValueState darkenSystemColors()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPerAppSettingsState;
    RetainPtr appId = applicationBundleIdentifier().createCFString();
    return WebKit::toWebKitAXValueState(_AXDarkenSystemColorsApp(appId.get()));
}

WebKit::WebKitAXValueState invertColorsEnabled()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPerAppSettingsState;
    RetainPtr appId = applicationBundleIdentifier().createCFString();
    return WebKit::toWebKitAXValueState(_AXSInvertColorsEnabledApp(appId.get()));
}
#endif // HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)

bool imageAnimationEnabled()
{
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialImageAnimationEnabled;
    if (auto* functionPointer = _AXSReduceMotionAutoplayAnimatedImagesEnabledPtr())
        return functionPointer();
#endif
    return true;
}

bool enhanceTextLegibilityOverall()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialShouldEnhanceTextLegibilityOverall;
    return _AXSEnhanceTextLegibilityEnabled();
}

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
bool prefersNonBlinkingCursor()
{
    if (shouldUseDefault()) [[unlikely]]
        return WebKit::initialPrefersNonBlinkingCursor;
    return _AXSPrefersNonBlinkingCursorIndicator();
}
#endif

} // namespace Accessibility
