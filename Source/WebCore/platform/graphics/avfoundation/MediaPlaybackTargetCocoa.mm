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

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

MediaPlaybackTargetContextCocoa::MediaPlaybackTargetContextCocoa(RetainPtr<AVOutputContext>&& outputContext)
    : MediaPlaybackTargetContext(Type::AVOutputContext)
    , m_outputContext(WTFMove(outputContext))
{
    ASSERT(m_outputContext);
}

MediaPlaybackTargetContextCocoa::~MediaPlaybackTargetContextCocoa() = default;

RetainPtr<AVOutputContext> MediaPlaybackTargetContextCocoa::outputContext() const
{
    return m_outputContext;
}

String MediaPlaybackTargetContextCocoa::deviceName() const
{
    if (![m_outputContext supportsMultipleOutputDevices])
        return [m_outputContext deviceName];

    auto outputDeviceNames = adoptNS([[NSMutableArray alloc] init]);
    for (AVOutputDevice *outputDevice in [m_outputContext outputDevices])
        [outputDeviceNames addObject:[outputDevice deviceName]];

    return [outputDeviceNames componentsJoinedByString:@" + "];
}

bool MediaPlaybackTargetContextCocoa::hasActiveRoute() const
{
    if ([m_outputContext supportsMultipleOutputDevices]) {
        for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
            if (outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio))
                return true;
        }
    } else {
        if (auto *outputDevice = [m_outputContext outputDevice])
            return outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio);
    }
    return m_outputContext.get().deviceName;
}
bool MediaPlaybackTargetContextCocoa::supportsRemoteVideoPlayback() const
{
    for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
        if (outputDevice.deviceFeatures & AVOutputDeviceFeatureVideo)
            return true;
    }

    return false;
}

Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create(MediaPlaybackTargetContextCocoa&& context)
{
    return adoptRef(*new MediaPlaybackTargetCocoa(WTFMove(context)));
}

MediaPlaybackTargetCocoa::MediaPlaybackTargetCocoa(MediaPlaybackTargetContextCocoa&& context)
    : m_context(context.outputContext())
{
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
Ref<MediaPlaybackTargetCocoa> MediaPlaybackTargetCocoa::create()
{
    auto *routingContextUID = [[PAL::getAVAudioSessionClassSingleton() sharedInstance] routingContextUID];
    return adoptRef(*new MediaPlaybackTargetCocoa(MediaPlaybackTargetContextCocoa([PAL::getAVOutputContextClassSingleton() outputContextForID:routingContextUID])));
}
#endif

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
