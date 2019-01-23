/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebVideoFullscreenControllerAVKit.h"

#import "Logging.h"
#import "MediaSelectionOption.h"
#import "PlaybackSessionInterfaceAVKit.h"
#import "PlaybackSessionModelMediaElement.h"
#import "TimeRanges.h"
#import "VideoFullscreenChangeObserver.h"
#import "VideoFullscreenInterfaceAVKit.h"
#import "VideoFullscreenModelVideoElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <UIKit/UIView.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/RenderVideo.h>
#import <WebCore/WebCoreThreadRun.h>
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

using namespace WebCore;

#if !HAVE(AVKIT)

@implementation WebVideoFullscreenController
- (void)setVideoElement:(WebCore::HTMLVideoElement*)videoElement
{
    UNUSED_PARAM(videoElement);
}

- (WebCore::HTMLVideoElement*)videoElement
{
    return nullptr;
}

- (void)enterFullscreen:(UIView *)view mode:(WebCore::HTMLMediaElementEnums::VideoFullscreenMode)mode
{
    UNUSED_PARAM(view);
    UNUSED_PARAM(mode);
}

- (void)requestHideAndExitFullscreen
{
}

- (void)exitFullscreen
{
}
@end

#else

static IntRect elementRectInWindow(HTMLVideoElement* videoElement)
{
    if (!videoElement)
        return { };
    auto* renderer = videoElement->renderer();
    auto* view = videoElement->document().view();
    if (!renderer || !view)
        return { };
    return view->convertToContainingWindow(renderer->absoluteBoundingBoxRect());
}

class VideoFullscreenControllerContext;

@interface WebVideoFullscreenController (delegate)
-(void)didFinishFullscreen:(VideoFullscreenControllerContext*)context;
@end

