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

#if PLATFORM(IOS_FAMILY)
#import "UIKitUtilities.h"
#endif

#import "TCCSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/SpeechSoftLink.h>

namespace WebKit {

bool checkSandboxRequirementForType(MediaPermissionType type)
{
#if PLATFORM(MAC)
    auto checkFunction = [](ASCIILiteral operation) {
        if (!currentProcessIsSandboxed())
            return true;

        int result = sandbox_check(getpid(), operation, static_cast<enum sandbox_filter_type>(SANDBOX_CHECK_NO_REPORT | SANDBOX_FILTER_NONE));
        if (result == -1)
            SAFE_WTFLOGALWAYS("Error checking '%s' sandbox access, errno=%ld", operation, (long)errno);
        return !result;
    };

    switch (type) {
    case MediaPermissionType::Audio:
        static bool isAudioEntitled = checkFunction("device-microphone"_s);
        return isAudioEntitled;
    case MediaPermissionType::Video:
        static bool isVideoEntitled = checkFunction("device-camera"_s);
        return isVideoEntitled;
    }
#endif
    return true;
}

bool checkUsageDescriptionStringForType(MediaPermissionType type)
{
    switch (type) {
    case MediaPermissionType::Audio:
        static TCCAccessPreflightResult audioAccess = TCCAccessPreflight(get_TCC_kTCCServiceMicrophoneSingleton(), NULL);
        if (audioAccess == kTCCAccessPreflightGranted)
            return true;
        static bool hasMicrophoneDescriptionString= dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length > 0;
        return hasMicrophoneDescriptionString;
    case MediaPermissionType::Video:
        static TCCAccessPreflightResult videoAccess = TCCAccessPreflight(get_TCC_kTCCServiceCameraSingleton(), NULL);
        if (videoAccess == kTCCAccessPreflightGranted)
            return true;
        static bool hasCameraDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSCameraUsageDescription"]).length > 0;
        return hasCameraDescriptionString;
    }
}

bool checkUsageDescriptionStringForSpeechRecognition()
{
    return dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSSpeechRecognitionUsageDescription"]).length > 0;
}

static RetainPtr<NSString> visibleDomain(const String& host)
{
    auto domain = WTF::URLHelpers::userVisibleURL(host.utf8().span());
    return startsWithLettersIgnoringASCIICase(domain, "www."_s) ? StringView(domain).substring(4).createNSString() : domain.createNSString();
}

RetainPtr<NSString> applicationVisibleNameFromOrigin(const WebCore::SecurityOriginData& origin)
{
    if (origin.protocol() != "http"_s && origin.protocol() != "https"_s)
        return nil;

    return visibleDomain(origin.host());
}

RetainPtr<NSString> applicationVisibleName()
{
    RetainPtr appBundle = [NSBundle mainBundle];
    if (RetainPtr displayName = dynamic_objc_cast<NSString>(appBundle.get().infoDictionary[bridge_cast(_kCFBundleDisplayNameKey)]))
        return displayName;
    return dynamic_objc_cast<NSString>(appBundle.get().infoDictionary[bridge_cast(kCFBundleNameKey)]);
}

static RetainPtr<NSString> alertMessageText(MediaPermissionReason reason, const WebCore::SecurityOriginData& origin)
{
    RetainPtr visibleOrigin = applicationVisibleNameFromOrigin(origin);
    if (!visibleOrigin)
        visibleOrigin = applicationVisibleName();

    switch (reason) {
    case MediaPermissionReason::Camera:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera?", @"Message for user camera access prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::CameraAndMicrophone:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera and microphone?", @"Message for user media prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::Microphone:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your microphone?", @"Message for user microphone access prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::ScreenCapture:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to observe your screen?", @"Message for screen sharing prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::DeviceOrientation:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"“%@” Would Like to Access Motion and Orientation", @"Message for requesting access to the device motion and orientation"), visibleOrigin.get()]);
    case MediaPermissionReason::Geolocation:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your current location?", @"Message for geolocation prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::SpeechRecognition:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to capture your audio and use it for speech recognition?", @"Message for spechrecognition prompt"), visibleDomain(origin.host()).get()]);
    }
}

static RetainPtr<NSString> allowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (usermedia)", @"Allow button title in user media prompt").createNSString();
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (screensharing)", @"Allow button title in screen sharing prompt").createNSString();
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt").createNSString();
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (geolocation)", @"Allow button title in geolocation prompt").createNSString();
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (speechrecognition)", @"Allow button title in speech recognition prompt").createNSString();
    }
}

static RetainPtr<NSString> doNotAllowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (usermedia)", @"Disallow button title in user media prompt").createNSString();
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (screensharing)", @"Disallow button title in screen sharing prompt").createNSString();
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Cancel", "Cancel (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt").createNSString();
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (geolocation)", @"Disallow button title in geolocation prompt").createNSString();
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (speechrecognition)", @"Disallow button title in speech recognition prompt").createNSString();
    }
}

