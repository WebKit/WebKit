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

#import "config.h"
#import "MediaPermissionUtilities.h"

#import "SandboxUtilities.h"
#import "TCCSPI.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SecurityOrigin.h>
#import <mutex>
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/URLHelpers.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/SpeechSoftLink.h>

SOFT_LINK_PRIVATE_FRAMEWORK(TCC)
SOFT_LINK(TCC, TCCAccessPreflight, TCCAccessPreflightResult, (CFStringRef service, CFDictionaryRef options), (service, options))
SOFT_LINK_CONSTANT(TCC, kTCCServiceMicrophone, CFStringRef)
SOFT_LINK_CONSTANT(TCC, kTCCServiceCamera, CFStringRef)

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

        int result = sandbox_check(getpid(), operation, SANDBOX_FILTER_NONE);
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
        static TCCAccessPreflightResult audioAccess = TCCAccessPreflight(getkTCCServiceMicrophone(), NULL);
        if (audioAccess == kTCCAccessPreflightGranted)
            return true;
        std::call_once(audioDescriptionFlag, [] {
            hasMicrophoneDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length > 0;
        });
        return hasMicrophoneDescriptionString;
    case MediaPermissionType::Video:
        static TCCAccessPreflightResult videoAccess = TCCAccessPreflight(getkTCCServiceCamera(), NULL);
        if (videoAccess == kTCCAccessPreflightGranted)
            return true;
        std::call_once(videoDescriptionFlag, [] {
            hasCameraDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length > 0;
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
    return startsWithLettersIgnoringASCIICase(domain, "www.") ? domain.substring(4) : domain;
}

static NSString *alertMessageText(MediaPermissionReason reason, OptionSet<MediaPermissionType> types, const WebCore::SecurityOrigin& origin)
{
    NSString *visibleOrigin;
    if (origin.protocol() != "http" && origin.protocol() != "https") {
        NSBundle *appBundle = [NSBundle mainBundle];
        NSString *displayName = appBundle.infoDictionary[(__bridge NSString *)_kCFBundleDisplayNameKey];
        NSString *readableName = appBundle.infoDictionary[(__bridge NSString *)kCFBundleNameKey];
        visibleOrigin = displayName ?: readableName;
    } else
        visibleOrigin = visibleDomain(origin.host());

    switch (reason) {
    case MediaPermissionReason::UserMedia:
        if (types.contains(MediaPermissionType::Audio) && types.contains(MediaPermissionType::Video))
            return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera and microphone?", @"Message for user media prompt"), visibleOrigin];
        if (types.contains(MediaPermissionType::Audio))
            return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your microphone?", @"Message for user microphone access prompt"), visibleOrigin];
        if (types.contains(MediaPermissionType::Video))
            return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera?", @"Message for user camera access prompt"), visibleOrigin];
        return nil;
    case MediaPermissionReason::SpeechRecognition:
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to capture your audio and use it for speech recognition?", @"Message for spechrecognition prompt"), visibleDomain(origin.host())];
    }
}

static NSString *allowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::UserMedia:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (usermedia)", @"Allow button title in user media prompt");
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (speechrecognition)", @"Allow button title in speech recognition prompt");
    }
}

static NSString *doNotAllowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::UserMedia:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (usermedia)", @"Disallow button title in user media prompt");
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (speechrecognition)", @"Disallow button title in speech recognition prompt");
    }
}

void alertForPermission(WebPageProxy& page, MediaPermissionReason reason, OptionSet<MediaPermissionType> types, const WebCore::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    auto *webView = fromWebPageProxy(page);
    if (!webView) {
        completionHandler(false);
        return;
    }
    
    auto *alertTitle = alertMessageText(reason, types, origin);
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
    [alert addButtonWithTitle:doNotAllowButtonString];
    [alert addButtonWithTitle:allowButtonString];
    [alert beginSheetModalForWindow:webView.window completionHandler:[completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertSecondButtonReturn;
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

    [[UIViewController _viewControllerForFullScreenPresentationFromView:webView] presentViewController:alert animated:YES completion:nil];
#endif
}

#if HAVE(AVCAPTUREDEVICE)

void requestAVCaptureAccessForType(MediaPermissionType type, CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainThread());

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
    ASSERT(isMainThread());

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
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusDenied || authorizationStatus == SFSpeechRecognizerAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusAuthorized)
        return MediaPermissionResult::Granted;
    return MediaPermissionResult::Unknown;
}

bool checkSpeechRecognitionServiceAvailability(const String& localeIdentifier)
{
    auto recognizer = localeIdentifier.isEmpty() ? adoptNS([PAL::allocSFSpeechRecognizerInstance() init]) : adoptNS([PAL::allocSFSpeechRecognizerInstance() initWithLocale:[NSLocale localeWithLocaleIdentifier:localeIdentifier]]);
    return recognizer && [recognizer isAvailable];
}

#endif // HAVE(SPEECHRECOGNIZER)

} // namespace WebKit