class VideoFullscreenControllerContext final
    : private VideoFullscreenModel
    , private VideoFullscreenModelClient
    , private VideoFullscreenChangeObserver
    , private PlaybackSessionModel
    , private PlaybackSessionModelClient
    , public ThreadSafeRefCounted<VideoFullscreenControllerContext> {

public:
    static Ref<VideoFullscreenControllerContext> create()
    {
        return adoptRef(*new VideoFullscreenControllerContext);
    }

    void setController(WebVideoFullscreenController* controller) { m_controller = controller; }
    void setUpFullscreen(HTMLVideoElement&, UIView *, HTMLMediaElementEnums::VideoFullscreenMode);
    void exitFullscreen();
    void requestHideAndExitFullscreen();
    void invalidate();

private:
    VideoFullscreenControllerContext() { }

    // VideoFullscreenChangeObserver
    void requestUpdateInlineRect() final;
    void requestVideoContentLayer() final;
    void returnVideoContentLayer() final;
    void didSetupFullscreen() final;
    void didEnterFullscreen() final { }
    void willExitFullscreen() final;
    void didExitFullscreen() final;
    void didCleanupFullscreen() final;
    void fullscreenMayReturnToInline() final;

    // VideoFullscreenModelClient
    void hasVideoChanged(bool) override;
    void videoDimensionsChanged(const FloatSize&) override;

    // PlaybackSessionModel
    void addClient(PlaybackSessionModelClient&) override;
    void removeClient(PlaybackSessionModelClient&) override;
    void play() override;
    void pause() override;
    void togglePlayState() override;
    void beginScrubbing() override;
    void endScrubbing() override;
    void seekToTime(double, double, double) override;
    void fastSeek(double time) override;
    void beginScanningForward() override;
    void beginScanningBackward() override;
    void endScanning() override;
    void selectAudioMediaOption(uint64_t) override;
    void selectLegibleMediaOption(uint64_t) override;
    double duration() const override;
    double playbackStartedTime() const override { return 0; }
    double currentTime() const override;
    double bufferedTime() const override;
    bool isPlaying() const override;
    bool isScrubbing() const override { return false; }
    float playbackRate() const override;
    Ref<TimeRanges> seekableRanges() const override;
    double seekableTimeRangesLastModifiedTime() const override;
    double liveUpdateInterval() const override;
    bool canPlayFastReverse() const override;
    Vector<MediaSelectionOption> audioMediaSelectionOptions() const override;
    uint64_t audioMediaSelectedIndex() const override;
    Vector<MediaSelectionOption> legibleMediaSelectionOptions() const override;
    uint64_t legibleMediaSelectedIndex() const override;
    bool externalPlaybackEnabled() const override;
    ExternalPlaybackTargetType externalPlaybackTargetType() const override;
    String externalPlaybackLocalizedDeviceName() const override;
    bool wirelessVideoPlaybackDisabled() const override;
    void togglePictureInPicture() override { }
    void toggleMuted() override;
    void setMuted(bool) final;
    void setVolume(double) final;
    void setPlayingOnSecondScreen(bool) final;

    // PlaybackSessionModelClient
    void durationChanged(double) override;
    void currentTimeChanged(double currentTime, double anchorTime) override;
    void bufferedTimeChanged(double) override;
    void rateChanged(bool isPlaying, float playbackRate) override;
    void seekableRangesChanged(const TimeRanges&, double lastModifiedTime, double liveUpdateInterval) override;
    void canPlayFastReverseChanged(bool) override;
    void audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    void legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    void externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) override;
    void wirelessVideoPlaybackDisabledChanged(bool) override;
    void mutedChanged(bool) override;
    void volumeChanged(double) override;

    // VideoFullscreenModel
    void addClient(VideoFullscreenModelClient&) override;
    void removeClient(VideoFullscreenModelClient&) override;
    void requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia = false) override;
    void setVideoLayerFrame(FloatRect) override;
    void setVideoLayerGravity(MediaPlayerEnums::VideoGravity) override;
    void fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode) override;
    bool hasVideo() const override;
    FloatSize videoDimensions() const override;
    bool isMuted() const override;
    double volume() const override;
    bool isPictureInPictureSupported() const override;
    bool isPictureInPictureActive() const override;
    void willEnterPictureInPicture() final;
    void didEnterPictureInPicture() final;
    void failedToEnterPictureInPicture() final;
    void willExitPictureInPicture() final;
    void didExitPictureInPicture() final;

    HashSet<PlaybackSessionModelClient*> m_playbackClients;
    HashSet<VideoFullscreenModelClient*> m_fullscreenClients;
    RefPtr<VideoFullscreenInterfaceAVKit> m_interface;
    RefPtr<VideoFullscreenModelVideoElement> m_fullscreenModel;
    RefPtr<PlaybackSessionModelMediaElement> m_playbackModel;
    RefPtr<HTMLVideoElement> m_videoElement;
    RetainPtr<UIView> m_videoFullscreenView;
    RetainPtr<WebVideoFullscreenController> m_controller;
};

#pragma mark VideoFullscreenChangeObserver

void VideoFullscreenControllerContext::requestUpdateInlineRect()
{
#if PLATFORM(IOS_FAMILY)
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] () mutable {
#endif
        IntRect clientRect = elementRectInWindow(m_videoElement.get());
        dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this, clientRect] {
            m_interface->setInlineRect(clientRect, clientRect != IntRect(0, 0, 0, 0));
        });
#if USE(WEB_THREAD)
    });
#endif
#else
    ASSERT_NOT_REACHED();
#endif
}

void VideoFullscreenControllerContext::requestVideoContentLayer()
{
#if PLATFORM(IOS_FAMILY)
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, videoFullscreenLayer = retainPtr([m_videoFullscreenView layer])] () mutable {
#endif
        [videoFullscreenLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];
        m_fullscreenModel->setVideoFullscreenLayer(videoFullscreenLayer.get(), [protectedThis = WTFMove(protectedThis), this] () mutable {
            dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this] {
                m_interface->setHasVideoContentLayer(true);
            });
        });
#if USE(WEB_THREAD)
    });
#endif
#else
    ASSERT_NOT_REACHED();
