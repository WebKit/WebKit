/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "MockMediaDeviceRoute.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#import "WebMockMediaDeviceRoute.h"
#import <AVKit/AVKit.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlaybackUserInterfaceMediaSelectionOption)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockMediaDeviceRoute);

Ref<MockMediaDeviceRoute> MockMediaDeviceRoute::create()
{
    return adoptRef(*new MockMediaDeviceRoute());
}

MockMediaDeviceRoute::MockMediaDeviceRoute()
    : m_platformRoute { adoptNS([[WebMockMediaDeviceRoute alloc] init]) }
{
}

WebMediaDevicePlatformRoute *MockMediaDeviceRoute::platformRoute() const
{
    return m_platformRoute.get();
}

void MockMediaDeviceRoute::setURLCallback(MockMediaDeviceRouteURLCallback* urlCallback)
{
    [m_platformRoute setURLCallback:urlCallback];
}

String MockMediaDeviceRoute::deviceName() const
{
    return [m_platformRoute routeDisplayName];
}

void MockMediaDeviceRoute::setDeviceName(const String& deviceName)
{
    [m_platformRoute setRouteDisplayName:deviceName.createNSString().get()];
}

bool MockMediaDeviceRoute::ready() const
{
    return [m_platformRoute isReady];
}

void MockMediaDeviceRoute::setReady(bool ready)
{
    [m_platformRoute setReady:ready];
}

bool MockMediaDeviceRoute::playing() const
{
    return [m_platformRoute isPlaying];
}

void MockMediaDeviceRoute::setPlaying(bool playing)
{
    [m_platformRoute setPlaying:playing];
}

bool MockMediaDeviceRoute::hasPlaybackError() const
{
    return !![m_platformRoute error];
}

void MockMediaDeviceRoute::setHasPlaybackError(bool)
{
    RetainPtr error = [NSError errorWithDomain:WebMockMediaDeviceRouteErrorDomain code:WebMockMediaDeviceRouteErrorCodePlaybackError userInfo:nil];
    [m_platformRoute setError:error.get()];
}

Vector<MockMediaDeviceRoute::AudioOption> MockMediaDeviceRoute::audioOptions() const
{
    NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *options = [m_platformRoute audioOptions];
    return Vector<AudioOption>(options.count, [&](size_t i) {
        AVPlaybackUserInterfaceMediaSelectionOption *option = options[i];
        return AudioOption {
            option.displayName,
            option.identifier,
            option.extendedLanguageTag,
        };
    });
}

void MockMediaDeviceRoute::setAudioOptions(const Vector<AudioOption>& audioOptions)
{
    RetainPtr options = [NSMutableArray arrayWithCapacity:audioOptions.size()];
    for (auto& option : audioOptions) {
        RetainPtr platformOption = adoptNS([allocAVPlaybackUserInterfaceMediaSelectionOptionInstance() initWithDisplayName:option.displayName.createNSString().get() identifier:option.identifier.createNSString().get() extendedLanguageTag:option.extendedLanguageTag.createNSString().get() mediaCharacteristics:@[]]);
        [options addObject:platformOption.get()];
    }
    [m_platformRoute setAudioOptions:options.get()];
}

float MockMediaDeviceRoute::playbackRate() const
{
    return [m_platformRoute playbackSpeed];
}

void MockMediaDeviceRoute::setPlaybackRate(float playbackRate)
{
    [m_platformRoute setPlaybackSpeed:playbackRate];
}

float MockMediaDeviceRoute::currentPlaybackPosition() const
{
    return CMTimeGetSeconds([[m_platformRoute playbackPosition] position]);
}

void MockMediaDeviceRoute::setCurrentPlaybackPosition(float currentPlaybackPosition)
{
    [m_platformRoute seekToPosition:CMTimeMakeWithSeconds(currentPlaybackPosition, 1000) tolerance:kCMTimeZero];
}

MockMediaDeviceRoute::TimeRange MockMediaDeviceRoute::timeRange() const
{
    CMTimeRange platformTimeRange = [m_platformRoute timeRange];
    return { CMTimeGetSeconds(platformTimeRange.start), CMTimeGetSeconds(platformTimeRange.duration) };
}

void MockMediaDeviceRoute::setTimeRange(const MockMediaDeviceRoute::TimeRange& timeRange)
{
    CMTime start = CMTimeMakeWithSeconds(timeRange.start, 1000);
    CMTime duration = CMTimeMakeWithSeconds(timeRange.duration, 1000);
    [m_platformRoute setTimeRange:CMTimeRangeMake(start, duration)];
}

float MockMediaDeviceRoute::volume() const
{
    return [m_platformRoute volume];
}

void MockMediaDeviceRoute::setVolume(float volume)
{
    [m_platformRoute setVolume:volume];
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
