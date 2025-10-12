/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#import "WebPreferencesDefaultValues.h"

#import "WebKitVersionChecks.h"
#import <Foundation/NSBundle.h>
#import <mach-o/dyld.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/Device.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

bool defaultAllowsInlineMediaPlayback()
{
    return !PAL::deviceClassIsSmallScreen();
}

bool defaultAllowsInlineMediaPlaybackAfterFullscreen()
{
    return PAL::deviceClassIsSmallScreen();
}

bool defaultAllowsPictureInPictureMediaPlayback()
{
    static bool shouldAllowPictureInPictureMediaPlayback = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PictureInPictureMediaPlayback);
    return shouldAllowPictureInPictureMediaPlayback;
}

bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute()
{
    return PAL::deviceClassIsSmallScreen();
}

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultShowModalDialogEnabled()
{
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoShowModalDialog);
    return !newSDK;
}

bool defaultRequiresUserGestureToLoadVideo()
{
    static bool shouldRequireUserGestureToLoadVideo = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RequiresUserGestureToLoadVideo);
    return shouldRequireUserGestureToLoadVideo;
}

bool defaultWebSQLEnabled()
{
    // For backward compatibility, keep WebSQL working until apps are rebuilt with the iOS 14 SDK.
    static bool webSQLEnabled = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::WebSQLDisabledByDefaultInLegacyWebKit);
    return webSQLEnabled;
}

bool defaultAllowContentSecurityPolicySourceStarToMatchAnyProtocol()
{
    static bool shouldAllowContentSecurityPolicySourceStarToMatchAnyProtocol = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_CONTENT_SECURITY_POLICY_SOURCE_STAR_PROTOCOL_RESTRICTION);
    return shouldAllowContentSecurityPolicySourceStarToMatchAnyProtocol;
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

bool defaultLoadDeferringEnabled()
{
    return !WTF::MacApplication::isAdobeInstaller();
}

bool defaultWindowFocusRestricted()
{
    return !WTF::MacApplication::isHRBlock();
}

bool defaultUsePreHTML5ParserQuirks()
{
    // Mail.app must continue to display HTML email that contains quirky markup.
    return WTF::MacApplication::isAppleMail();
}

bool defaultNeedsAdobeFrameReloadingQuirk()
{
    static bool needsQuirk = _CFAppVersionCheckLessThan(CFSTR("com.adobe.Acrobat"), -1, 9.0)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.Acrobat.Pro"), -1, 9.0)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.Reader"), -1, 9.0)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.distiller"), -1, 9.0)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.Contribute"), -1, 4.2)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.dreamweaver-9.0"), -1, 9.1)
        || _CFAppVersionCheckLessThan(CFSTR("com.macromedia.fireworks"), -1, 9.1)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.InCopy"), -1, 5.1)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.InDesign"), -1, 5.1)
        || _CFAppVersionCheckLessThan(CFSTR("com.adobe.Soundbooth"), -1, 2);

    return needsQuirk;
}

bool defaultScrollAnimatorEnabled()
{
    static bool enabled = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSScrollAnimationEnabled"];
    return enabled;
}

bool defaultTreatsAnyTextCSSLinkAsStylesheet()
{
    static bool needsQuirk = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_LINK_ELEMENT_TEXT_CSS_QUIRK)
        && _CFAppVersionCheckLessThan(CFSTR("com.e-frontier.shade10"), -1, 10.6);
    return needsQuirk;
}

bool defaultNeedsFrameNameFallbackToIdQuirk()
{
    static bool needsQuirk = _CFAppVersionCheckLessThan(CFSTR("info.colloquy"), -1, 2.5);
    return needsQuirk;
}

bool defaultNeedsKeyboardEventDisambiguationQuirks()
{
    static bool needsQuirks = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_IE_COMPATIBLE_KEYBOARD_EVENT_DISPATCH) && !WTF::MacApplication::isSafari();
    return needsQuirks;
}

#endif // PLATFORM(MAC)

bool defaultMutationEventsEnabled()
{
    return WTF::CocoaApplication::isAppleApplication() || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MutationEventsDisabledByDefault);
}

bool defaultAttachmentElementEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return WTF::IOSApplication::isMobileMail();
#else
    return WTF::MacApplication::isAppleMail();
#endif
}

bool defaultShouldRestrictBaseURLSchemes()
{
    static bool shouldRestrictBaseURLSchemes = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RestrictsBaseURLSchemes);
    return shouldRestrictBaseURLSchemes;
}

bool defaultShouldConvertInvalidURLsToBlank()
{
    static bool shouldConvertInvalidURLsToBlank = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToBlank);
    return shouldConvertInvalidURLsToBlank;
}

bool defaultPopoverAttributeEnabled()
{
    static bool newSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PopoverAttributeEnabled);
    return newSDK;
}

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

#if ENABLE(MEDIA_SOURCE) && PLATFORM(IOS_FAMILY)

bool defaultMediaSourceEnabled()
{
    return !PAL::deviceClassIsSmallScreen();
}

#endif

} // namespace WebKit
