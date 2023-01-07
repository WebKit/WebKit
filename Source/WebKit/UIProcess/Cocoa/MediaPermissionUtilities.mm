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

#import "config.h"
#import "MediaPermissionUtilities.h"

#import "SandboxUtilities.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SecurityOriginData.h>
#import <mutex>
#import <wtf/BlockPtr.h>
#import <wtf/URLHelpers.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#import "TCCSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/SpeechSoftLink.h>

namespace WebKit {

bool checkSandboxRequirementForType(MediaPermissionType type)
{
#if PLATFORM(MAC)
    static std::once_flag audioFlag;
    static std::once_flag videoFlag;
    static bool isAudioEntitled = true;
    static bool isVideoEntitled = true;
    
    auto checkFunction = [](const char* operation, bool* entitled) {
        if (!currentProcessIsSandboxed())
            return;

        int result = sandbox_check(getpid(), operation, static_cast<enum sandbox_filter_type>(SANDBOX_CHECK_NO_REPORT | SANDBOX_FILTER_NONE));
        if (result == -1)
            WTFLogAlways("Error checking '%s' sandbox access, errno=%ld", operation, (long)errno);
        *entitled = !result;
    };

    switch (type) {
    case MediaPermissionType::Audio:
        std::call_once(audioFlag, checkFunction, "device-microphone", &isAudioEntitled);
        return isAudioEntitled;
    case MediaPermissionType::Video:
        std::call_once(videoFlag, checkFunction, "device-camera", &isVideoEntitled);
        return isVideoEntitled;
    }
#endif
    return true;
}

bool checkUsageDescriptionStringForType(MediaPermissionType type)
{
    static std::once_flag audioDescriptionFlag;
    static std::once_flag videoDescriptionFlag;
    static bool hasMicrophoneDescriptionString = false;
    static bool hasCameraDescriptionString = false;

    switch (type) {
    case MediaPermissionType::Audio:
        static TCCAccessPreflightResult audioAccess = TCCAccessPreflight(get_TCC_kTCCServiceMicrophone(), NULL);
        if (audioAccess == kTCCAccessPreflightGranted)
            return true;
        std::call_once(audioDescriptionFlag, [] {
            hasMicrophoneDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length > 0;
        });
        return hasMicrophoneDescriptionString;
    case MediaPermissionType::Video:
        static TCCAccessPreflightResult videoAccess = TCCAccessPreflight(get_TCC_kTCCServiceCamera(), NULL);
        if (videoAccess == kTCCAccessPreflightGranted)
            return true;
        std::call_once(videoDescriptionFlag, [] {
            hasCameraDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSCameraUsageDescription"]).length > 0;
        });
        return hasCameraDescriptionString;
    }
}

bool checkUsageDescriptionStringForSpeechRecognition()
{
    return dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSSpeechRecognitionUsageDescription"]).length > 0;
}

static NSString* visibleDomain(const String& host)
{
    auto domain = WTF::URLHelpers::userVisibleURL(host.utf8());
    return startsWithLettersIgnoringASCIICase(domain, "www."_s) ? StringView(domain).substring(4).createNSString().autorelease() : static_cast<NSString *>(domain);
}

NSString *applicationVisibleNameFromOrigin(const WebCore::SecurityOriginData& origin)
{
    if (origin.protocol != "http"_s && origin.protocol != "https"_s)
        return nil;

    return visibleDomain(origin.host);
}

NSString *applicationVisibleName()
{
    NSBundle *appBundle = [NSBundle mainBundle];
    NSString *displayName = appBundle.infoDictionary[(__bridge NSString *)_kCFBundleDisplayNameKey];
    NSString *readableName = appBundle.infoDictionary[(__bridge NSString *)kCFBundleNameKey];
    return displayName ?: readableName;
}

static NSString *alertMessageText(MediaPermissionReason reason, const WebCore::SecurityOriginData& origin)
{
    NSString *visibleOrigin = applicationVisibleNameFromOrigin(origin);
    if (!visibleOrigin)
        visibleOrigin = applicationVisibleName();

    switch (reason) {
    case MediaPermissionReason::Camera:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera?", @"Message for user camera access prompt"), visibleOrigin];
    case MediaPermissionReason::CameraAndMicrophone:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera and microphone?", @"Message for user media prompt"), visibleOrigin];
    case MediaPermissionReason::Microphone:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your microphone?", @"Message for user microphone access prompt"), visibleOrigin];
    case MediaPermissionReason::ScreenCapture:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to observe your screen?", @"Message for screen sharing prompt"), visibleOrigin];
    case MediaPermissionReason::DeviceOrientation:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"“%@” Would Like to Access Motion and Orientation", @"Message for requesting access to the device motion and orientation"), visibleOrigin];
    case MediaPermissionReason::Geolocation:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your current location?", @"Message for geolocation prompt"), visibleOrigin];
    case MediaPermissionReason::SpeechRecognition:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to capture your audio and use it for speech recognition?", @"Message for spechrecognition prompt"), visibleDomain(origin.host)];
    }
}

