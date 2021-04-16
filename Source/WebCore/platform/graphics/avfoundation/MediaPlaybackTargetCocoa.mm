/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "MediaPlaybackTargetCocoa.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create(AVOutputContext *context)
{
    return adoptRef(*new MediaPlaybackTargetCocoa(context));
}

MediaPlaybackTargetCocoa::MediaPlaybackTargetCocoa(AVOutputContext *context)
    : MediaPlaybackTarget()
    , m_outputContext(context)
{
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create()
{
    auto *routingContextUID = [[PAL::getAVAudioSessionClass() sharedInstance] routingContextUID];
    return adoptRef(*new MediaPlaybackTargetCocoa([PAL::getAVOutputContextClass() outputContextForID:routingContextUID]));
}
#endif

bool MediaPlaybackTargetCocoa::supportsRemoteVideoPlayback() const
{
    if (!m_outputContext)
        return false;

    if (![m_outputContext respondsToSelector:@selector(supportsMultipleOutputDevices)] || ![m_outputContext supportsMultipleOutputDevices] || ![m_outputContext respondsToSelector:@selector(outputDevices)]) {
        if (auto *outputDevice = [m_outputContext outputDevice]) {
            if (outputDevice.deviceFeatures & AVOutputDeviceFeatureVideo)
                return true;
        }

        return false;
    }

    for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
        if (outputDevice.deviceFeatures & AVOutputDeviceFeatureVideo)
            return true;
    }

    return false;
}

MediaPlaybackTargetCocoa::~MediaPlaybackTargetCocoa()
{
}

const MediaPlaybackTargetContext& MediaPlaybackTargetCocoa::targetContext() const
{
    m_context = MediaPlaybackTargetContext(m_outputContext.get());
    return m_context;
}

bool MediaPlaybackTargetCocoa::hasActiveRoute() const
{
    if (!m_outputContext)
        return false;

    if ([m_outputContext respondsToSelector:@selector(supportsMultipleOutputDevices)] && [m_outputContext supportsMultipleOutputDevices] && [m_outputContext respondsToSelector:@selector(outputDevices)]) {
        for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
            if (outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio))
                return true;
        }

        return false;
    }

    if ([m_outputContext respondsToSelector:@selector(outputDevice)]) {
        if (auto *outputDevice = [m_outputContext outputDevice])
            return outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio);
    }

    return m_outputContext.get().deviceName;
}

String MediaPlaybackTargetCocoa::deviceName() const
{
    if (!m_outputContext)
        return emptyString();

    if (![m_outputContext supportsMultipleOutputDevices])
        return [m_outputContext deviceName];

    auto outputDeviceNames = adoptNS([[NSMutableArray alloc] init]);
    for (AVOutputDevice *outputDevice in [m_outputContext outputDevices])
        [outputDeviceNames addObject:[outputDevice deviceName]];

    return [outputDeviceNames componentsJoinedByString:@" + "];
}

MediaPlaybackTargetCocoa* toMediaPlaybackTargetCocoa(MediaPlaybackTarget* rep)
{
    return const_cast<MediaPlaybackTargetCocoa*>(toMediaPlaybackTargetCocoa(const_cast<const MediaPlaybackTarget*>(rep)));
}

const MediaPlaybackTargetCocoa* toMediaPlaybackTargetCocoa(const MediaPlaybackTarget* rep)
{
    ASSERT_WITH_SECURITY_IMPLICATION(rep->targetType() == MediaPlaybackTarget::AVFoundation);
    return static_cast<const MediaPlaybackTargetCocoa*>(rep);
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
