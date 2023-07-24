/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "AudioSessionCocoa.h"

#if USE(AUDIO_SESSION) && PLATFORM(COCOA)

#import "Logging.h"
#import "NotImplemented.h"
#import <AVFoundation/AVAudioSession.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/WorkQueue.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

void AudioSessionCocoa::setEligibleForSmartRoutingInternal(bool eligible)
{
#if HAVE(AVAUDIOSESSION_SMARTROUTING)
    if (!AudioSession::shouldManageAudioSessionCategory())
        return;

    static bool supportsEligibleForBT = [PAL::getAVAudioSessionClass() instancesRespondToSelector:@selector(setEligibleForBTSmartRoutingConsideration:error:)]
        && [PAL::getAVAudioSessionClass() instancesRespondToSelector:@selector(eligibleForBTSmartRoutingConsideration)];
    if (!supportsEligibleForBT)
        return;

    RELEASE_LOG(Media, "AudioSession::setEligibleForSmartRouting() %s", eligible ? "true" : "false");

    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    if (session.eligibleForBTSmartRoutingConsideration == eligible)
        return;

    NSError *error = nil;
    if (![session setEligibleForBTSmartRoutingConsideration:eligible error:&error])
        RELEASE_LOG_ERROR(Media, "failed to set eligible to %d with error: %@", eligible, error.localizedDescription);
#else
    UNUSED_PARAM(eligible);
#endif
}

AudioSessionCocoa::AudioSessionCocoa()
    : m_workQueue(WorkQueue::create("AudioSession Activation Queue"))
{
}

AudioSessionCocoa::~AudioSessionCocoa()
{
    setEligibleForSmartRouting(false, ForceUpdate::Yes);
}

void AudioSessionCocoa::setEligibleForSmartRouting(bool isEligible, ForceUpdate forceUpdate)
{
    if (forceUpdate == ForceUpdate::No && m_isEligibleForSmartRouting == isEligible)
        return;

    m_isEligibleForSmartRouting = isEligible;
    m_workQueue->dispatch([this, isEligible] {
        setEligibleForSmartRoutingInternal(isEligible);
    });
}

bool AudioSessionCocoa::tryToSetActiveInternal(bool active)
{
#if HAVE(AVAUDIOSESSION)
    static bool supportsSharedInstance = [PAL::getAVAudioSessionClass() respondsToSelector:@selector(sharedInstance)];
    static bool supportsSetActive = [PAL::getAVAudioSessionClass() instancesRespondToSelector:@selector(setActive:withOptions:error:)];

    if (!supportsSharedInstance)
        return true;

    // We need to deactivate the session on another queue because the AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation option
    // means that AVAudioSession may synchronously unduck previously ducked clients. Activation needs to complete before this method
    // returns, so do it synchronously on the same serial queue.
    if (active) {
        bool success = false;
        setEligibleForSmartRouting(true);
        m_workQueue->dispatchSync([&success] {
            NSError *error = nil;
            if (supportsSetActive)
                [[PAL::getAVAudioSessionClass() sharedInstance] setActive:YES withOptions:0 error:&error];
            success = !error;
        });
        return success;
    }

    m_workQueue->dispatch([] {
        NSError *error = nil;
        if (supportsSetActive)
            [[PAL::getAVAudioSessionClass() sharedInstance] setActive:NO withOptions:0 error:&error];
    });
    setEligibleForSmartRouting(false);
#else
    UNUSED_PARAM(active);
    notImplemented();
#endif
    return true;
}

void AudioSessionCocoa::setCategory(CategoryType newCategory, Mode, RouteSharingPolicy)
{
    // Disclaim support for Smart Routing when we are not generating audio.
    setEligibleForSmartRouting(isActive() && newCategory != AudioSessionCategory::None);
}

}

#endif // USE(AUDIO_SESSION) && PLATFORM(COCOA)