#endif
}

void VideoFullscreenControllerContext::returnVideoContentLayer()
{
#if PLATFORM(IOS_FAMILY)
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, videoFullscreenLayer = retainPtr([m_videoFullscreenView layer])] () mutable {
#endif
        [videoFullscreenLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];
        m_fullscreenModel->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), this] () mutable {
            dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this] {
                m_interface->setHasVideoContentLayer(false);
            });
        });
#if USE(WEB_THREAD)
    });
#endif
#else
    ASSERT_NOT_REACHED();
#endif
}

void VideoFullscreenControllerContext::didSetupFullscreen()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
#if PLATFORM(IOS_FAMILY)
    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), this] {
        m_interface->enterFullscreen();
    });
#else
#if USE(WEB_THREAD)
    WebThreadRun([protectedThis = makeRefPtr(this), this, videoFullscreenLayer = retainPtr([m_videoFullscreenView layer])] () mutable {
#endif
        [videoFullscreenLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];
        m_fullscreenModel->setVideoFullscreenLayer(videoFullscreenLayer.get(), [protectedThis = WTFMove(protectedThis), this] () mutable {
            dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this] {
                m_interface->enterFullscreen();
            });
        });
#if USE(WEB_THREAD)
    });
#endif
#endif
}

void VideoFullscreenControllerContext::willExitFullscreen()
{
#if PLATFORM(WATCHOS)
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] () mutable {
#endif
        m_fullscreenModel->willExitFullscreen();
#if USE(WEB_THREAD)
    });
#endif
#endif
}

void VideoFullscreenControllerContext::didExitFullscreen()
{
    ASSERT(isUIThread());
#if PLATFORM(IOS_FAMILY)
    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), this] {
        m_interface->cleanupFullscreen();
    });
#else
#if USE(WEB_THREAD)
    WebThreadRun([protectedThis = makeRefPtr(this), this] () mutable {
#endif
        m_fullscreenModel->setVideoFullscreenLayer(nil, [protectedThis = WTFMove(protectedThis), this] () mutable {
            dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this] {
                m_interface->cleanupFullscreen();
            });
        });
#if USE(WEB_THREAD)
    });
#endif
#endif
}

void VideoFullscreenControllerContext::didCleanupFullscreen()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    m_interface->setVideoFullscreenModel(nullptr);
    m_interface->setVideoFullscreenChangeObserver(nullptr);
    m_interface = nullptr;
    m_videoFullscreenView = nil;

#if USE(WEB_THREAD)
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        m_fullscreenModel->setVideoFullscreenLayer(nil);
        m_fullscreenModel->setVideoElement(nullptr);
        m_playbackModel->setMediaElement(nullptr);
        m_playbackModel->removeClient(*this);
        m_fullscreenModel->removeClient(*this);
        m_fullscreenModel = nullptr;
        m_videoElement = nullptr;

        [m_controller didFinishFullscreen:this];
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::fullscreenMayReturnToInline()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] () mutable {
#endif
        IntRect clientRect = elementRectInWindow(m_videoElement.get());
        dispatch_async(dispatch_get_main_queue(), [protectedThis = WTFMove(protectedThis), this, clientRect] {
            m_interface->preparedToReturnToInline(true, clientRect);
        });
#if USE(WEB_THREAD)
    });
#endif
}

#pragma mark PlaybackSessionModelClient

void VideoFullscreenControllerContext::durationChanged(double duration)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), duration] {
            protectedThis->durationChanged(duration);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->durationChanged(duration);
}

void VideoFullscreenControllerContext::currentTimeChanged(double currentTime, double anchorTime)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), currentTime, anchorTime] {
            protectedThis->currentTimeChanged(currentTime, anchorTime);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->currentTimeChanged(currentTime, anchorTime);
}

void VideoFullscreenControllerContext::bufferedTimeChanged(double bufferedTime)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), bufferedTime] {
            protectedThis->bufferedTimeChanged(bufferedTime);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->bufferedTimeChanged(bufferedTime);
}