#if PLATFORM(MAC)
static MediaPermissionPromptResult promptResultForAlertResponse(NSModalResponse returnCode, const Vector<MediaPermissionPromptAdditionalAction>& additionalActions)
{
    if (returnCode == NSAlertFirstButtonReturn)
        return { .granted = true, .wasAnswered = true };
    if (returnCode == NSAlertSecondButtonReturn)
        return { .granted = false, .wasAnswered = true };

    // Return NSAlertFirstButtonReturn and NSAlertSecondButtonReturn for the two built-in buttons,
    // NSAlertThirdButtonReturn... for custom buttons
    size_t index = returnCode - NSAlertThirdButtonReturn;
    if (index >= additionalActions.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return {
        .granted = additionalActions[index].decision == MediaPermissionPromptAdditionalAction::Decision::Allow,
        .wasAnswered = true,
        .chosenAction = additionalActions[index].source,
    };
}
#endif

static RetainPtr<NSString> promptMessageText(const PermissionPromptRequest& request)
{
    if (!request.customization.messageText.isNull())
        return request.customization.messageText.createNSString();
    return alertMessageText(request.reason, request.origin);
}

static RetainPtr<NSString> promptAllowButtonText(const PermissionPromptRequest& request)
{
    if (!request.customization.allowButtonTitle.isNull())
        return request.customization.allowButtonTitle.createNSString();
    return allowButtonText(request.reason);
}

static RetainPtr<NSString> promptDenyButtonText(const PermissionPromptRequest& request)
{
    if (!request.customization.denyButtonTitle.isNull())
        return request.customization.denyButtonTitle.createNSString();
    return doNotAllowButtonText(request.reason);
}

static RetainPtr<NSString> promptInformativeText(const PermissionPromptRequest& request)
{
    if (request.customization.informativeText.isNull())
        return nil;
    return request.customization.informativeText.createNSString();
}

void alertForPermission(WebPageProxy& page, PermissionPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

#if PLATFORM(IOS_FAMILY)
    if (request.reason == MediaPermissionReason::DeviceOrientation) {
        if (auto& userPermissionHandler = page.deviceOrientationUserPermissionHandlerForTesting())
            return completionHandler({ .granted = userPermissionHandler() });
    }
#endif

    RetainPtr webView = page.cocoaView();
    if (!webView)
        return completionHandler({ });

    RetainPtr alertTitle = promptMessageText(request);
    if (!alertTitle)
        return completionHandler({ });

    RetainPtr allowButtonString = promptAllowButtonText(request);
    RetainPtr doNotAllowButtonString = promptDenyButtonText(request);
    auto completionBlock = makeBlockPtr(WTF::move(completionHandler));

#if PLATFORM(MAC)
    RetainPtr alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle.get()];
    if (RetainPtr informativeText = promptInformativeText(request))
        [alert setInformativeText:informativeText.get()];
    RetainPtr button = [alert addButtonWithTitle:allowButtonString.get()];
    [button setKeyEquivalent:@""];
    // Tags keep the responses promptResultForAlertResponse expects, which would otherwise follow position.
    for (size_t index = 0; index < request.customization.additionalActions.size(); ++index) {
        button = [alert addButtonWithTitle:request.customization.additionalActions[index].title.createNSString().get()];
        // Cleared so AppKit does not hand Return to a button the application supplied.
        [button setKeyEquivalent:@""];
        [button setTag:NSAlertThirdButtonReturn + index];
    }
    button = [alert addButtonWithTitle:doNotAllowButtonString.get()];
    [button setKeyEquivalent:@"\E"];
    [button setTag:NSAlertSecondButtonReturn];

    [alert beginSheetModalForWindow:retainPtr([webView window]).get() completionHandler:[completionBlock, additionalActions = WTF::move(request.customization.additionalActions)](NSModalResponse returnCode) {
        completionBlock(promptResultForAlertResponse(returnCode, additionalActions));
    }];
#else
    RetainPtr alert = WebKit::createUIAlertController(alertTitle.get(), promptInformativeText(request).get());
    RetainPtr allowAction = [UIAlertAction actionWithTitle:allowButtonString.get() style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *) {
        completionBlock({ .granted = true, .wasAnswered = true });
    }];

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString.get() style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *) {
        completionBlock({ .granted = false, .wasAnswered = true });
    }];

    [alert addAction:allowAction.get()];

    for (auto& action : request.customization.additionalActions) {
        bool granted = action.decision == MediaPermissionPromptAdditionalAction::Decision::Allow;
        [alert addAction:[UIAlertAction actionWithTitle:action.title.createNSString().get() style:UIAlertActionStyleDefault handler:[completionBlock, source = action.source, granted](UIAlertAction *) {
            completionBlock({ .granted = granted, .wasAnswered = true, .chosenAction = source });
        }]];
    }

    [alert addAction:doNotAllowAction.get()];

#if PLATFORM(VISION)
    page.dispatchWillPresentModalUI();
#endif
    [[webView _wk_viewControllerForFullScreenPresentation] presentViewController:alert.get() animated:YES completion:nil];
#endif
}

