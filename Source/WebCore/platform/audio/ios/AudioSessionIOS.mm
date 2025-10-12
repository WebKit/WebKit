/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#import "AudioSessionIOS.h"

#if USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)

#import "AVAudioSessionCaptureDeviceManager.h"
#import "Logging.h"
#import <AVFoundation/AVAudioSession.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/LoggerHelper.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@interface WebInterruptionObserverHelper : NSObject {
    WebCore::AudioSession* _callback;
}

- (id)initWithCallback:(WebCore::AudioSession*)callback;
- (void)clearCallback;
- (void)interruption:(NSNotification *)notification;
@end

@implementation WebInterruptionObserverHelper

- (id)initWithCallback:(WebCore::AudioSession*)callback
{
    if (!(self = [super init]))
        return nil;

    _callback = callback;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(interruption:) name:AVAudioSessionInterruptionNotification object:[PAL::getAVAudioSessionClassSingleton() sharedInstance]];

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)clearCallback
{
    _callback = nil;
}

- (void)interruption:(NSNotification *)notification
{
    if (!_callback)
        return;

    NSUInteger type = [[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    auto flags = (type == AVAudioSessionInterruptionTypeEnded && [[[notification userInfo] objectForKey:AVAudioSessionInterruptionOptionKey] unsignedIntegerValue] == AVAudioSessionInterruptionOptionShouldResume) ? WebCore::AudioSession::MayResume::Yes : WebCore::AudioSession::MayResume::No;

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self), type, flags]() mutable {
        auto* callback = protectedSelf->_callback;
        if (!callback)
            return;

        if (type == AVAudioSessionInterruptionTypeBegan)
            callback->beginInterruption();
        else
            callback->endInterruption(flags);
    });
}
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioSessionIOS);

static WeakHashSet<AudioSessionIOS::CategoryChangedObserver>& audioSessionCategoryChangedObservers()
{
    static NeverDestroyed<WeakHashSet<AudioSessionIOS::CategoryChangedObserver>> observers;
    return observers;
}

void AudioSessionIOS::addAudioSessionCategoryChangedObserver(const CategoryChangedObserver& observer)
{
    audioSessionCategoryChangedObservers().add(observer);
    observer(AudioSession::singleton(), AudioSession::singleton().category());
}

Ref<AudioSessionIOS> AudioSessionIOS::create()
{
    return adoptRef(*new AudioSessionIOS);
}

AudioSessionIOS::AudioSessionIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    lazyInitialize(m_interruptionObserverHelper, adoptNS([[WebInterruptionObserverHelper alloc] initWithCallback:this]));
    END_BLOCK_OBJC_EXCEPTIONS
}

AudioSessionIOS::~AudioSessionIOS()
{
    [m_interruptionObserverHelper clearCallback];
}

void AudioSessionIOS::setHostProcessAttribution(audit_token_t auditToken)
{
#if ENABLE(APP_PRIVACY_REPORT) && !PLATFORM(MACCATALYST)
    ALWAYS_LOG(LOGIDENTIFIER);

    NSError *error = nil;
    auto bundleProxy = [LSBundleProxy bundleProxyWithAuditToken:auditToken error:&error];
    if (error) {
        RELEASE_LOG_ERROR(WebRTC, "Failed to get attribution bundleID from audit token with error: %@.", error.localizedDescription);
        return;
    }

    auto bundleIdentifier = bundleProxy.bundleIdentifier;
    if (!bundleIdentifier) {
        RELEASE_LOG_ERROR(WebRTC, "-[LSBundleProxy bundleIdentifier] returned nil!");
        return;
    }

    [[PAL::getAVAudioSessionClassSingleton() sharedInstance] setHostProcessAttribution:@[ bundleIdentifier ] error:&error];
    if (error)
        RELEASE_LOG_ERROR(WebRTC, "Failed to set attribution bundleID with error: %@.", error.localizedDescription);
#else
    UNUSED_PARAM(auditToken);
#endif
};

