/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "PlaybackSessionInterfaceLMK.h"

#if PLATFORM(VISION)

#import <WebCore/MediaSelectionOption.h>
#import <WebCore/TimeRanges.h>
#import <WebKitSwift/WebKitSwift.h>
#import <wtf/WeakPtr.h>

#import "WebKitSwiftSoftLink.h"

@interface WKLinearMediaPlayerDelegate : NSObject <WKSLinearMediaPlayerDelegate>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithInterface:(WebKit::PlaybackSessionInterfaceLMK&)interface;
@end

@implementation WKLinearMediaPlayerDelegate {
    WeakPtr<WebKit::PlaybackSessionInterfaceLMK> _interface;
}

- (instancetype)initWithInterface:(WebKit::PlaybackSessionInterfaceLMK&)interface
{
    self = [super init];
    if (!self)
        return nil;

    _interface = interface;
    return self;
}

@end

namespace WebKit {

Ref<PlaybackSessionInterfaceLMK> PlaybackSessionInterfaceLMK::create(PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceLMK(model));
    interface->initialize();
    return interface;
}

PlaybackSessionInterfaceLMK::PlaybackSessionInterfaceLMK(PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS { model }
    , m_player { adoptNS([allocWKSLinearMediaPlayerInstance() init]) }
    , m_playerDelegate { adoptNS([[WKLinearMediaPlayerDelegate alloc] initWithInterface:*this]) }
{
    ASSERT(isUIThread());
    [m_player setDelegate:m_playerDelegate.get()];
}

PlaybackSessionInterfaceLMK::~PlaybackSessionInterfaceLMK()
{
    ASSERT(isUIThread());
    invalidate();
}

void PlaybackSessionInterfaceLMK::invalidate()
{
    [m_player setDelegate:nullptr];
    PlaybackSessionInterfaceIOS::invalidate();
}

WebAVPlayerController *PlaybackSessionInterfaceLMK::playerController() const
{
    return nullptr;
}

void PlaybackSessionInterfaceLMK::durationChanged(double duration)
{
    [m_player setDuration:duration];
    [m_player setCanTogglePlayback:YES];
    [m_player setHasAudioContent:YES];
}

void PlaybackSessionInterfaceLMK::currentTimeChanged(double currentTime, double)
{
    [m_player setCurrentTime:currentTime];
}

void PlaybackSessionInterfaceLMK::bufferedTimeChanged(double bufferedTime)
{

}

void PlaybackSessionInterfaceLMK::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState>, double, double)
{

}

void PlaybackSessionInterfaceLMK::seekableRangesChanged(const TimeRanges&, double, double)
{

}

void PlaybackSessionInterfaceLMK::canPlayFastReverseChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>&, uint64_t)
{

}

void PlaybackSessionInterfaceLMK::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>&, uint64_t)
{

}

void PlaybackSessionInterfaceLMK::externalPlaybackChanged(bool, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{

}

void PlaybackSessionInterfaceLMK::wirelessVideoPlaybackDisabledChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::mutedChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::volumeChanged(double)
{

}

#if !RELEASE_LOG_DISABLED
const char* PlaybackSessionInterfaceLMK::logClassName() const
{
    return "PlaybackSessionInterfaceLMK";
}
#endif

} // namespace WebKit

#endif // PLATFORM(VISION)
