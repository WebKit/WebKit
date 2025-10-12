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
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>

#if PLATFORM(IOS_FAMILY)
#import "MediaSessionHelperIOS.h"
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioSessionCocoa);

void AudioSessionCocoa::setEligibleForSmartRoutingInternal(bool eligible)
{
#if HAVE(AVAUDIOSESSION_SMARTROUTING)
    if (!AudioSession::shouldManageAudioSessionCategory())
        return;

    // FIXME: This is a safer cpp false positive (160259918).
    SUPPRESS_UNRETAINED_ARG static bool supportsEligibleForBT = [PAL::getAVAudioSessionClassSingleton() instancesRespondToSelector:@selector(setEligibleForBTSmartRoutingConsideration:error:)] && [PAL::getAVAudioSessionClassSingleton() instancesRespondToSelector:@selector(eligibleForBTSmartRoutingConsideration)];
    if (!supportsEligibleForBT)
        return;

    RELEASE_LOG(Media, "AudioSession::setEligibleForSmartRouting() %s", eligible ? "true" : "false");

    // FIXME: This is a safer cpp false positive (160259918).
    SUPPRESS_UNRETAINED_ARG AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
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
    : m_workQueue(WorkQueue::create("AudioSession Activation Queue"_s))
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
    m_workQueue->dispatch([isEligible] {
        setEligibleForSmartRoutingInternal(isEligible);
    });
}

bool AudioSessionCocoa::tryToSetActiveInternal(bool active)
{
#if HAVE(AVAUDIOSESSION)
    // FIXME: This is a safer cpp false positive (160259918).
    SUPPRESS_UNRETAINED_ARG static bool supportsSharedInstance = [PAL::getAVAudioSessionClassSingleton() respondsToSelector:@selector(sharedInstance)];
    // FIXME: This is a safer cpp false positive (160259918).
    SUPPRESS_UNRETAINED_ARG static bool supportsSetActive = [PAL::getAVAudioSessionClassSingleton() instancesRespondToSelector:@selector(setActive:withOptions:error:)];

    if (!supportsSharedInstance)
        return true;

    // We need to deactivate the session on another queue because the AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation option
    // means that AVAudioSession may synchronously unduck previously ducked clients. Activation needs to complete before this method
    // returns, so do it synchronously on the same serial queue.
    if (active) {
#if PLATFORM(IOS_FAMILY) && !ENABLE(EXTENSION_CAPABILITIES)
        ASSERT(!isInAuxiliaryProcess() || MediaSessionHelper::sharedHelper().presentedApplicationPID());
#endif

        bool success = false;
        setEligibleForSmartRouting(true);
        m_workQueue->dispatchSync([&success] {
            NSError *error = nil;
            if (supportsSetActive) {
                // FIXME: This is a safer cpp false positive (160259918).
                SUPPRESS_UNRETAINED_ARG [[PAL::getAVAudioSessionClassSingleton() sharedInstance] setActive:YES withOptions:0 error:&error];
            }
            if (error)
                RELEASE_LOG_ERROR(Media, "failed to activate audio session, error: %@", error.localizedDescription);
            success = !error;
        });
        return success;
    }

    m_workQueue->dispatch([] {
        NSError *error = nil;
        if (supportsSetActive) {
            // FIXME: This is a safer cpp false positive (160259918).
            SUPPRESS_UNRETAINED_ARG [[PAL::getAVAudioSessionClassSingleton() sharedInstance] setActive:NO withOptions:0 error:&error];
        }
        if (error)
            RELEASE_LOG_ERROR(Media, "failed to deactivate audio session, error: %@", error.localizedDescription);
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
