/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import <WebCore/VersionChecks.h>
#import <mach-o/dyld.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/Device.h>
#endif

namespace WebKit {

#if PLATFORM(COCOA) && HAVE(SYSTEM_FEATURE_FLAGS)

// Because of <rdar://problem/60608008>, WebKit has to parse the feature flags plist file
bool isFeatureFlagEnabled(const String& featureName)
{
    BOOL isWebKitBundleFromStagedFramework = [[[NSBundle mainBundle] bundlePath] hasPrefix:@"/Library/Apple/System/Library/StagedFrameworks/WebKit"];

    if (!isWebKitBundleFromStagedFramework)
        return _os_feature_enabled_impl("WebKit", (const char*)featureName.utf8().data());

    static NSDictionary* dictionary = [[NSDictionary dictionaryWithContentsOfFile:@"/Library/Apple/System/Library/FeatureFlags/Domain/WebKit.plist"] retain];

    if (![[dictionary objectForKey:featureName] objectForKey:@"Enabled"])
        return _os_feature_enabled_impl("WebKit", (const char*)featureName.characters8());

    return [[[dictionary objectForKey:featureName] objectForKey:@"Enabled"] isKindOfClass:[NSNumber class]] && [[[dictionary objectForKey:featureName] objectForKey:@"Enabled"] boolValue];
}

#endif

#if ENABLE(WEBGPU)

bool defaultWebGPUEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebGPU");
#endif

    return false;
}

#endif // ENABLE(WEBGPU)

#if HAVE(INCREMENTAL_PDF_APIS)

bool defaultIncrementalPDFEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("incremental_pdf");
#endif

    return false;
}

#endif

#if ENABLE(WEBXR)

bool defaultWebXREnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebXR");
#endif

    return false;
}

#endif // ENABLE(WEBXR)

#if PLATFORM(IOS_FAMILY)

bool defaultAllowsInlineMediaPlayback()
{
    return WebCore::deviceClass() == MGDeviceClassiPad;
}

bool defaultAllowsInlineMediaPlaybackAfterFullscreen()
{
    return WebCore::deviceClass() != MGDeviceClassiPad;
}

bool defaultAllowsPictureInPictureMediaPlayback()
{
    static bool shouldAllowPictureInPictureMediaPlayback = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_9_0;
    return shouldAllowPictureInPictureMediaPlayback;
}

bool defaultJavaScriptCanOpenWindowsAutomatically()
{
    static bool shouldAllowWindowOpenWithoutUserGesture = WebCore::IOSApplication::isTheSecretSocietyHiddenMystery() && dyld_get_program_sdk_version() < DYLD_IOS_VERSION_10_0;
    return shouldAllowWindowOpenWithoutUserGesture;
}

bool defaultInlineMediaPlaybackRequiresPlaysInlineAttribute()
{
    return WebCore::deviceClass() != MGDeviceClassiPad;
}

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatDefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultRequiresUserGestureToLoadVideo()
{
    static bool shouldRequireUserGestureToLoadVideo = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_10_0;
    return shouldRequireUserGestureToLoadVideo;
}

bool defaultWebSQLEnabled()
{
    // For backward compatibility, keep WebSQL working until apps are rebuilt with the iOS 14 SDK.
    static bool webSQLEnabled = dyld_get_program_sdk_version() < DYLD_IOS_VERSION_14_0;
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
        && (WebCore::MacApplication::isAOLInstantMessenger() || WebCore::MacApplication::isMicrosoftMyDay());

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

bool defaultEnforceCSSMIMETypeInNoQuirksMode()
{
    static bool needsQuirk = !_CFAppVersionCheckLessThan(CFSTR("com.apple.iWeb"), -1, 2.1);
    return needsQuirk;
}

bool defaultNeedsIsLoadingInAPISenseQuirk()
{
    static bool needsQuirk = _CFAppVersionCheckLessThan(CFSTR("com.apple.iAdProducer"), -1, 2.1);
    return needsQuirk;
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
    static bool shouldRestrictBaseURLSchemes = linkedOnOrAfter(WebCore::SDKVersion::FirstThatRestrictsBaseURLSchemes);
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
#if PLATFORM(IOS_FAMILY)
    static bool shouldConvertInvalidURLsToBlank = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_10_0;
#else
    static bool shouldConvertInvalidURLsToBlank = dyld_get_program_sdk_version() >= DYLD_MACOSX_VERSION_10_12;
#endif
    return shouldConvertInvalidURLsToBlank;
}

#if PLATFORM(MAC)

bool defaultPassiveWheelListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatDefaultsToPassiveWheelListenersOnDocument);
    return result;
}

bool defaultWheelEventGesturesBecomeNonBlocking()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatAllowsWheelEventGesturesToBecomeNonBlocking);
    return result;
}

#endif

} // namespace WebKit
