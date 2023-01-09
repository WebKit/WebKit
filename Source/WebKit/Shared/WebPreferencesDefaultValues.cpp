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
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#import "WebProcess.h"
#import <wtf/cocoa/Entitlements.h>
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

#if !USE(APPLE_INTERNAL_SDK)
bool defaultAlternateFormControlDesignEnabled()
{
    return false;
}
#endif

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

bool defaultOfflineWebApplicationCacheEnabled()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ApplicationCacheDisabledByDefault);
    return !newSDK;
#else
    // FIXME: Other platforms should consider turning this off.
    // ApplicationCache is on its way to being removed from WebKit.
    return true;
#endif
}

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

bool defaultShouldTakeSuspendedAssertions()
{
#if PLATFORM(IOS_FAMILY)
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContent);
    return !newSDK;
#else
    return true;
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

} // namespace WebKit
