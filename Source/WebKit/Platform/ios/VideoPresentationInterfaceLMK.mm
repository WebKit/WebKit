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
#import "VideoPresentationInterfaceLMK.h"

#if PLATFORM(VISION)

#import "PlaybackSessionInterfaceLMK.h"
#import <UIKit/UIKit.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <WebKitSwift/WebKitSwift.h>

namespace WebKit {

VideoPresentationInterfaceLMK::~VideoPresentationInterfaceLMK()
{

}

Ref<VideoPresentationInterfaceLMK> VideoPresentationInterfaceLMK::create(PlaybackSessionInterfaceIOS& playbackSessionInterface)
{
    return adoptRef(*new VideoPresentationInterfaceLMK(playbackSessionInterface));
}

VideoPresentationInterfaceLMK::VideoPresentationInterfaceLMK(PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : VideoPresentationInterfaceIOS { playbackSessionInterface }
{
}

WKSLinearMediaPlayer *VideoPresentationInterfaceLMK::linearMediaPlayer() const
{
    return nullptr;
}

void VideoPresentationInterfaceLMK::setupFullscreen(UIView&, const FloatRect&, const FloatSize&, UIView*, HTMLMediaElementEnums::VideoFullscreenMode, bool, bool, bool)
{

}

bool VideoPresentationInterfaceLMK::pictureInPictureWasStartedWhenEnteringBackground() const
{
    return false;
}

void VideoPresentationInterfaceLMK::hasVideoChanged(bool)
{

}

void VideoPresentationInterfaceLMK::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{

}

bool VideoPresentationInterfaceLMK::mayAutomaticallyShowVideoPictureInPicture() const
{
    return false;
}

void VideoPresentationInterfaceLMK::setupPlayerViewController()
{

}

void VideoPresentationInterfaceLMK::invalidatePlayerViewController()
{

}

void VideoPresentationInterfaceLMK::presentFullscreen(bool, CompletionHandler<void(BOOL, NSError *)>&&)
{

}

void VideoPresentationInterfaceLMK::dismissFullscreen(bool, CompletionHandler<void(BOOL, NSError *)>&&)
{

}

bool VideoPresentationInterfaceLMK::isPlayingVideoInEnhancedFullscreen() const
{
    return false;
}

AVPlayerViewController *VideoPresentationInterfaceLMK::avPlayerViewController() const
{
    return nullptr;
}

UIViewController *VideoPresentationInterfaceLMK::playerViewController() const
{
    return nullptr;
}

void VideoPresentationInterfaceLMK::setContentDimensions(const FloatSize&)
{

}

void VideoPresentationInterfaceLMK::setShowsPlaybackControls(bool)
{

}

} // namespace WebKit

#endif // PLATFORM(VISION)