static NSString *allowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (usermedia)", @"Allow button title in user media prompt");
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (screensharing)", @"Allow button title in screen sharing prompt");
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt");
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (geolocation)", @"Allow button title in geolocation prompt");
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (speechrecognition)", @"Allow button title in speech recognition prompt");
    }
}

static NSString *doNotAllowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (usermedia)", @"Disallow button title in user media prompt");
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (screensharing)", @"Disallow button title in screen sharing prompt");
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Cancel", "Cancel (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt");
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (geolocation)", @"Disallow button title in geolocation prompt");
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (speechrecognition)", @"Disallow button title in speech recognition prompt");
    }
}

void alertForPermission(WebPageProxy& page, MediaPermissionReason reason, const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

#if PLATFORM(IOS_FAMILY)
    if (reason == MediaPermissionReason::DeviceOrientation) {
        if (auto& userPermissionHandler = page.deviceOrientationUserPermissionHandlerForTesting())
            return completionHandler(userPermissionHandler());
    }
#endif

    auto webView = page.cocoaView();
    if (!webView) {
        completionHandler(false);
        return;
    }
    
    auto *alertTitle = alertMessageText(reason, origin);
    if (!alertTitle) {
        completionHandler(false);
        return;
    }

    auto *allowButtonString = allowButtonText(reason);
    auto *doNotAllowButtonString = doNotAllowButtonText(reason);
    auto completionBlock = makeBlockPtr(WTFMove(completionHandler));

#if PLATFORM(MAC)
    auto alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle];
    NSButton *button = [alert addButtonWithTitle:allowButtonString];
    button.keyEquivalent = @"";
    button = [alert addButtonWithTitle:doNotAllowButtonString];
    button.keyEquivalent = @"\E";
    [alert beginSheetModalForWindow:[webView window] completionHandler:[completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertFirstButtonReturn;
        completionBlock(shouldAllow);
    }];
#else
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:nil preferredStyle:UIAlertControllerStyleAlert];
    UIAlertAction* allowAction = [UIAlertAction actionWithTitle:allowButtonString style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *action) {
        completionBlock(true);
    }];

    UIAlertAction* doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *action) {
        completionBlock(false);
    }];

    [alert addAction:doNotAllowAction];
    [alert addAction:allowAction];

    [[UIViewController _viewControllerForFullScreenPresentationFromView:webView.get()] presentViewController:alert animated:YES completion:nil];
#endif
}

#if HAVE(AVCAPTUREDEVICE)

void requestAVCaptureAccessForType(MediaPermissionType type, CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    AVMediaType mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL authorized) mutable {
        callOnMainRunLoop([completionHandler = WTFMove(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getAVCaptureDeviceClass() requestAccessForMediaType:mediaType completionHandler:decisionHandler.get()];
}

MediaPermissionResult checkAVCaptureAccessForType(MediaPermissionType type)
{
    AVMediaType mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto authorizationStatus = [PAL::getAVCaptureDeviceClass() authorizationStatusForMediaType:mediaType];
    if (authorizationStatus == AVAuthorizationStatusDenied || authorizationStatus == AVAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == AVAuthorizationStatusNotDetermined)
        return MediaPermissionResult::Unknown;
    return MediaPermissionResult::Granted;
}

#endif // HAVE(AVCAPTUREDEVICE)

#if HAVE(SPEECHRECOGNIZER)

void requestSpeechRecognitionAccess(CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](SFSpeechRecognizerAuthorizationStatus status) mutable {
        bool authorized = status == SFSpeechRecognizerAuthorizationStatusAuthorized;
        callOnMainRunLoop([completionHandler = WTFMove(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getSFSpeechRecognizerClass() requestAuthorization:decisionHandler.get()];
}

MediaPermissionResult checkSpeechRecognitionServiceAccess()
{
    auto authorizationStatus = [PAL::getSFSpeechRecognizerClass() authorizationStatus];
IGNORE_WARNINGS_BEGIN("deprecated-enum-compare")
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusDenied || authorizationStatus == SFSpeechRecognizerAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusAuthorized)
        return MediaPermissionResult::Granted;
IGNORE_WARNINGS_END
    return MediaPermissionResult::Unknown;
}

bool checkSpeechRecognitionServiceAvailability(const String& localeIdentifier)
{
    auto recognizer = localeIdentifier.isEmpty() ? adoptNS([PAL::allocSFSpeechRecognizerInstance() init]) : adoptNS([PAL::allocSFSpeechRecognizerInstance() initWithLocale:[NSLocale localeWithLocaleIdentifier:localeIdentifier]]);
    return recognizer && [recognizer isAvailable];
}

#endif // HAVE(SPEECHRECOGNIZER)

} // namespace WebKit