void alertForPermission(WebPageProxy& page, MediaPermissionReason reason, const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    alertForPermission(page, { .reason = reason, .origin = origin }, [completionHandler = WTF::move(completionHandler)](MediaPermissionPromptResult&& result) mutable {
        completionHandler(result.granted);
    });
}

#if ENABLE(MEDIA_STREAM)

Ref<MediaPermissionPromptForTesting> MediaPermissionPromptForTesting::create(String&& title, String&& informativeText, Vector<String>&& buttonTitles, Vector<MediaPermissionPromptAdditionalAction>&& additionalActions, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    return adoptRef(*new MediaPermissionPromptForTesting(WTF::move(title), WTF::move(informativeText), WTF::move(buttonTitles), WTF::move(additionalActions), WTF::move(completionHandler)));
}

MediaPermissionPromptForTesting::MediaPermissionPromptForTesting(String&& title, String&& informativeText, Vector<String>&& buttonTitles, Vector<MediaPermissionPromptAdditionalAction>&& additionalActions, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
    : m_title(WTF::move(title))
    , m_informativeText(WTF::move(informativeText))
    , m_buttonTitles(WTF::move(buttonTitles))
    , m_additionalActions(WTF::move(additionalActions))
    , m_completionHandler(WTF::move(completionHandler))
{
}

MediaPermissionPromptForTesting::~MediaPermissionPromptForTesting()
{
    respond(std::nullopt);
}

void MediaPermissionPromptForTesting::respond(std::optional<size_t> buttonIndex)
{
    ASSERT(!buttonIndex || *buttonIndex < m_buttonTitles.size());
    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    if (!completionHandler)
        return;

    MediaPermissionPromptResult result;
    if (auto index = buttonIndex) {
        result.wasAnswered = true;
        if (!*index)
            result.granted = true;
        else if (*index <= m_additionalActions.size()) {
            result.chosenAction = m_additionalActions[*index - 1].source;
            result.granted = m_additionalActions[*index - 1].decision == MediaPermissionPromptAdditionalAction::Decision::Allow;
        }
    }
    completionHandler(WTF::move(result));
}

MediaPermissionPromptDismisser alertForPermissionForTesting(WebPageProxy& page, PermissionPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    Vector<String> buttonTitles;
    buttonTitles.append(promptAllowButtonText(request).get());
    for (auto& action : request.customization.additionalActions)
        buttonTitles.append(action.title);
    buttonTitles.append(promptDenyButtonText(request).get());

    Ref prompt = MediaPermissionPromptForTesting::create(promptMessageText(request).get(), promptInformativeText(request).get(), WTF::move(buttonTitles), WTF::move(request.customization.additionalActions), WTF::move(completionHandler));
    page.mediaPermissionPromptHandlerForTesting()(prompt.copyRef());

    return [prompt] {
        prompt->respond(std::nullopt);
    };
}

#endif // ENABLE(MEDIA_STREAM)



void requestAVCaptureAccessForType(MediaPermissionType type, CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

#if HAVE(AVCAPTUREDEVICE)
    RetainPtr mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto decisionHandler = makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL authorized) mutable {
        callOnMainRunLoop([completionHandler = WTF::move(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getAVCaptureDeviceClassSingleton() requestAccessForMediaType:mediaType.get() completionHandler:decisionHandler.get()];
#else
    UNUSED_PARAM(type);
    completionHandler(false);
#endif
}

MediaPermissionResult checkAVCaptureAccessForType(MediaPermissionType type)
{
#if HAVE(AVCAPTUREDEVICE)
    RetainPtr mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto authorizationStatus = [PAL::getAVCaptureDeviceClassSingleton() authorizationStatusForMediaType:mediaType.get()];
    if (authorizationStatus == AVAuthorizationStatusDenied || authorizationStatus == AVAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == AVAuthorizationStatusNotDetermined)
        return MediaPermissionResult::Unknown;
    return MediaPermissionResult::Granted;
#else
    UNUSED_PARAM(type);
    return MediaPermissionResult::Denied;
#endif
}

#if HAVE(SPEECHRECOGNIZER)

void requestSpeechRecognitionAccess(CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    auto decisionHandler = makeBlockPtr([completionHandler = WTF::move(completionHandler)](SFSpeechRecognizerAuthorizationStatus status) mutable {
        bool authorized = status == SFSpeechRecognizerAuthorizationStatusAuthorized;
        callOnMainRunLoop([completionHandler = WTF::move(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getSFSpeechRecognizerClassSingleton() requestAuthorization:decisionHandler.get()];
}

MediaPermissionResult checkSpeechRecognitionServiceAccess()
{
    auto authorizationStatus = [PAL::getSFSpeechRecognizerClassSingleton() authorizationStatus];
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
    auto recognizer = localeIdentifier.isEmpty() ? adoptNS([PAL::allocSFSpeechRecognizerInstance() init]) : adoptNS([PAL::allocSFSpeechRecognizerInstance() initWithLocale:[NSLocale localeWithLocaleIdentifier:localeIdentifier.createNSString().get()]]);
    return recognizer && [recognizer isAvailable];
}

#endif // HAVE(SPEECHRECOGNIZER)

} // namespace WebKit
