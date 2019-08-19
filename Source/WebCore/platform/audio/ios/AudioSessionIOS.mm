/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#import "AudioSession.h"

#if USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import <AVFoundation/AVAudioSession.h>
#import <objc/runtime.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

#if !LOG_DISABLED
static const char* categoryName(AudioSession::CategoryType category)
{
#define CASE(category) case AudioSession::category: return #category
    switch (category) {
        CASE(None);
        CASE(AmbientSound);
        CASE(SoloAmbientSound);
        CASE(MediaPlayback);
        CASE(RecordAudio);
        CASE(PlayAndRecord);
        CASE(AudioProcessing);
    }
    
    ASSERT_NOT_REACHED();
    return "";
}
#endif

class AudioSessionPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AudioSessionPrivate(AudioSession*);
    AudioSession::CategoryType m_categoryOverride;
    OSObjectPtr<dispatch_queue_t> m_dispatchQueue;
};

AudioSessionPrivate::AudioSessionPrivate(AudioSession*)
    : m_categoryOverride(AudioSession::None)
{
}

AudioSession::AudioSession()
    : m_private(makeUnique<AudioSessionPrivate>(this))
{
}

AudioSession::~AudioSession()
{
}

void AudioSession::setCategory(CategoryType newCategory, RouteSharingPolicy policy)
{
#if !HAVE(ROUTE_SHARING_POLICY_LONG_FORM_VIDEO)
    if (policy == RouteSharingPolicy::LongFormVideo)
        policy = RouteSharingPolicy::LongFormAudio;
#endif

    LOG(Media, "AudioSession::setCategory() - category = %s", categoryName(newCategory));

    if (categoryOverride() && categoryOverride() != newCategory) {
        LOG(Media, "AudioSession::setCategory() - override set, NOT changing");
        return;
    }

    NSString *categoryString;
    NSString *categoryMode = AVAudioSessionModeDefault;
    AVAudioSessionCategoryOptions options = 0;

    switch (newCategory) {
    case AmbientSound:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    case SoloAmbientSound:
        categoryString = AVAudioSessionCategorySoloAmbient;
        break;
    case MediaPlayback:
        categoryString = AVAudioSessionCategoryPlayback;
        break;
    case RecordAudio:
        categoryString = AVAudioSessionCategoryRecord;
        break;
    case PlayAndRecord:
        categoryString = AVAudioSessionCategoryPlayAndRecord;
        categoryMode = AVAudioSessionModeVideoChat;
        options |= AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP | AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowAirPlay;
        break;
    case AudioProcessing:
        categoryString = AVAudioSessionCategoryAudioProcessing;
        break;
    case None:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    }

    NSError *error = nil;
    [[PAL::getAVAudioSessionClass() sharedInstance] setCategory:categoryString mode:categoryMode routeSharingPolicy:static_cast<AVAudioSessionRouteSharingPolicy>(policy) options:options error:&error];
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
    ASSERT(!error);
#endif
}

AudioSession::CategoryType AudioSession::category() const
{
    NSString *categoryString = [[PAL::getAVAudioSessionClass() sharedInstance] category];
    if ([categoryString isEqual:AVAudioSessionCategoryAmbient])
        return AmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategorySoloAmbient])
        return SoloAmbientSound;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayback])
        return MediaPlayback;
    if ([categoryString isEqual:AVAudioSessionCategoryRecord])
        return RecordAudio;
    if ([categoryString isEqual:AVAudioSessionCategoryPlayAndRecord])
        return PlayAndRecord;
    if ([categoryString isEqual:AVAudioSessionCategoryAudioProcessing])
        return AudioProcessing;
    return None;
}

RouteSharingPolicy AudioSession::routeSharingPolicy() const
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

String AudioSession::routingContextUID() const
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    return [[PAL::getAVAudioSessionClass() sharedInstance] routingContextUID];
#else
    return emptyString();
#endif
}

void AudioSession::setCategoryOverride(CategoryType category)
{
    if (m_private->m_categoryOverride == category)
        return;

    m_private->m_categoryOverride = category;
    setCategory(category, RouteSharingPolicy::Default);
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    return m_private->m_categoryOverride;
}

float AudioSession::sampleRate() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] sampleRate];
}

size_t AudioSession::bufferSize() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] IOBufferDuration] * sampleRate();
}

size_t AudioSession::numberOfOutputChannels() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] outputNumberOfChannels];
}

bool AudioSession::tryToSetActiveInternal(bool active)
{
    __block NSError* error = nil;

    if (!m_private->m_dispatchQueue)
        m_private->m_dispatchQueue = adoptOSObject(dispatch_queue_create("AudioSession Activation Queue", DISPATCH_QUEUE_SERIAL));

    // We need to deactivate the session on another queue because the AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation option
    // means that AVAudioSession may synchronously unduck previously ducked clients. Activation needs to complete before this method
    // returns, so do it synchronously on the same serial queue.
    if (active) {
        dispatch_sync(m_private->m_dispatchQueue.get(), ^{
            [[PAL::getAVAudioSessionClass() sharedInstance] setActive:YES withOptions:0 error:&error];
        });

        return !error;
    }

    dispatch_async(m_private->m_dispatchQueue.get(), ^{
        [[PAL::getAVAudioSessionClass() sharedInstance] setActive:NO withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&error];
    });

    return true;
}

size_t AudioSession::preferredBufferSize() const
{
    return [[PAL::getAVAudioSessionClass() sharedInstance] preferredIOBufferDuration] * sampleRate();
}

void AudioSession::setPreferredBufferSize(size_t bufferSize)
{
    NSError *error = nil;
    float duration = bufferSize / sampleRate();
    [[PAL::getAVAudioSessionClass() sharedInstance] setPreferredIOBufferDuration:duration error:&error];
    ASSERT(!error);
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)