void VideoFullscreenControllerContext::rateChanged(bool isPlaying, float playbackRate)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), isPlaying, playbackRate] {
            protectedThis->rateChanged(isPlaying, playbackRate);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->rateChanged(isPlaying, playbackRate);
}

void VideoFullscreenControllerContext::hasVideoChanged(bool hasVideo)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), hasVideo] {
            protectedThis->hasVideoChanged(hasVideo);
        });
        return;
    }

    for (auto& client : m_fullscreenClients)
        client->hasVideoChanged(hasVideo);
}

void VideoFullscreenControllerContext::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), videoDimensions = videoDimensions] {
            protectedThis->videoDimensionsChanged(videoDimensions);
        });
        return;
    }

    for (auto& client : m_fullscreenClients)
        client->videoDimensionsChanged(videoDimensions);
}

void VideoFullscreenControllerContext::seekableRangesChanged(const TimeRanges& timeRanges, double lastModifiedTime, double liveUpdateInterval)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), platformTimeRanges = timeRanges.ranges(), lastModifiedTime, liveUpdateInterval] {
            protectedThis->seekableRangesChanged(TimeRanges::create(platformTimeRanges), lastModifiedTime, liveUpdateInterval);
        });
        return;
    }

    for (auto &client : m_playbackClients)
        client->seekableRangesChanged(timeRanges, lastModifiedTime, liveUpdateInterval);
}

void VideoFullscreenControllerContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), canPlayFastReverse] {
            protectedThis->canPlayFastReverseChanged(canPlayFastReverse);
        });
        return;
    }

    for (auto &client : m_playbackClients)
        client->canPlayFastReverseChanged(canPlayFastReverse);
}

static Vector<MediaSelectionOption> isolatedCopy(const Vector<MediaSelectionOption>& options)
{
    Vector<MediaSelectionOption> optionsCopy;
    optionsCopy.reserveInitialCapacity(options.size());
    for (auto& option : options)
        optionsCopy.uncheckedAppend({ option.displayName.isolatedCopy(), option.type });
    return optionsCopy;
}

void VideoFullscreenControllerContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), options = isolatedCopy(options), selectedIndex] {
            protectedThis->audioMediaSelectionOptionsChanged(options, selectedIndex);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->audioMediaSelectionOptionsChanged(options, selectedIndex);
}

void VideoFullscreenControllerContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), options = isolatedCopy(options), selectedIndex] {
            protectedThis->legibleMediaSelectionOptionsChanged(options, selectedIndex);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->legibleMediaSelectionOptionsChanged(options, selectedIndex);
}

void VideoFullscreenControllerContext::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedDeviceName)
{
    if (WebThreadIsCurrent()) {
        callOnMainThread([protectedThis = makeRef(*this), this, enabled, type, localizedDeviceName = localizedDeviceName.isolatedCopy()] {
            for (auto& client : m_playbackClients)
                client->externalPlaybackChanged(enabled, type, localizedDeviceName);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->externalPlaybackChanged(enabled, type, localizedDeviceName);
}

void VideoFullscreenControllerContext::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), disabled] {
            protectedThis->wirelessVideoPlaybackDisabledChanged(disabled);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->wirelessVideoPlaybackDisabledChanged(disabled);
}

void VideoFullscreenControllerContext::mutedChanged(bool muted)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), muted] {
            protectedThis->mutedChanged(muted);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->mutedChanged(muted);
}

void VideoFullscreenControllerContext::volumeChanged(double volume)
{
    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), volume] {
            protectedThis->volumeChanged(volume);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->volumeChanged(volume);
}
#pragma mark VideoFullscreenModel

void VideoFullscreenControllerContext::addClient(VideoFullscreenModelClient& client)
{
    ASSERT(!m_fullscreenClients.contains(&client));
    m_fullscreenClients.add(&client);
}

void VideoFullscreenControllerContext::removeClient(VideoFullscreenModelClient& client)
{
    ASSERT(m_fullscreenClients.contains(&client));
    m_fullscreenClients.remove(&client);
}

void VideoFullscreenControllerContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, mode, finishedWithMedia] {
#endif
        if (m_fullscreenModel)
            m_fullscreenModel->requestFullscreenMode(mode, finishedWithMedia);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::setVideoLayerFrame(FloatRect frame)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    RetainPtr<CALayer> videoFullscreenLayer = [m_videoFullscreenView layer];
    [videoFullscreenLayer setSublayerTransform:[videoFullscreenLayer transform]];

    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), this, frame, videoFullscreenLayer = WTFMove(videoFullscreenLayer)] () mutable {
#if USE(WEB_THREAD)
        WebThreadRun([protectedThis = WTFMove(protectedThis), this, frame, videoFullscreenLayer = WTFMove(videoFullscreenLayer)] {
#endif
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [CATransaction setAnimationDuration:0];
            
            [videoFullscreenLayer setSublayerTransform:CATransform3DIdentity];
            
            if (m_fullscreenModel)
                m_fullscreenModel->setVideoLayerFrame(frame);
            [CATransaction commit];
#if USE(WEB_THREAD)
        });
#endif
    });
}

void VideoFullscreenControllerContext::setVideoLayerGravity(MediaPlayerEnums::VideoGravity videoGravity)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, videoGravity] {
#endif
        if (m_fullscreenModel)
            m_fullscreenModel->setVideoLayerGravity(videoGravity);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, mode] {
#endif
        if (m_fullscreenModel)
            m_fullscreenModel->fullscreenModeChanged(mode);
#if USE(WEB_THREAD)
    });
#endif
}

bool VideoFullscreenControllerContext::hasVideo() const
{
    ASSERT(isUIThread());
    return m_fullscreenModel ? m_fullscreenModel->hasVideo() : false;
}

bool VideoFullscreenControllerContext::isMuted() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->isMuted() : false;
}

double VideoFullscreenControllerContext::volume() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->volume() : 0;
}

bool VideoFullscreenControllerContext::isPictureInPictureActive() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->isPictureInPictureActive() : false;
}

bool VideoFullscreenControllerContext::isPictureInPictureSupported() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->isPictureInPictureSupported() : false;
}

void VideoFullscreenControllerContext::willEnterPictureInPicture()
{
    ASSERT(isUIThread());
    for (auto* client : m_fullscreenClients)
        client->willEnterPictureInPicture();
}

void VideoFullscreenControllerContext::didEnterPictureInPicture()
{
    ASSERT(isUIThread());
    for (auto* client : m_fullscreenClients)
        client->didEnterPictureInPicture();
}

void VideoFullscreenControllerContext::failedToEnterPictureInPicture()
{
    ASSERT(isUIThread());
    for (auto* client : m_fullscreenClients)
        client->failedToEnterPictureInPicture();
}

void VideoFullscreenControllerContext::willExitPictureInPicture()
{
    ASSERT(isUIThread());
    for (auto* client : m_fullscreenClients)
        client->willExitPictureInPicture();
}

void VideoFullscreenControllerContext::didExitPictureInPicture()
{
    ASSERT(isUIThread());
    for (auto* client : m_fullscreenClients)
        client->didExitPictureInPicture();
}

FloatSize VideoFullscreenControllerContext::videoDimensions() const
{
    ASSERT(isUIThread());
    return m_fullscreenModel ? m_fullscreenModel->videoDimensions() : FloatSize();
}

#pragma mark - PlaybackSessionModel

void VideoFullscreenControllerContext::addClient(PlaybackSessionModelClient& client)
{
    ASSERT(!m_playbackClients.contains(&client));
    m_playbackClients.add(&client);
}

void VideoFullscreenControllerContext::removeClient(PlaybackSessionModelClient& client)
{
    ASSERT(m_playbackClients.contains(&client));
    m_playbackClients.remove(&client);
}

