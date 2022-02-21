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
#import <wtf/RetainPtr.h>
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
    [center addObserver:self selector:@selector(interruption:) name:AVAudioSessionInterruptionNotification object:[PAL::getAVAudioSessionClass() sharedInstance]];

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

static WeakHashSet<AudioSessionIOS::CategoryChangedObserver>& audioSessionCategoryChangedObservers()
{
    static NeverDestroyed<WeakHashSet<AudioSessionIOS::CategoryChangedObserver>> observers;
    return observers;
}

void AudioSessionIOS::addAudioSessionCategoryChangedObserver(const CategoryChangedObserver& observer)
{
    audioSessionCategoryChangedObservers().add(observer);
    observer(AudioSession::sharedSession(), AudioSession::sharedSession().category());
}

static void setEligibleForSmartRouting(bool eligible)
{
#if PLATFORM(IOS)
    if (!AudioSession::shouldManageAudioSessionCategory())
        return;

    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    if (![session respondsToSelector:@selector(setEligibleForBTSmartRoutingConsideration:error:)]
        || ![session respondsToSelector:@selector(eligibleForBTSmartRoutingConsideration)])
        return;

    if (session.eligibleForBTSmartRoutingConsideration == eligible)
        return;

    NSError *error = nil;
    if (![session setEligibleForBTSmartRoutingConsideration:eligible error:&error])
        RELEASE_LOG_ERROR(Media, "failed to set eligible to %d with error: %@", eligible, error.localizedDescription);
#else
    UNUSED_PARAM(eligible);
#endif
}

AudioSessionIOS::AudioSessionIOS()
    : m_workQueue(WorkQueue::create("AudioSession Activation Queue"))
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_interruptionObserverHelper = adoptNS([[WebInterruptionObserverHelper alloc] initWithCallback:this]);
    END_BLOCK_OBJC_EXCEPTIONS

    m_workQueue->dispatch([] {
        setEligibleForSmartRouting(false);
    });

}

AudioSessionIOS::~AudioSessionIOS()
{
    [m_interruptionObserverHelper clearCallback];
}

void AudioSessionIOS::setHostProcessAttribution(audit_token_t auditToken)
{
#if ENABLE(APP_PRIVACY_REPORT) && !PLATFORM(MACCATALYST)
    NSError *error = nil;
    auto bundleProxy = [LSBundleProxy bundleProxyWithAuditToken:auditToken error:&error];
    if (error) {
        RELEASE_LOG_ERROR(WebRTC, "Failed to get attribution bundleID from audit token with error: %@.", error.localizedDescription);
        return;
    }

    [[PAL::getAVAudioSessionClass() sharedInstance] setHostProcessAttribution:@[ bundleProxy.bundleIdentifier ] error:&error];
    if (error)
        RELEASE_LOG_ERROR(WebRTC, "Failed to set attribution bundleID with error: %@.", error.localizedDescription);
#else
    UNUSED_PARAM(auditToken);
#endif
};

void AudioSessionIOS::setPresentingProcesses(Vector<audit_token_t>&& auditTokens)
{
#if HAVE(AUDIOSESSION_PROCESSASSERTION)
    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    if (![session respondsToSelector:@selector(setAuditTokensForProcessAssertion:error:)])
        return;

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

void AudioSessionIOS::setCategory(CategoryType newCategory, RouteSharingPolicy policy)
{
#if !HAVE(ROUTE_SHARING_POLICY_LONG_FORM_VIDEO)
    if (policy == RouteSharingPolicy::LongFormVideo)
        policy = RouteSharingPolicy::LongFormAudio;
#endif

    LOG(Media, "AudioSession::setCategory() - category = %s", convertEnumerationToString(newCategory).ascii().data());

    if (categoryOverride() !=  CategoryType::None && categoryOverride() != newCategory) {
        LOG(Media, "AudioSession::setCategory() - override set, NOT changing");
        return;
    }

    NSString *categoryString;
    NSString *categoryMode = AVAudioSessionModeDefault;
    AVAudioSessionCategoryOptions options = 0;

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
        categoryString = AVAudioSessionCategoryPlayAndRecord;
        categoryMode = AVAudioSessionModeVideoChat;
        options |= AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP | AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowAirPlay;
        break;
    case CategoryType::AudioProcessing:
        categoryString = AVAudioSessionCategoryAudioProcessing;
        break;
    case CategoryType::None:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    }

    bool needDeviceUpdate = false;
#if ENABLE(MEDIA_STREAM)
    auto preferredDeviceUID = AVAudioSessionCaptureDeviceManager::singleton().preferredAudioSessionDeviceUID();
    if ((newCategory == CategoryType::PlayAndRecord || newCategory == CategoryType::RecordAudio) && !preferredDeviceUID.isEmpty()) {
        if (m_lastSetPreferredAudioDeviceUID != preferredDeviceUID)
            needDeviceUpdate = true;
    } else
        m_lastSetPreferredAudioDeviceUID = emptyString();
#endif

    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    auto *currentCategory = [session category];
    auto currentOptions = [session categoryOptions];
    auto currentPolicy = [session routeSharingPolicy];
    auto needSessionUpdate = ![currentCategory isEqualToString:categoryString] || currentOptions != options || currentPolicy != static_cast<AVAudioSessionRouteSharingPolicy>(policy);

    if (!needSessionUpdate && !needDeviceUpdate)
        return;

    if (needSessionUpdate) {
        NSError *error = nil;
        [session setCategory:categoryString mode:categoryMode routeSharingPolicy:static_cast<AVAudioSessionRouteSharingPolicy>(policy) options:options error:&error];
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
        ASSERT(!error);
#endif
    }

#if ENABLE(MEDIA_STREAM)
    if (needDeviceUpdate) {
        AVAudioSessionCaptureDeviceManager::singleton().configurePreferredAudioCaptureDevice();
        m_lastSetPreferredAudioDeviceUID = AVAudioSessionCaptureDeviceManager::singleton().preferredAudioSessionDeviceUID();
    }
#endif
    for (auto& observer : audioSessionCategoryChangedObservers())
        observer(*this, category());
}