void AudioSessionIOS::setPresentingProcesses(Vector<audit_token_t>&& auditTokens)
{
#if HAVE(AUDIOSESSION_PROCESSASSERTION)
    ALWAYS_LOG(LOGIDENTIFIER);

    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    auto nsAuditTokens = adoptNS([[NSMutableArray alloc] init]);
    for (auto& token : auditTokens) {
        auto nsToken = adoptNS([[NSData alloc] initWithBytes:token.val length:sizeof(token.val)]);
        [nsAuditTokens addObject:nsToken.get()];
    }

    NSError *error = nil;
    [session setAuditTokensForProcessAssertion:nsAuditTokens.get() error:&error];
    if (error)
        RELEASE_LOG_ERROR(Media, "Failed to set audit tokens for process assertion with error: %@", error.localizedDescription);
#else
    UNUSED_PARAM(auditTokens);
#endif
}

void AudioSessionIOS::setCategory(CategoryType newCategory, Mode newMode, RouteSharingPolicy policy)
{
#if !HAVE(ROUTE_SHARING_POLICY_LONG_FORM_VIDEO)
    if (policy == RouteSharingPolicy::LongFormVideo)
        policy = RouteSharingPolicy::LongFormAudio;
#endif

    auto identifier = LOGIDENTIFIER;

    AudioSessionCocoa::setCategory(newCategory, newMode, policy);

    if (categoryOverride() != CategoryType::None && categoryOverride() != newCategory) {
        ALWAYS_LOG(identifier, "override set, NOT changing");
        return;
    }

    NSString *categoryString;
    AVAudioSessionCategoryOptions options = 0;

#if PLATFORM(IOS_FAMILY_SIMULATOR)
    m_isFakingPlayAndRecordForTesting = false;
#endif
    switch (newCategory) {
    case CategoryType::AmbientSound:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    case CategoryType::SoloAmbientSound:
        categoryString = AVAudioSessionCategorySoloAmbient;
        break;
    case CategoryType::MediaPlayback:
        categoryString = AVAudioSessionCategoryPlayback;
        break;
    case CategoryType::RecordAudio:
        categoryString = AVAudioSessionCategoryRecord;
        break;
    case CategoryType::PlayAndRecord:
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        // We prevent setting category to AVAudioSessionCategoryPlayAndRecord as it may trigger TCC prompts.
        m_isFakingPlayAndRecordForTesting = true;
        categoryString = AVAudioSessionCategoryPlayback;
#else
        categoryString = AVAudioSessionCategoryPlayAndRecord;
        // FIXME: Stop using `AVAudioSessionCategoryOptionAllowBluetooth` as it is deprecated (rdar://145294046).
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options |= AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP | AVAudioSessionCategoryOptionAllowAirPlay;
        ALLOW_DEPRECATED_DECLARATIONS_END
#if ENABLE(MEDIA_STREAM)
        if (!AVAudioSessionCaptureDeviceManager::singleton().isReceiverPreferredSpeaker())
#endif
            options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
#endif
        break;
    case CategoryType::AudioProcessing:
        categoryString = AVAudioSessionCategoryAudioProcessing;
        break;
    case CategoryType::None:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    }

    NSString *modeString = [&] {
        switch (newMode) {
        case Mode::MoviePlayback:
            return AVAudioSessionModeMoviePlayback;
        case Mode::VideoChat:
#if ENABLE(MEDIA_STREAM)
            if (AVAudioSessionCaptureDeviceManager::singleton().isReceiverPreferredSpeaker())
                return AVAudioSessionModeVoiceChat;
#endif
            return AVAudioSessionModeVideoChat;
        case Mode::Default:
            break;
        }
        return AVAudioSessionModeDefault;
    }();

    bool needDeviceUpdate = false;
#if ENABLE(MEDIA_STREAM)
    auto preferredMicrophoneID = AVAudioSessionCaptureDeviceManager::singleton().preferredMicrophoneID();
    if ((newCategory == CategoryType::PlayAndRecord || newCategory == CategoryType::RecordAudio) && !preferredMicrophoneID.isEmpty()) {
        if (m_lastSetPreferredMicrophoneID != preferredMicrophoneID)
            needDeviceUpdate = true;
    } else
        m_lastSetPreferredMicrophoneID = emptyString();
#endif

    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    auto *currentCategory = [session category];
    auto *currentMode = [session mode];
    auto currentOptions = [session categoryOptions];
    auto currentPolicy = [session routeSharingPolicy];
    auto needSessionUpdate = ![currentCategory isEqualToString:categoryString] || ![currentMode isEqualToString:modeString] || currentOptions != options || currentPolicy != static_cast<AVAudioSessionRouteSharingPolicy>(policy);

    if (!needSessionUpdate && !needDeviceUpdate)
        return;

    if (needSessionUpdate) {
        ALWAYS_LOG(identifier, newCategory, ", mode = ", newMode);
        NSError *error = nil;
        [session setCategory:categoryString mode:modeString routeSharingPolicy:static_cast<AVAudioSessionRouteSharingPolicy>(policy) options:options error:&error];
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
        ASSERT(!error);
#endif
    }

#if ENABLE(MEDIA_STREAM)
    if (needDeviceUpdate) {
        AVAudioSessionCaptureDeviceManager::singleton().configurePreferredMicrophone();
        m_lastSetPreferredMicrophoneID = AVAudioSessionCaptureDeviceManager::singleton().preferredMicrophoneID();
        ALWAYS_LOG(identifier, "prefered microphone = ", m_lastSetPreferredMicrophoneID);
    }
#endif
    for (auto& observer : audioSessionCategoryChangedObservers())
        observer(*this, category());
}