void VideoFullscreenControllerContext::play()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->play();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::pause()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->pause();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::togglePlayState()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->togglePlayState();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::toggleMuted()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->toggleMuted();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::setMuted(bool muted)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, muted] {
#endif
        if (m_playbackModel)
            m_playbackModel->setMuted(muted);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::setVolume(double volume)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, volume] {
#endif
        if (m_playbackModel)
            m_playbackModel->setVolume(volume);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::setPlayingOnSecondScreen(bool value)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, value] {
#endif
        if (m_playbackModel)
            m_playbackModel->setPlayingOnSecondScreen(value);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::beginScrubbing()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->beginScrubbing();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::endScrubbing()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->endScrubbing();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::seekToTime(double time, double toleranceBefore, double toleranceAfter)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, time, toleranceBefore, toleranceAfter] {
#endif
        if (m_playbackModel)
            m_playbackModel->seekToTime(time, toleranceBefore, toleranceAfter);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::fastSeek(double time)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, time] {
#endif
        if (m_playbackModel)
            m_playbackModel->fastSeek(time);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::beginScanningForward()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->beginScanningForward();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::beginScanningBackward()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->beginScanningBackward();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::endScanning()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this] {
#endif
        if (m_playbackModel)
            m_playbackModel->endScanning();
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::selectAudioMediaOption(uint64_t index)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, index] {
#endif
        if (m_playbackModel)
            m_playbackModel->selectAudioMediaOption(index);
#if USE(WEB_THREAD)
    });
#endif
}

void VideoFullscreenControllerContext::selectLegibleMediaOption(uint64_t index)
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
    WebThreadRun([protectedThis = makeRefPtr(this), this, index] {
#endif
        if (m_playbackModel)
            m_playbackModel->selectLegibleMediaOption(index);
#if USE(WEB_THREAD)
    });
#endif
}

double VideoFullscreenControllerContext::duration() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->duration() : 0;
}

double VideoFullscreenControllerContext::currentTime() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->currentTime() : 0;
}

double VideoFullscreenControllerContext::bufferedTime() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->bufferedTime() : 0;
}

bool VideoFullscreenControllerContext::isPlaying() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->isPlaying() : false;
}

float VideoFullscreenControllerContext::playbackRate() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->playbackRate() : 0;
}

Ref<TimeRanges> VideoFullscreenControllerContext::seekableRanges() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->seekableRanges() : TimeRanges::create();
}

double VideoFullscreenControllerContext::seekableTimeRangesLastModifiedTime() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->seekableTimeRangesLastModifiedTime() : 0;
}

double VideoFullscreenControllerContext::liveUpdateInterval() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->liveUpdateInterval() : 0;
}

bool VideoFullscreenControllerContext::canPlayFastReverse() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->canPlayFastReverse() : false;
}

Vector<MediaSelectionOption> VideoFullscreenControllerContext::audioMediaSelectionOptions() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    if (m_playbackModel)
        return m_playbackModel->audioMediaSelectionOptions();
    return { };
}

uint64_t VideoFullscreenControllerContext::audioMediaSelectedIndex() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->audioMediaSelectedIndex() : -1;
}

Vector<MediaSelectionOption> VideoFullscreenControllerContext::legibleMediaSelectionOptions() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    if (m_playbackModel)
        return m_playbackModel->legibleMediaSelectionOptions();
    return { };
}

uint64_t VideoFullscreenControllerContext::legibleMediaSelectedIndex() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->legibleMediaSelectedIndex() : -1;
}

bool VideoFullscreenControllerContext::externalPlaybackEnabled() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->externalPlaybackEnabled() : false;
}

PlaybackSessionModel::ExternalPlaybackTargetType VideoFullscreenControllerContext::externalPlaybackTargetType() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->externalPlaybackTargetType() : TargetTypeNone;
}

String VideoFullscreenControllerContext::externalPlaybackLocalizedDeviceName() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->externalPlaybackLocalizedDeviceName() : String();
}

bool VideoFullscreenControllerContext::wirelessVideoPlaybackDisabled() const
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    return m_playbackModel ? m_playbackModel->wirelessVideoPlaybackDisabled() : true;
}

