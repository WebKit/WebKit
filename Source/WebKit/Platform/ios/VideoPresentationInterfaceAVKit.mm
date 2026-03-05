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
#import "VideoPresentationInterfaceAVKit.h"

#if HAVE(AVEXPERIENCECONTROLLER)

#import "WKAVContentSource.h"
#import "WKSExperienceController.h"
#import <AVKit/AVPlayerViewControllerContentSource.h>
#import <WebCore/WebAVPlayerLayer.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>

#import "WebKitSwiftSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlayerViewControllerContentSource)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceAVKit);

Ref<VideoPresentationInterfaceAVKit> VideoPresentationInterfaceAVKit::create(WebCore::PlaybackSessionInterfaceIOS& playbackSessionInterface)
{
    return adoptRef(*new VideoPresentationInterfaceAVKit(playbackSessionInterface));
}

VideoPresentationInterfaceAVKit::VideoPresentationInterfaceAVKit(WebCore::PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : VideoPresentationInterfaceIOS { playbackSessionInterface }
{
}

VideoPresentationInterfaceAVKit::~VideoPresentationInterfaceAVKit() = default;

void VideoPresentationInterfaceAVKit::setupFullscreen(const WebCore::FloatRect& initialRect, const WebCore::FloatSize& videoDimensions, UIView *parentView, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    [playbackSessionInterface().contentSource() setVideoSize:videoDimensions];
    VideoPresentationInterfaceIOS::setupFullscreen(initialRect, videoDimensions, parentView, mode, allowsPictureInPicturePlayback, standby, blocksReturnToFullscreenFromPictureInPicture);
}

void VideoPresentationInterfaceAVKit::finalizeSetup()
{
    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }] {
        if (RefPtr model = protectedThis->videoPresentationModel())
            model->didSetupFullscreen();
    });
}

void VideoPresentationInterfaceAVKit::setupPlayerViewController()
{
    RetainPtr contentSource = playbackSessionInterface().contentSource();
    if (!m_experienceController) {
        RetainPtr platformContentSource = [allocAVPlayerViewControllerContentSourceInstance() initWithVideoPlaybackControllable:contentSource.get()];
        m_experienceController = [allocWKSExperienceControllerInstance() initWithContentSource:platformContentSource.get()];
    }

    [contentSource setVideoLayer:playerLayer()];
}

void VideoPresentationInterfaceAVKit::invalidatePlayerViewController()
{
    m_experienceController = nil;
}

void VideoPresentationInterfaceAVKit::presentFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().startObservingNowPlayingMetadata();
    [m_experienceController enterFullscreenWithCompletionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)] (BOOL success) {
        completionHandler(success, nil);
    }).get()];
}

void VideoPresentationInterfaceAVKit::dismissFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().stopObservingNowPlayingMetadata();
    [m_experienceController exitFullscreenWithCompletionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)] (BOOL success) {
        completionHandler(success, nil);
    }).get()];
}

void VideoPresentationInterfaceAVKit::setContentDimensions(const WebCore::FloatSize& contentDimensions)
{
    [playbackSessionInterface().contentSource() setVideoSize:contentDimensions];
}

} // namespace WebKit

#endif // HAVE(AVEXPERIENCECONTROLLER)