AudioSession::CategoryType AudioSessionIOS::category() const
{
    NSString *categoryString = [[PAL::getAVAudioSessionClassSingleton() sharedInstance] category];
    if ([categoryString isEqual:AVAudioSessionCategoryAmbient])
        return CategoryType::AmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategorySoloAmbient])
        return CategoryType::SoloAmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayback]) {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        if (m_isFakingPlayAndRecordForTesting)
            return CategoryType::PlayAndRecord;
#endif
        return CategoryType::MediaPlayback;
    }
    if ([categoryString isEqual:AVAudioSessionCategoryRecord])
        return CategoryType::RecordAudio;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayAndRecord])
        return CategoryType::PlayAndRecord;
    if ([categoryString isEqual:AVAudioSessionCategoryAudioProcessing])
        return CategoryType::AudioProcessing;
    return CategoryType::None;
}

AudioSession::Mode AudioSessionIOS::mode() const
{
    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    NSString *modeString = [session mode];
    if ([modeString isEqual:AVAudioSessionModeVideoChat] || [modeString isEqual:AVAudioSessionModeVoiceChat])
        return Mode::VideoChat;
    if ([modeString isEqual:AVAudioSessionModeMoviePlayback])
        return Mode::MoviePlayback;
    return Mode::Default;
}

RouteSharingPolicy AudioSessionIOS::routeSharingPolicy() const
{
    static_assert(static_cast<size_t>(RouteSharingPolicy::Default) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyDefault), "RouteSharingPolicy::Default is not AVAudioSessionRouteSharingPolicyDefault as expected");
#if HAVE(ROUTE_SHARING_POLICY_LONG_FORM_VIDEO)
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongFormAudio) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyLongFormAudio), "RouteSharingPolicy::LongFormAudio is not AVAudioSessionRouteSharingPolicyLongFormAudio as expected");
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongFormVideo) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyLongFormVideo), "RouteSharingPolicy::LongFormVideo is not AVAudioSessionRouteSharingPolicyLongFormVideo as expected");
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongFormAudio) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyLongForm), "RouteSharingPolicy::LongFormAudio is not AVAudioSessionRouteSharingPolicyLongForm as expected");
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    static_assert(static_cast<size_t>(RouteSharingPolicy::Independent) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyIndependent), "RouteSharingPolicy::Independent is not AVAudioSessionRouteSharingPolicyIndependent as expected");

    AVAudioSessionRouteSharingPolicy policy = [[PAL::getAVAudioSessionClassSingleton() sharedInstance] routeSharingPolicy];
    ASSERT(static_cast<RouteSharingPolicy>(policy) <= RouteSharingPolicy::LongFormVideo);
    return static_cast<RouteSharingPolicy>(policy);
}

String AudioSessionIOS::routingContextUID() const
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] routingContextUID];
#else
    return emptyString();
