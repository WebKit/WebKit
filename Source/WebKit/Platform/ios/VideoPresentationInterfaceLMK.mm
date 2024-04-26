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

#if ENABLE(LINEAR_MEDIA_PLAYER)

#import "LinearMediaKitSPI.h"
#import "PlaybackSessionInterfaceLMK.h"
#import "WKSLinearMediaPlayer.h"
#import "WKSLinearMediaTypes.h"
#import <UIKit/UIKit.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <wtf/BlockPtr.h>

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
    return playbackSessionInterface().linearMediaPlayer();
}

void VideoPresentationInterfaceLMK::setupFullscreen(UIView& videoView, const FloatRect& initialRect, const FloatSize& videoDimensions, UIView* parentView, HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    linearMediaPlayer().contentDimensions = videoDimensions;
    VideoPresentationInterfaceIOS::setupFullscreen(videoView, initialRect, videoDimensions, parentView, mode, allowsPictureInPicturePlayback, standby, blocksReturnToFullscreenFromPictureInPicture);
}

void VideoPresentationInterfaceLMK::finalizeSetup()
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (RefPtr model = videoPresentationModel())
            model->didSetupFullscreen();
    });
}

void VideoPresentationInterfaceLMK::setupPlayerViewController()
{
    linearMediaPlayer().captionLayer = captionsLayer();
    linearMediaPlayer().contentType = WKSLinearMediaContentTypePlanar;

    ensurePlayableViewController();
}

void VideoPresentationInterfaceLMK::invalidatePlayerViewController()
{
    m_playerViewController = nil;
}

void VideoPresentationInterfaceLMK::presentFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().startObservingNowPlayingMetadata();
    [linearMediaPlayer() enterFullscreenWithCompletionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}

void VideoPresentationInterfaceLMK::dismissFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().stopObservingNowPlayingMetadata();
    [linearMediaPlayer() exitFullscreenWithCompletionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}

UIViewController *VideoPresentationInterfaceLMK::playerViewController() const
{
    return m_playerViewController.get();
}

void VideoPresentationInterfaceLMK::setContentDimensions(const FloatSize& contentDimensions)
{
    linearMediaPlayer().contentDimensions = contentDimensions;
}

void VideoPresentationInterfaceLMK::setShowsPlaybackControls(bool showsPlaybackControls)
{
    linearMediaPlayer().showsPlaybackControls = showsPlaybackControls;
}

void VideoPresentationInterfaceLMK::setupCaptionsLayer(CALayer *, const FloatSize& initialSize)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [captionsLayer() removeFromSuperlayer];
    [captionsLayer() setAnchorPoint:CGPointZero];
    [captionsLayer() setBounds:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
    [CATransaction commit];
}

LMPlayableViewController *VideoPresentationInterfaceLMK::playableViewController()
{
    ensurePlayableViewController();
    return m_playerViewController.get();
}

void VideoPresentationInterfaceLMK::ensurePlayableViewController()
{
    if (!m_playerViewController)
        m_playerViewController = [linearMediaPlayer() makeViewController];
}

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
