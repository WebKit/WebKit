/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPreferencesDefaultValues.h"

#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/NumberOfCores.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/UserInterfaceIdiom.h>
#endif
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#import "WebProcess.h"
#import <wtf/cocoa/Entitlements.h>
#endif

#if USE(LIBWEBRTC)
#include <WebCore/LibWebRTCProvider.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultCSSOMViewScrollingAPIEnabled()
{
    static bool result = WebCore::IOSApplication::isIMDb() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoIMDbCSSOMViewScrollingQuirk);
    return !result;
}

bool defaultShouldPrintBackgrounds()
{
    static bool result = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToExcludingBackgroundsWhenPrinting);
    return result;
}

bool defaultAlternateFormControlDesignEnabled()
{
    return PAL::currentUserInterfaceIdiomIsVisionOrVisionLegacy();
}

bool defaultVideoFullscreenRequiresElementFullscreen()
{
    return PAL::currentUserInterfaceIdiomIsVisionOrVisionLegacy();
}

#endif

#if PLATFORM(MAC)

bool defaultPassiveWheelListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToPassiveWheelListenersOnDocument);
    return result;
}

bool defaultWheelEventGesturesBecomeNonBlocking()
{
    static bool result = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::AllowsWheelEventGesturesToBecomeNonBlocking);
    return result;
}

#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)

bool defaultDisallowSyncXHRDuringPageDismissalEnabled()
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (CFPreferencesGetAppBooleanValue(CFSTR("allowDeprecatedSynchronousXMLHttpRequestDuringUnload"), CFSTR("com.apple.WebKit"), nullptr)) {
        WTFLogAlways("Allowing synchronous XHR during page unload due to managed preference");
        return false;
    }
#elif PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    if (allowsDeprecatedSynchronousXMLHttpRequestDuringUnload()) {
        WTFLogAlways("Allowing synchronous XHR during page unload due to managed preference");
        return false;
    }
#endif
    return true;
}

#endif

#if PLATFORM(MAC)

bool defaultAppleMailPaginationQuirkEnabled()
{
    return WebCore::MacApplication::isAppleMail();
}

#endif

#if ENABLE(MEDIA_STREAM)

bool defaultCaptureAudioInGPUProcessEnabled()
{
#if PLATFORM(MAC)
    // FIXME: Enable GPU process audio capture when <rdar://problem/29448368> is fixed.
    if (!WebCore::MacApplication::isSafari())
        return false;
#endif

#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    return true;
#else
    return false;
#endif
}

bool defaultCaptureAudioInUIProcessEnabled()
{
#if PLATFORM(MAC)
    return !defaultCaptureAudioInGPUProcessEnabled();
#endif

    return false;
}

bool defaultManageCaptureStatusBarInGPUProcessEnabled()
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Enable by default for all applications.
    return !WebCore::IOSApplication::isMobileSafari() && !WebCore::IOSApplication::isSafariViewService();
#else
    return false;
#endif
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(MEDIA_SOURCE)
bool defaultManagedMediaSourceEnabled()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}
#endif

#if ENABLE(MEDIA_SOURCE) && ENABLE(WIRELESS_PLAYBACK_TARGET)
bool defaultManagedMediaSourceNeedsAirPlay()
{
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
bool defaultMediaSessionCoordinatorEnabled()
{
    static dispatch_once_t onceToken;
    static bool enabled { false };
    dispatch_once(&onceToken, ^{
        if (WebCore::isInWebProcess())
            enabled = WebProcess::singleton().parentProcessHasEntitlement("com.apple.developer.group-session.urlactivity"_s);
        else
            enabled = WTF::processHasEntitlement("com.apple.developer.group-session.urlactivity"_s);
    });
    return enabled;
}
#endif

bool defaultRunningBoardThrottlingEnabled()
{
#if PLATFORM(MAC)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RunningBoardThrottling);
    return newSDK;
#else
    return false;
#endif
}

bool defaultShouldDropNearSuspendedAssertionAfterDelay()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContent);
    return newSDK;
#else
    return false;
#endif
}

bool defaultShouldTakeNearSuspendedAssertion()
{
#if PLATFORM(IOS_FAMILY)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContentImmediately);
    return !newSDK;
#else
    return true;
#endif
}

bool defaultLiveRangeSelectionEnabled()
{
#if PLATFORM(IOS_FAMILY)
    static bool enableForAllApps = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::LiveRangeSelectionEnabledForAllApps);
    if (!enableForAllApps && WebCore::IOSApplication::isGmail())
        return false;
#endif
    return true;
}

bool defaultShowModalDialogEnabled()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoShowModalDialog);
    return !newSDK;
#else
    return false;
#endif
}

#if ENABLE(GAMEPAD)
bool defaultGamepadVibrationActuatorEnabled()
{
#if HAVE(WIDE_GAMECONTROLLER_SUPPORT)
    return true;
#else
    return false;
#endif
}
#endif

bool defaultShouldEnableScreenOrientationAPI()
{
#if PLATFORM(MAC)
    return true;
#elif PLATFORM(IOS_FAMILY)
    static bool shouldEnableScreenOrientationAPI = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ScreenOrientationAPIEnabled) || WebCore::IOSApplication::isHoYoLAB();
    return shouldEnableScreenOrientationAPI;
#else
    return false;
#endif
}

#if USE(LIBWEBRTC)
bool defaultPeerConnectionEnabledAvailable()
{
    // This helper function avoid an expensive header include in WebPreferences.h
    return WebCore::WebRTCProvider::webRTCAvailable();
}
#endif

bool defaultPopoverAttributeEnabled()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PopoverAttributeEnabled);
    return newSDK;
#else
    return true;
#endif
}

bool defaultUseGPUProcessForDOMRenderingEnabled()
{
#if ENABLE(GPU_PROCESS_BY_DEFAULT) && ENABLE(GPU_PROCESS_DOM_RENDERING_BY_DEFAULT)
#if PLATFORM(MAC)
    static bool haveSufficientCores = WTF::numberOfPhysicalProcessorCores() >= 4;
    return haveSufficientCores;
#else
    return true;
#endif
#endif

#if USE(GRAPHICS_LAYER_WC)
    return true;
#endif

    return false;
}

bool defaultSearchInputIncrementalAttributeAndSearchEventEnabled()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoSearchInputIncrementalAttributeAndSearchEvent);
    return !newSDK;
#else
    return false;
#endif
}

} // namespace WebKit