AudioSession::CategoryType AudioSessionIOS::category() const
{
    NSString *categoryString = [[PAL::getAVAudioSessionClass() sharedInstance] category];
    if ([categoryString isEqual:AVAudioSessionCategoryAmbient])
        return CategoryType::AmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategorySoloAmbient])
        return CategoryType::SoloAmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayback])
        return CategoryType::MediaPlayback;
    if ([categoryString isEqual:AVAudioSessionCategoryRecord])
        return CategoryType::RecordAudio;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayAndRecord])
        return CategoryType::PlayAndRecord;
    if ([categoryString isEqual:AVAudioSessionCategoryAudioProcessing])
        return CategoryType::AudioProcessing;
    return CategoryType::None;
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

    AVAudioSessionRouteSharingPolicy policy = [[PAL::getAVAudioSessionClass() sharedInstance] routeSharingPolicy];
    ASSERT(static_cast<RouteSharingPolicy>(policy) <= RouteSharingPolicy::LongFormVideo);
    return static_cast<RouteSharingPolicy>(policy);
}

String AudioSessionIOS::routingContextUID() const
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    return [[PAL::getAVAudioSessionClass() sharedInstance] routingContextUID];
#else
    return emptyString();
#endif
}

float AudioSessionIOS::sampleRate() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] sampleRate];
}

size_t AudioSessionIOS::bufferSize() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] IOBufferDuration] * sampleRate();
}

size_t AudioSessionIOS::numberOfOutputChannels() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] outputNumberOfChannels];
}

size_t AudioSessionIOS::maximumNumberOfOutputChannels() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] maximumOutputNumberOfChannels];
}

bool AudioSessionIOS::tryToSetActiveInternal(bool active)
{
    // We need to deactivate the session on another queue because the AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation option
    // means that AVAudioSession may synchronously unduck previously ducked clients. Activation needs to complete before this method
    // returns, so do it synchronously on the same serial queue.
    if (active) {
        bool success = false;
        m_workQueue->dispatchSync([&success] {
            NSError *error = nil;
            setEligibleForSmartRouting(true);
            [[PAL::getAVAudioSessionClass() sharedInstance] setActive:YES withOptions:0 error:&error];
            success = !error;
        });
        return success;
    }

    m_workQueue->dispatch([] {
        NSError *error = nil;
        [[PAL::getAVAudioSessionClass() sharedInstance] setActive:NO withOptions:0 error:&error];
        setEligibleForSmartRouting(false);
    });

    return true;
}

size_t AudioSessionIOS::preferredBufferSize() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] preferredIOBufferDuration] * sampleRate();
}

void AudioSessionIOS::setPreferredBufferSize(size_t bufferSize)
{
    NSError *error = nil;
    float duration = bufferSize / sampleRate();
    [[PAL::getAVAudioSessionClass() sharedInstance] setPreferredIOBufferDuration:duration error:&error];
    RELEASE_LOG_ERROR_IF(error, Media, "failed to set preferred buffer duration to %f with error: %@", duration, error.localizedDescription);
    ASSERT(!error);
}

bool AudioSessionIOS::isMuted() const
{
    return false;
}

void AudioSessionIOS::handleMutedStateChange()
{
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)
