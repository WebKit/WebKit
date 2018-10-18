/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVAudioSession)

SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryAmbient, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategorySoloAmbient, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryPlayback, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryRecord, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryPlayAndRecord, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryAudioProcessing, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionModeDefault, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionModeVideoChat, NSString *)

#define AVAudioSession getAVAudioSessionClass()
#define AVAudioSessionCategoryAmbient getAVAudioSessionCategoryAmbient()
#define AVAudioSessionCategorySoloAmbient getAVAudioSessionCategorySoloAmbient()
#define AVAudioSessionCategoryPlayback getAVAudioSessionCategoryPlayback()
#define AVAudioSessionCategoryRecord getAVAudioSessionCategoryRecord()
#define AVAudioSessionCategoryPlayAndRecord getAVAudioSessionCategoryPlayAndRecord()
#define AVAudioSessionCategoryAudioProcessing getAVAudioSessionCategoryAudioProcessing()
#define AVAudioSessionModeDefault getAVAudioSessionModeDefault()
#define AVAudioSessionModeVideoChat getAVAudioSessionModeVideoChat()

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
public:
    AudioSessionPrivate(AudioSession*);
    AudioSession::CategoryType m_categoryOverride;
};

AudioSessionPrivate::AudioSessionPrivate(AudioSession*)
    : m_categoryOverride(AudioSession::None)
{
}

AudioSession::AudioSession()
    : m_private(std::make_unique<AudioSessionPrivate>(this))
{
}

AudioSession::~AudioSession()
{
}

void AudioSession::setCategory(CategoryType newCategory)
{
    LOG(Media, "AudioSession::setCategory() - category = %s", categoryName(newCategory));

    if (categoryOverride() && categoryOverride() != newCategory) {
        LOG(Media, "AudioSession::setCategory() - override set, NOT changing");
        return;
    }

    NSString *categoryString;
    NSString *categoryMode = AVAudioSessionModeDefault;
    AVAudioSessionCategoryOptions options = 0;
    AVAudioSessionRouteSharingPolicy policy = AVAudioSessionRouteSharingPolicyDefault;

    switch (newCategory) {
    case AmbientSound:
        categoryString = AVAudioSessionCategoryAmbient;
        break;
    case SoloAmbientSound:
        categoryString = AVAudioSessionCategorySoloAmbient;
        break;
    case MediaPlayback:
        categoryString = AVAudioSessionCategoryPlayback;
        policy = AVAudioSessionRouteSharingPolicyLongForm;
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
    [[AVAudioSession sharedInstance] setCategory:categoryString mode:categoryMode routeSharingPolicy:policy options:options error:&error];
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC)
    ASSERT(!error);
#endif
}

AudioSession::CategoryType AudioSession::category() const
{
    NSString *categoryString = [[AVAudioSession sharedInstance] category];
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
    static_assert(static_cast<size_t>(RouteSharingPolicy::LongForm) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyLongForm), "RouteSharingPolicy::LongForm is not AVAudioSessionRouteSharingPolicyLongForm as expected");
    static_assert(static_cast<size_t>(RouteSharingPolicy::Independent) == static_cast<size_t>(AVAudioSessionRouteSharingPolicyIndependent), "RouteSharingPolicy::Independent is not AVAudioSessionRouteSharingPolicyIndependent as expected");

    AVAudioSessionRouteSharingPolicy policy = [[AVAudioSession sharedInstance] routeSharingPolicy];
    ASSERT(static_cast<RouteSharingPolicy>(policy) <= RouteSharingPolicy::Independent);
    return static_cast<RouteSharingPolicy>(policy);
}

String AudioSession::routingContextUID() const
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC) && !PLATFORM(WATCHOS)
    return [[AVAudioSession sharedInstance] routingContextUID];
#else
    return emptyString();
#endif
}

void AudioSession::setCategoryOverride(CategoryType category)
{
    if (m_private->m_categoryOverride == category)
        return;

    m_private->m_categoryOverride = category;
    setCategory(category);
}

AudioSession::CategoryType AudioSession::categoryOverride() const
{
    return m_private->m_categoryOverride;
}

float AudioSession::sampleRate() const
{
    return [[AVAudioSession sharedInstance] sampleRate];
}

size_t AudioSession::bufferSize() const
{
    return [[AVAudioSession sharedInstance] IOBufferDuration] * sampleRate();
}

size_t AudioSession::numberOfOutputChannels() const
{
    return [[AVAudioSession sharedInstance] outputNumberOfChannels];
}

bool AudioSession::tryToSetActive(bool active)
{
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setActive:active withOptions:active ? 0 : AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&error];
    return !error;
}

size_t AudioSession::preferredBufferSize() const
{
    return [[AVAudioSession sharedInstance] preferredIOBufferDuration] * sampleRate();
}

void AudioSession::setPreferredBufferSize(size_t bufferSize)
{
    NSError *error = nil;
    float duration = bufferSize / sampleRate();
    [[AVAudioSession sharedInstance] setPreferredIOBufferDuration:duration error:&error];
    ASSERT(!error);
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(IOS_FAMILY)
