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

#ifndef WebPlaybackControlsManager_h
#define WebPlaybackControlsManager_h

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import <WebCore/AVKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace WebCore {
class WebPlaybackSessionInterfaceMac;
}

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

WEBCORE_EXPORT
@interface WebPlaybackControlsManager : NSObject <AVFunctionBarPlaybackControlsControlling> {
    NSTimeInterval _contentDuration;
    RetainPtr<AVValueTiming> _timing;
    NSTimeInterval _seekToTime;
    RetainPtr<NSArray> _seekableTimeRanges;
    BOOL _hasEnabledAudio;
    BOOL _hasEnabledVideo;
    RetainPtr<NSArray<AVFunctionBarMediaSelectionOption *>> _audioFunctionBarMediaSelectionOptions;
    RetainPtr<AVFunctionBarMediaSelectionOption> _currentAudioFunctionBarMediaSelectionOption;
    RetainPtr<NSArray<AVFunctionBarMediaSelectionOption *>> _legibleFunctionBarMediaSelectionOptions;
    RetainPtr<AVFunctionBarMediaSelectionOption> _currentLegibleFunctionBarMediaSelectionOption;
    float _rate;
    BOOL _canTogglePlayback;

@private
    RefPtr<WebCore::WebPlaybackSessionInterfaceMac> _webPlaybackSessionInterfaceMac;
}

@property (assign) WebCore::WebPlaybackSessionInterfaceMac* webPlaybackSessionInterfaceMac;
@property (readwrite) NSTimeInterval contentDuration;
@property (nonatomic, retain, readwrite) AVValueTiming *timing;
@property (nonatomic) NSTimeInterval seekToTime;
@property (nonatomic, retain, readwrite) NSArray *seekableTimeRanges;
@property (nonatomic) BOOL hasEnabledAudio;
@property (nonatomic) BOOL hasEnabledVideo;
@property (getter=isPlaying) BOOL playing;
@property BOOL canTogglePlayback;

@property (nonatomic) float rate;

- (AVFunctionBarMediaSelectionOption *)currentAudioFunctionBarMediaSelectionOption;
- (void)setCurrentAudioFunctionBarMediaSelectionOption:(AVFunctionBarMediaSelectionOption *)option;
- (AVFunctionBarMediaSelectionOption *)currentLegibleFunctionBarMediaSelectionOption;
- (void)setCurrentLegibleFunctionBarMediaSelectionOption:(AVFunctionBarMediaSelectionOption *)option;
- (void)setAudioMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex;
- (void)setLegibleMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex;
@end

#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#endif // WebPlaybackControlsManager_h
