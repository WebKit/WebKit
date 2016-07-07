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
#include "WebPlaybackControlsManager.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#if USE(APPLE_INTERNAL_SDK) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
#import <WebKitAdditions/WebPlaybackControlsControllerAdditions.mm>
#else

#import "WebPlaybackSessionInterfaceMac.h"

@implementation WebPlaybackControlsManager

using namespace WebCore;

@synthesize contentDuration=_contentDuration;
@synthesize hasEnabledAudio=_hasEnabledAudio;
@synthesize hasEnabledVideo=_hasEnabledVideo;
@synthesize rate=_rate;
@synthesize playing=_playing;
@synthesize canTogglePlayback=_canTogglePlayback;

- (WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    return _webPlaybackSessionInterfaceMac.get();
}

- (void)setWebPlaybackSessionInterfaceMac:(WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    _webPlaybackSessionInterfaceMac = webPlaybackSessionInterfaceMac;
}

- (AVValueTiming *)timing
{
    return _timing.get();
}

- (void)setTiming:(AVValueTiming *)timing
{
    _timing = timing;
}

- (NSArray *)seekableTimeRanges
{
    return _seekableTimeRanges.get();
}

- (void)setSeekableTimeRanges:(NSArray *)timeRanges
{
    _seekableTimeRanges = timeRanges;
}

- (void)setAudioMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    UNUSED_PARAM(options);
    UNUSED_PARAM(selectedIndex);
}

- (void)setLegibleMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    UNUSED_PARAM(options);
    UNUSED_PARAM(selectedIndex);
}

@end
#endif // USE(APPLE_INTERNAL_SDK) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