#endif
}

float AudioSessionIOS::sampleRate() const
{
    return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] sampleRate];
}

size_t AudioSessionIOS::bufferSize() const
{
    return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] IOBufferDuration] * sampleRate();
}

size_t AudioSessionIOS::numberOfOutputChannels() const
{
    return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] outputNumberOfChannels];
}

size_t AudioSessionIOS::maximumNumberOfOutputChannels() const
{
    return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] maximumOutputNumberOfChannels];
}

size_t AudioSessionIOS::preferredBufferSize() const
{
// FIXME: rdar://138773933
IGNORE_WARNINGS_BEGIN("objc-multiple-method-names")
     return [[PAL::getAVAudioSessionClassSingleton() sharedInstance] preferredIOBufferDuration] * sampleRate();
IGNORE_WARNINGS_END
}

void AudioSessionIOS::setPreferredBufferSize(size_t bufferSize)
{
    ALWAYS_LOG(LOGIDENTIFIER, bufferSize);

    NSError *error = nil;
    float duration = bufferSize / sampleRate();
    [[PAL::getAVAudioSessionClassSingleton() sharedInstance] setPreferredIOBufferDuration:duration error:&error];
    RELEASE_LOG_ERROR_IF(error, Media, "failed to set preferred buffer duration to %f with error: %@", duration, error.localizedDescription);
    ASSERT(!error);
}

size_t AudioSessionIOS::outputLatency() const
{
    auto latency = [[PAL::getAVAudioSessionClassSingleton() sharedInstance] outputLatency];
    return latency * sampleRate();
}

bool AudioSessionIOS::isMuted() const
{
    return false;
}

void AudioSessionIOS::handleMutedStateChange()
{
}

void AudioSessionIOS::updateSpatialExperience()
{
#if PLATFORM(VISION)
    AVAudioSessionSoundStageSize size = [&] {
        switch (m_soundStageSize) {
        case AudioSession::SoundStageSize::Automatic:
            return AVAudioSessionSoundStageSizeAutomatic;
        case AudioSession::SoundStageSize::Small:
            return AVAudioSessionSoundStageSizeSmall;
        case AudioSession::SoundStageSize::Medium:
            return AVAudioSessionSoundStageSizeMedium;
        case AudioSession::SoundStageSize::Large:
            return AVAudioSessionSoundStageSizeLarge;
        };
        ASSERT_NOT_REACHED();
        return AVAudioSessionSoundStageSizeAutomatic;
    }();
    NSError *error = nil;
    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    if (m_sceneIdentifier.length()) {
        [session setIntendedSpatialExperience:AVAudioSessionSpatialExperienceHeadTracked options:@{
            @"AVAudioSessionSpatialExperienceOptionSoundStageSize" : @(size),
            @"AVAudioSessionSpatialExperienceOptionAnchoringStrategy" : @(AVAudioSessionAnchoringStrategyScene),
            @"AVAudioSessionSpatialExperienceOptionSceneIdentifier" : m_sceneIdentifier.createNSString().get()
        } error:&error];
    } else {
        [session setIntendedSpatialExperience:AVAudioSessionSpatialExperienceHeadTracked options:@{
            @"AVAudioSessionSpatialExperienceOptionSoundStageSize" : @(size),
            @"AVAudioSessionSpatialExperienceOptionAnchoringStrategy" : @(AVAudioSessionAnchoringStrategyAutomatic)
        } error:&error];
    }

    if (error)
        ALWAYS_LOG(error.localizedDescription.UTF8String);
#endif
}

void AudioSessionIOS::setSceneIdentifier(const String& sceneIdentifier)
{
    if (m_sceneIdentifier == sceneIdentifier)
        return;
    m_sceneIdentifier = sceneIdentifier;
    ALWAYS_LOG(LOGIDENTIFIER, sceneIdentifier);

    updateSpatialExperience();
}

void AudioSessionIOS::setSoundStageSize(SoundStageSize size)
{
    if (m_soundStageSize == size)
        return;
    m_soundStageSize = size;
    ALWAYS_LOG(LOGIDENTIFIER, size);

    updateSpatialExperience();
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)
