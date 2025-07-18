/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#include <wtf/NumberOfCores.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/UserInterfaceIdiom.h>
#endif
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR) || HAVE(DIGITAL_CREDENTIALS_UI)
#import "WebProcess.h"
#import <wtf/cocoa/Entitlements.h>
#endif

#if USE(LIBWEBRTC)
#include <WebCore/LibWebRTCProvider.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebPreferencesDefaultValuesAdditions.h>
#endif

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/DeprecatedGlobalSettingsAdditions.cpp>)
#include <WebKitAdditions/DeprecatedGlobalSettingsAdditions.cpp>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultShouldPrintBackgrounds()
{
    static bool result = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToExcludingBackgroundsWhenPrinting);
    return result;
}

#endif

#if ENABLE(FULLSCREEN_API)

bool defaultVideoFullscreenRequiresElementFullscreen()
{
#if USE(APPLE_INTERNAL_SDK)
    if (videoFullscreenRequiresElementFullscreenFromAdditions())
        return true;
#endif

#if PLATFORM(IOS_FAMILY)
    if (PAL::currentUserInterfaceIdiomIsVision())
        return true;
#endif

    return false;
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

bool defaultAppleMailPaginationQuirkEnabled()
{
    return WTF::MacApplication::isAppleMail();
}

#endif

#if ENABLE(MEDIA_STREAM)

bool defaultCaptureAudioInGPUProcessEnabled()
{
#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    return true;
#else
    return false;
#endif
}

bool defaultManageCaptureStatusBarInGPUProcessEnabled()
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Enable by default for all applications.
    return !WTF::IOSApplication::isMobileSafari() && !WTF::IOSApplication::isSafariViewService();
#else
    return false;
#endif
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(MEDIA_SOURCE)
bool defaultManagedMediaSourceEnabled()
{
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
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
        if (isInWebProcess())
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
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContentImmediately);
    return !newSDK;
#else
    return true;
#endif
}

bool defaultLinearMediaPlayerEnabled()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    return PAL::currentUserInterfaceIdiomIsVision();
#else
    return false;
#endif
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

#if ENABLE(WEB_AUTHN)
bool defaultDigitalCredentialsEnabled()
{
#if HAVE(DIGITAL_CREDENTIALS_UI)
    static dispatch_once_t onceToken;
    static bool enabled { false };
    dispatch_once(&onceToken, ^{
        auto entitlementChecker = [inWebProcess = isInWebProcess()](auto entitlement) {
            if (inWebProcess)
                return WebProcess::singleton().parentProcessHasEntitlement(entitlement);
            return WTF::processHasEntitlement(entitlement);
        };
        enabled = entitlementChecker("com.apple.developer.web-browser"_s) || entitlementChecker("com.apple.developer.identity-document-services.web-presentment-controller"_s);
    });
    return enabled;
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
    static bool shouldEnableScreenOrientationAPI = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ScreenOrientationAPIEnabled) || WTF::IOSApplication::isHoYoLAB();
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

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
bool defaultBuiltInNotificationsEnabled()
{
#if defined(DEPRECATED_GLOBAL_SETTINGS_BUILT_IN_NOTIFICATIONS_ENABLED_ADDITIONS)
    DEPRECATED_GLOBAL_SETTINGS_BUILT_IN_NOTIFICATIONS_ENABLED_ADDITIONS;
#endif

#if defined(WEB_PREFERENCES_BUILT_IN_NOTIFICATIONS_ENABLED_ADDITIONS)
    WEB_PREFERENCES_BUILT_IN_NOTIFICATIONS_ENABLED_ADDITIONS;
#endif

    return false;
}
#endif

#if ENABLE(DEVICE_ORIENTATION)
bool defaultDeviceOrientationPermissionAPIEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportsDeviceOrientationAndMotionPermissionAPI);
#else
    return false;
#endif
}
#endif

#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
bool defaultRequiresPageVisibilityForVideoToBeNowPlaying()
{
#if USE(APPLE_INTERNAL_SDK)
    if (requiresPageVisibilityForVideoToBeNowPlayingFromAdditions())
        return true;
#endif

    return false;
}
#endif

bool defaultCookieStoreAPIEnabled()
{
#if ENABLE(COOKIE_STORE_API_BY_DEFAULT)
    return true;
#else
    return false;
#endif
}

#if ENABLE(CONTENT_EXTENSIONS)
bool defaultIFrameResourceMonitoringEnabled()
{
#if PLATFORM(COCOA)
    return isFullWebBrowserOrRunningTest();
#else
    return false;
#endif
}
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
bool defaultPreferSpatialAudioExperience()
{
#if defined(WEB_PREFERENCES_PREFER_SPATIAL_AUDIO_EXPERIENCE_ADDITIONS)
    WEB_PREFERENCES_PREFER_SPATIAL_AUDIO_EXPERIENCE_ADDITIONS;
#endif

    return false;
}
#endif

#if PLATFORM(COCOA)
static bool isSafariOrWebApp()
{
#if PLATFORM(MAC)
    return WTF::MacApplication::isSafari();
#else
    return WTF::IOSApplication::isMobileSafari() || WTF::IOSApplication::isSafariViewService() || WTF::IOSApplication::isAppleWebApp();
#endif
}
#endif

bool defaultMutationEventsEnabled()
{
#if PLATFORM(COCOA)
    return (WTF::CocoaApplication::isAppleApplication() && !isSafariOrWebApp()) || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MutationEventsDisabledByDefault);
#else
    return false;
#endif
}

bool defaultTrustedTypesEnabled()
{
#if PLATFORM(COCOA)
    return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::EnableTrustedTypesByDefault);
#else
    return true;
#endif
}

#if !PLATFORM(COCOA)
bool defaultContentInsetBackgroundFillEnabled()
{
    return false;
}
#endif

#if !PLATFORM(COCOA)
bool defaultTopContentInsetBackgroundCanChangeAfterScrolling()
{
    return false;
}
#endif

} // namespace WebKit
