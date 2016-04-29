/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPlaybackSessionInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "AVKitSPI.h"
#import "IntRect.h"
#import "MediaTimeAVFoundation.h"
#import "TimeRanges.h"
#import "WebPlaybackControlsManager.h"
#import "WebPlaybackSessionModel.h"
#import <AVFoundation/AVFoundation.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

using namespace WebCore;

namespace WebCore {

WebPlaybackSessionInterfaceMac::~WebPlaybackSessionInterfaceMac()
{
}

void WebPlaybackSessionInterfaceMac::setWebPlaybackSessionModel(WebPlaybackSessionModel* model)
{
    m_playbackSessionModel = model;
}

void WebPlaybackSessionInterfaceMac::setClient(WebPlaybackSessionInterfaceMacClient* client)
{
    m_client = client;

    if (m_client) {
        float rate = [playBackControlsManager() rate];
        m_client->rateChanged(!!rate, rate);
    }
}

void WebPlaybackSessionInterfaceMac::setDuration(double duration)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    controlsManager.contentDuration = duration;

    // FIXME: We take this as an indication that playback is ready, but that is not necessarily true.
    controlsManager.hasEnabledAudio = YES;
    controlsManager.hasEnabledVideo = YES;
}

void WebPlaybackSessionInterfaceMac::setCurrentTime(double currentTime, double anchorTime)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    NSTimeInterval anchorTimeStamp = ![controlsManager rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [controlsManager setTiming:timing];
}

void WebPlaybackSessionInterfaceMac::setRate(bool isPlaying, float playbackRate)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();
    [controlsManager setRate:isPlaying ? playbackRate : 0.];

    if (m_client)
        m_client->rateChanged(isPlaying, playbackRate);
}

void WebPlaybackSessionInterfaceMac::setSeekableRanges(const TimeRanges& timeRanges)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        const PlatformTimeRanges& ranges = timeRanges.ranges();
        CMTimeRange range = CMTimeRangeMake(toCMTime(ranges.start(i)), toCMTime(ranges.end(i)));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }

    [controlsManager setSeekableTimeRanges:seekableRanges.get()];
}

void WebPlaybackSessionInterfaceMac::setAudioMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();
    [controlsManager setAudioMediaSelectionOptions:options withSelectedIndex:static_cast<NSUInteger>(selectedIndex)];
}

void WebPlaybackSessionInterfaceMac::setLegibleMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();
    [controlsManager setLegibleMediaSelectionOptions:options withSelectedIndex:static_cast<NSUInteger>(selectedIndex)];
}

void WebPlaybackSessionInterfaceMac::invalidate()
{
}

void WebPlaybackSessionInterfaceMac::ensureControlsManager()
{
    playBackControlsManager();
}

WebPlaybackControlsManager *WebPlaybackSessionInterfaceMac::playBackControlsManager()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    return m_playbackControlsManager.get();
#else
    return nil;
#endif
}

void WebPlaybackSessionInterfaceMac::setPlayBackControlsManager(WebPlaybackControlsManager *manager)
{
    m_playbackControlsManager = manager;
}

}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
