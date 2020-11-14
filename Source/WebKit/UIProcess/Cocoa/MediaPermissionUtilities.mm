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
#import <mutex>
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if HAVE(SPEECHRECOGNIZER)
#import <Speech/Speech.h>

SOFT_LINK_FRAMEWORK(Speech)
SOFT_LINK_CLASS(Speech, SFSpeechRecognizer)
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

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

#if HAVE(AVCAPTUREDEVICE)

void requestAVCaptureAccessForType(MediaPermissionType type, CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainThread());

    AVMediaType mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL authorized) mutable {
        if (isMainThread()) {
            completionHandler(authorized);
            return;
        }

        callOnMainThread([completionHandler = WTFMove(completionHandler), authorized]() mutable {
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
        if (!isMainThread()) {
            callOnMainThread([completionHandler = WTFMove(completionHandler), authorized]() mutable {
                completionHandler(authorized);
            });
            return;
        }
        completionHandler(authorized);
    });
    [getSFSpeechRecognizerClass() requestAuthorization:decisionHandler.get()];
}

MediaPermissionResult checkSpeechRecognitionServiceAccess()
{
    auto authorizationStatus = [getSFSpeechRecognizerClass() authorizationStatus];
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusDenied || authorizationStatus == SFSpeechRecognizerAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusAuthorized)
        return MediaPermissionResult::Granted;
    return MediaPermissionResult::Unknown;
}

#endif // HAVE(SPEECHRECOGNIZER)

} // namespace WebKit
