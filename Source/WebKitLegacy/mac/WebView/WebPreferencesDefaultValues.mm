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
#import <WebCore/RuntimeApplicationChecks.h>
#import <mach-o/dyld.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/Device.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

bool defaultAllowsInlineMediaPlayback()
{
    return !WebCore::deviceClassIsSmallScreen();
}

bool defaultAllowsInlineMediaPlaybackAfterFullscreen()
{
    return WebCore::deviceClassIsSmallScreen();
}

bool defaultAllowsPictureInPictureMediaPlayback()
{
    static bool shouldAllowPictureInPictureMediaPlayback = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::PictureInPictureMediaPlayback);
    return shouldAllowPictureInPictureMediaPlayback;
}

bool defaultJavaScriptCanOpenWindowsAutomatically()
{
    static bool shouldAllowWindowOpenWithoutUserGesture = WebCore::IOSApplication::isTheSecretSocietyHiddenMystery() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoTheSecretSocietyHiddenMysteryWindowOpenQuirk);
    return shouldAllowWindowOpenWithoutUserGesture;
}

bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute()
{
    return WebCore::deviceClassIsSmallScreen();
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
    return !WebCore::MacApplication::isAdobeInstaller();
}

bool defaultWindowFocusRestricted()
{
    return !WebCore::MacApplication::isHRBlock();
}

bool defaultUsePreHTML5ParserQuirks()
{
    // AOL Instant Messenger and Microsoft My Day contain markup incompatible
    // with the new HTML5 parser. If these applications were linked against a
    // version of WebKit prior to the introduction of the HTML5 parser, enable
    // parser quirks to maintain compatibility. For details, see
    // <https://bugs.webkit.org/show_bug.cgi?id=46134> and
    // <https://bugs.webkit.org/show_bug.cgi?id=46334>.
    static bool isApplicationNeedingParserQuirks = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_HTML5_PARSER)
        && (WebCore::MacApplication::isMicrosoftMyDay());

    // Mail.app must continue to display HTML email that contains quirky markup.
    static bool isAppleMail = WebCore::MacApplication::isAppleMail();

    return isApplicationNeedingParserQuirks || isAppleMail;
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
    static bool needsQuirks = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_IE_COMPATIBLE_KEYBOARD_EVENT_DISPATCH) && !WebCore::MacApplication::isSafari();
    return needsQuirks;
}

#endif // PLATFORM(MAC)

bool defaultAttachmentElementEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return WebCore::IOSApplication::isMobileMail();
#else
    return WebCore::MacApplication::isAppleMail();
#endif
}

bool defaultShouldRestrictBaseURLSchemes()
{
    static bool shouldRestrictBaseURLSchemes = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::RestrictsBaseURLSchemes);
    return shouldRestrictBaseURLSchemes;
}

bool defaultUseLegacyBackgroundSizeShorthandBehavior()
{
#if PLATFORM(IOS_FAMILY)
    static bool shouldUseLegacyBackgroundSizeShorthandBehavior = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_LEGACY_BACKGROUNDSIZE_SHORTHAND_BEHAVIOR);
#else
    static bool shouldUseLegacyBackgroundSizeShorthandBehavior = WebCore::MacApplication::isVersions()
        && !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_LEGACY_BACKGROUNDSIZE_SHORTHAND_BEHAVIOR);
#endif
    return shouldUseLegacyBackgroundSizeShorthandBehavior;
}

bool defaultAllowDisplayOfInsecureContent()
{
    static bool shouldAllowDisplayOfInsecureContent = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_INSECURE_CONTENT_BLOCKING);
    return shouldAllowDisplayOfInsecureContent;
}

bool defaultAllowRunningOfInsecureContent()
{
    static bool shouldAllowRunningOfInsecureContent = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_INSECURE_CONTENT_BLOCKING);
    return shouldAllowRunningOfInsecureContent;
}

bool defaultShouldConvertInvalidURLsToBlank()
{
    static bool shouldConvertInvalidURLsToBlank = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToBlank);
    return shouldConvertInvalidURLsToBlank;
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
    return !WebCore::deviceClassIsSmallScreen();
}

#endif

} // namespace WebKit