#pragma mark Other

void VideoFullscreenControllerContext::setUpFullscreen(HTMLVideoElement& videoElement, UIView *view, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
#if USE(WEB_THREAD)
    ASSERT(isMainThread());
#endif
    RetainPtr<UIView> viewRef = view;
    m_videoElement = &videoElement;
    m_playbackModel = PlaybackSessionModelMediaElement::create();
    m_playbackModel->addClient(*this);
    m_playbackModel->setMediaElement(m_videoElement.get());

    m_fullscreenModel = VideoFullscreenModelVideoElement::create();
    m_fullscreenModel->addClient(*this);
    m_fullscreenModel->setVideoElement(m_videoElement.get());

    bool allowsPictureInPicture = m_videoElement->webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);

    IntRect videoElementClientRect = elementRectInWindow(m_videoElement.get());
    FloatRect videoLayerFrame = FloatRect(FloatPoint(), videoElementClientRect.size());
    m_fullscreenModel->setVideoLayerFrame(videoLayerFrame);

    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), this, videoElementClientRect, viewRef, mode, allowsPictureInPicture] {
#if USE(WEB_THREAD)
        ASSERT(isUIThread());
#endif

        Ref<PlaybackSessionInterfaceAVKit> sessionInterface = PlaybackSessionInterfaceAVKit::create(*this);
        m_interface = VideoFullscreenInterfaceAVKit::create(sessionInterface.get());
        m_interface->setVideoFullscreenChangeObserver(this);
        m_interface->setVideoFullscreenModel(this);
        m_interface->setVideoFullscreenChangeObserver(this);

        m_videoFullscreenView = adoptNS([PAL::allocUIViewInstance() init]);

        m_interface->setupFullscreen(*m_videoFullscreenView.get(), videoElementClientRect, viewRef.get(), mode, allowsPictureInPicture, false);
    });
}

void VideoFullscreenControllerContext::exitFullscreen()
{
#if USE(WEB_THREAD)
    ASSERT(WebThreadIsCurrent() || isMainThread());
#endif
    IntRect clientRect = elementRectInWindow(m_videoElement.get());
    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), this, clientRect] {
#if USE(WEB_THREAD)
        ASSERT(isUIThread());
#endif
        m_interface->exitFullscreen(clientRect);
    });
}

void VideoFullscreenControllerContext::requestHideAndExitFullscreen()
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    m_interface->requestHideAndExitFullscreen();
}

@implementation WebVideoFullscreenController {
    RefPtr<VideoFullscreenControllerContext> _context;
    RefPtr<HTMLVideoElement> _videoElement;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    return self;
}

- (void)setVideoElement:(HTMLVideoElement*)videoElement
{
    _videoElement = videoElement;
}

- (HTMLVideoElement*)videoElement
{
    return _videoElement.get();
}

- (void)enterFullscreen:(UIView *)view mode:(HTMLMediaElementEnums::VideoFullscreenMode)mode
{
#if USE(WEB_THREAD)
    ASSERT(isMainThread());
#endif
    _context = VideoFullscreenControllerContext::create();
    _context->setController(self);
    _context->setUpFullscreen(*_videoElement.get(), view, mode);
}

- (void)exitFullscreen
{
#if USE(WEB_THREAD)
    ASSERT(WebThreadIsCurrent() || isMainThread());
#endif
    _context->exitFullscreen();
}

- (void)requestHideAndExitFullscreen
{
#if USE(WEB_THREAD)
    ASSERT(isUIThread());
#endif
    if (_context)
        _context->requestHideAndExitFullscreen();
}

- (void)didFinishFullscreen:(VideoFullscreenControllerContext*)context
{
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(context, context == _context);
    [[self retain] autorelease]; // retain self before breaking a retain cycle.
    _context->setController(nil);
    _context = nullptr;
    _videoElement = nullptr;
}

@end

#endif // !HAVE(AVKIT)

#endif // PLATFORM(IOS_FAMILY)
