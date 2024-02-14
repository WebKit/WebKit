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

#pragma once

#if PLATFORM(VISION)

#include <WebCore/VideoPresentationInterfaceIOS.h>

OBJC_CLASS LMPlayableViewController;
OBJC_CLASS WKPlayableViewControllerDelegate;
OBJC_CLASS WKSLinearMediaPlayer;

namespace WebCore {
class PlaybackSessionInterfaceIOS;
}

namespace WebKit {

using namespace WebCore;

class VideoPresentationInterfaceLMK : public VideoPresentationInterfaceIOS {
public:
    static Ref<VideoPresentationInterfaceLMK> create(PlaybackSessionInterfaceIOS&);
#if !RELEASE_LOG_DISABLED
    const char* logClassName() const { return "VideoPresentationInterfaceLMK"; };
#endif
    ~VideoPresentationInterfaceLMK();
    void setupFullscreen(UIView&, const FloatRect&, const FloatSize&, UIView*, HTMLMediaElementEnums::VideoFullscreenMode, bool, bool, bool);
    bool pictureInPictureWasStartedWhenEnteringBackground() const final;
    AVPlayerViewController *avPlayerViewController() const;

    void hasVideoChanged(bool);
    void setPlayerIdentifier(std::optional<MediaPlayerIdentifier>) final;
    bool mayAutomaticallyShowVideoPictureInPicture() const;
    bool isPlayingVideoInEnhancedFullscreen() const;
private:
    VideoPresentationInterfaceLMK(PlaybackSessionInterfaceIOS&);

    WKSLinearMediaPlayer *linearMediaPlayer() const;

    void updateRouteSharingPolicy() final { }
    void setupPlayerViewController() final;
    void invalidatePlayerViewController() final;
    UIViewController *playerViewController() const final;
    void tryToStartPictureInPicture() final { }
    void stopPictureInPicture() final { }
    void presentFullscreen(bool animated, CompletionHandler<void(BOOL, NSError *)>&&) final;
    void dismissFullscreen(bool animated, CompletionHandler<void(BOOL, NSError *)>&&) final;
    void setShowsPlaybackControls(bool) final;
    void setContentDimensions(const FloatSize&) final;
    void setAllowsPictureInPicturePlayback(bool) final { }

    RetainPtr<LMPlayableViewController> m_playerViewController;
    RetainPtr<WKPlayableViewControllerDelegate> m_playerViewControllerDelegate;
    RetainPtr<UIViewController> m_presentingViewController;
};

} // namespace WebKit

#endif // PLATFORM(VISION)
