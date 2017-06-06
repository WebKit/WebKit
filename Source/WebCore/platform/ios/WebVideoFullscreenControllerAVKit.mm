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

#if PLATFORM(IOS)

#import "WebVideoFullscreenControllerAVKit.h"

#import "Logging.h"
#import "MediaSelectionOption.h"
#import "QuartzCoreSPI.h"
#import "SoftLinking.h"
#import "TimeRanges.h"
#import "WebPlaybackSessionInterfaceAVKit.h"
#import "WebPlaybackSessionModelMediaElement.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenInterfaceAVKit.h"
#import "WebVideoFullscreenModelVideoElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <UIKit/UIView.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/RenderVideo.h>
#import <WebCore/WebCoreThreadRun.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIView)

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

class WebVideoFullscreenControllerContext;

@interface WebVideoFullscreenController (delegate)
-(void)didFinishFullscreen:(WebVideoFullscreenControllerContext*)context;
@end

class WebVideoFullscreenControllerContext final
    : private WebVideoFullscreenModel
    , private WebVideoFullscreenModelClient
    , private WebVideoFullscreenChangeObserver
    , private WebPlaybackSessionModel
    , private WebPlaybackSessionModelClient
    , public ThreadSafeRefCounted<WebVideoFullscreenControllerContext> {

public:
    static Ref<WebVideoFullscreenControllerContext> create()
    {
        return adoptRef(*new WebVideoFullscreenControllerContext);
    }

    void setController(WebVideoFullscreenController* controller) { m_controller = controller; }
    void setUpFullscreen(HTMLVideoElement&, UIView *, HTMLMediaElementEnums::VideoFullscreenMode);
    void exitFullscreen();
    void requestHideAndExitFullscreen();
    void invalidate();

private:
    WebVideoFullscreenControllerContext() { }

    // WebVideoFullscreenChangeObserver
    void didSetupFullscreen() override;
    void didEnterFullscreen() override { }
    void didExitFullscreen() override;
    void didCleanupFullscreen() override;
    void fullscreenMayReturnToInline() override;

    // WebVideoFullscreenModelClient
    void hasVideoChanged(bool) override;
    void videoDimensionsChanged(const FloatSize&) override;

    // WebPlaybackSessionModel
    void addClient(WebPlaybackSessionModelClient&) override;
    void removeClient(WebPlaybackSessionModelClient&) override;
    void play() override;
    void pause() override;
    void togglePlayState() override;
    void beginScrubbing() override;
    void endScrubbing() override;
    void seekToTime(double) override;
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

    // WebPlaybackSessionModelClient
    void durationChanged(double) override;
    void currentTimeChanged(double currentTime, double anchorTime) override;
    void bufferedTimeChanged(double) override;
    void rateChanged(bool isPlaying, float playbackRate) override;
    void seekableRangesChanged(const TimeRanges&) override;
    void canPlayFastReverseChanged(bool) override;
    void audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    void legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    void externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) override;
    void wirelessVideoPlaybackDisabledChanged(bool) override;
    void mutedChanged(bool) override;

    // WebVideoFullscreenModel
    void addClient(WebVideoFullscreenModelClient&) override;
    void removeClient(WebVideoFullscreenModelClient&) override;
    void requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia = false) override;
    void setVideoLayerFrame(FloatRect) override;
    void setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity) override;
    void fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode) override;
    bool isVisible() const override;
    bool hasVideo() const override;
    FloatSize videoDimensions() const override;
    bool isMuted() const override;

    HashSet<WebPlaybackSessionModelClient*> m_playbackClients;
    HashSet<WebVideoFullscreenModelClient*> m_fullscreenClients;
    RefPtr<WebVideoFullscreenInterfaceAVKit> m_interface;
    RefPtr<WebVideoFullscreenModelVideoElement> m_fullscreenModel;
    RefPtr<WebPlaybackSessionModelMediaElement> m_playbackModel;
    RefPtr<HTMLVideoElement> m_videoElement;
    RetainPtr<UIView> m_videoFullscreenView;
    RetainPtr<WebVideoFullscreenController> m_controller;
};

#pragma mark WebVideoFullscreenChangeObserver

void WebVideoFullscreenControllerContext::didSetupFullscreen()
{
    ASSERT(isUIThread());
    RetainPtr<CALayer> videoFullscreenLayer = [m_videoFullscreenView layer];
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, videoFullscreenLayer] {
        [videoFullscreenLayer setBackgroundColor:cachedCGColor(WebCore::Color::transparent)];
        m_fullscreenModel->setVideoFullscreenLayer(videoFullscreenLayer.get(), [protectedThis, this] {
            dispatch_async(dispatch_get_main_queue(), [protectedThis, this] {
                m_interface->enterFullscreen();
            });
        });
    });
}

void WebVideoFullscreenControllerContext::didExitFullscreen()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        m_fullscreenModel->setVideoFullscreenLayer(nil, [protectedThis, this] {
            dispatch_async(dispatch_get_main_queue(), [protectedThis, this] {
                m_interface->cleanupFullscreen();
            });
        });
    });
}

void WebVideoFullscreenControllerContext::didCleanupFullscreen()
{
    ASSERT(isUIThread());
    m_interface->setWebVideoFullscreenModel(nullptr);
    m_interface->setWebVideoFullscreenChangeObserver(nullptr);
    m_interface = nullptr;
    m_videoFullscreenView = nil;

    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        m_fullscreenModel->setVideoFullscreenLayer(nil);
        m_fullscreenModel->setVideoElement(nullptr);
        m_playbackModel->setMediaElement(nullptr);
        m_fullscreenModel->removeClient(*this);
        m_fullscreenModel = nullptr;
        m_videoElement = nullptr;

        [m_controller didFinishFullscreen:this];
    });
}

void WebVideoFullscreenControllerContext::fullscreenMayReturnToInline()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        IntRect clientRect = elementRectInWindow(m_videoElement.get());
        dispatch_async(dispatch_get_main_queue(), [protectedThis, this, clientRect] {
            m_interface->preparedToReturnToInline(true, clientRect);
        });
    });
}

#pragma mark WebPlaybackSessionModelClient

void WebVideoFullscreenControllerContext::durationChanged(double duration)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, duration] {
            protectedThis->durationChanged(duration);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->durationChanged(duration);
}

void WebVideoFullscreenControllerContext::currentTimeChanged(double currentTime, double anchorTime)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, currentTime, anchorTime] {
            protectedThis->currentTimeChanged(currentTime, anchorTime);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->currentTimeChanged(currentTime, anchorTime);
}

void WebVideoFullscreenControllerContext::bufferedTimeChanged(double bufferedTime)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, bufferedTime] {
            protectedThis->bufferedTimeChanged(bufferedTime);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->bufferedTimeChanged(bufferedTime);
}

void WebVideoFullscreenControllerContext::rateChanged(bool isPlaying, float playbackRate)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, isPlaying, playbackRate] {
            protectedThis->rateChanged(isPlaying, playbackRate);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->rateChanged(isPlaying, playbackRate);
}

void WebVideoFullscreenControllerContext::hasVideoChanged(bool hasVideo)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, hasVideo] {
            protectedThis->hasVideoChanged(hasVideo);
        });
        return;
    }

    for (auto& client : m_fullscreenClients)
        client->hasVideoChanged(hasVideo);
}

void WebVideoFullscreenControllerContext::videoDimensionsChanged(const FloatSize& videoDimensions)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, videoDimensions = videoDimensions] {
            protectedThis->videoDimensionsChanged(videoDimensions);
        });
        return;
    }

    for (auto& client : m_fullscreenClients)
        client->videoDimensionsChanged(videoDimensions);
}

void WebVideoFullscreenControllerContext::seekableRangesChanged(const TimeRanges& timeRanges)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, platformTimeRanges = timeRanges.ranges()] {
            protectedThis->seekableRangesChanged(TimeRanges::create(platformTimeRanges));
        });
        return;
    }

    for (auto &client : m_playbackClients)
        client->seekableRangesChanged(timeRanges);
}

void WebVideoFullscreenControllerContext::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, canPlayFastReverse] {
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

void WebVideoFullscreenControllerContext::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, options = isolatedCopy(options), selectedIndex] {
            protectedThis->audioMediaSelectionOptionsChanged(options, selectedIndex);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->audioMediaSelectionOptionsChanged(options, selectedIndex);
}

void WebVideoFullscreenControllerContext::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, options = isolatedCopy(options), selectedIndex] {
            protectedThis->legibleMediaSelectionOptionsChanged(options, selectedIndex);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->legibleMediaSelectionOptionsChanged(options, selectedIndex);
}

void WebVideoFullscreenControllerContext::externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType type, const String& localizedDeviceName)
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

void WebVideoFullscreenControllerContext::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, disabled] {
            protectedThis->wirelessVideoPlaybackDisabledChanged(disabled);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->wirelessVideoPlaybackDisabledChanged(disabled);
}

void WebVideoFullscreenControllerContext::mutedChanged(bool muted)
{
    if (WebThreadIsCurrent()) {
        RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis, muted] {
            protectedThis->mutedChanged(muted);
        });
        return;
    }

    for (auto& client : m_playbackClients)
        client->mutedChanged(muted);
}

#pragma mark WebVideoFullscreenModel

void WebVideoFullscreenControllerContext::addClient(WebVideoFullscreenModelClient& client)
{
    ASSERT(!m_fullscreenClients.contains(&client));
    m_fullscreenClients.add(&client);
}

void WebVideoFullscreenControllerContext::removeClient(WebVideoFullscreenModelClient& client)
{
    ASSERT(m_fullscreenClients.contains(&client));
    m_fullscreenClients.remove(&client);
}

void WebVideoFullscreenControllerContext::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, mode, finishedWithMedia] {
        if (m_fullscreenModel)
            m_fullscreenModel->requestFullscreenMode(mode, finishedWithMedia);
    });
}

void WebVideoFullscreenControllerContext::setVideoLayerFrame(FloatRect frame)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    RetainPtr<CALayer> videoFullscreenLayer = [m_videoFullscreenView layer];
    
    [videoFullscreenLayer setSublayerTransform:[videoFullscreenLayer transform]];

    dispatch_async(dispatch_get_main_queue(), ^{
        WebThreadRun([protectedThis, this, frame, videoFullscreenLayer] {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            [CATransaction setAnimationDuration:0];
            
            [videoFullscreenLayer setSublayerTransform:CATransform3DIdentity];
            
            if (m_fullscreenModel)
                m_fullscreenModel->setVideoLayerFrame(frame);
            [CATransaction commit];
        });
    });
}

void WebVideoFullscreenControllerContext::setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity videoGravity)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, videoGravity] {
        if (m_fullscreenModel)
            m_fullscreenModel->setVideoLayerGravity(videoGravity);
    });
}

void WebVideoFullscreenControllerContext::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, mode] {
        if (m_fullscreenModel)
            m_fullscreenModel->fullscreenModeChanged(mode);
    });
}

bool WebVideoFullscreenControllerContext::isVisible() const
{
    ASSERT(isUIThread());
    return m_fullscreenModel ? m_fullscreenModel->isVisible() : false;
}

bool WebVideoFullscreenControllerContext::hasVideo() const
{
    ASSERT(isUIThread());
    return m_fullscreenModel ? m_fullscreenModel->hasVideo() : false;
}

bool WebVideoFullscreenControllerContext::isMuted() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->isMuted() : false;
}

FloatSize WebVideoFullscreenControllerContext::videoDimensions() const
{
    ASSERT(isUIThread());
    return m_fullscreenModel ? m_fullscreenModel->videoDimensions() : FloatSize();
}

#pragma mark - WebPlaybackSessionModel

void WebVideoFullscreenControllerContext::addClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(!m_playbackClients.contains(&client));
    m_playbackClients.add(&client);
}

void WebVideoFullscreenControllerContext::removeClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(m_playbackClients.contains(&client));
    m_playbackClients.remove(&client);
}

void WebVideoFullscreenControllerContext::play()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->play();
    });
}

void WebVideoFullscreenControllerContext::pause()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->pause();
    });
}

void WebVideoFullscreenControllerContext::togglePlayState()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->togglePlayState();
    });
}

void WebVideoFullscreenControllerContext::toggleMuted()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->toggleMuted();
    });
}

void WebVideoFullscreenControllerContext::beginScrubbing()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->beginScrubbing();
    });
}

void WebVideoFullscreenControllerContext::endScrubbing()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->endScrubbing();
    });
}

void WebVideoFullscreenControllerContext::seekToTime(double time)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, time] {
        if (m_playbackModel)
            m_playbackModel->seekToTime(time);
    });
}

void WebVideoFullscreenControllerContext::fastSeek(double time)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, time] {
        if (m_playbackModel)
            m_playbackModel->fastSeek(time);
    });
}

void WebVideoFullscreenControllerContext::beginScanningForward()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->beginScanningForward();
    });
}

void WebVideoFullscreenControllerContext::beginScanningBackward()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->beginScanningBackward();
    });
}

void WebVideoFullscreenControllerContext::endScanning()
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this] {
        if (m_playbackModel)
            m_playbackModel->endScanning();
    });
}

void WebVideoFullscreenControllerContext::selectAudioMediaOption(uint64_t index)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, index] {
        if (m_playbackModel)
            m_playbackModel->selectAudioMediaOption(index);
    });
}

void WebVideoFullscreenControllerContext::selectLegibleMediaOption(uint64_t index)
{
    ASSERT(isUIThread());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    WebThreadRun([protectedThis, this, index] {
        if (m_playbackModel)
            m_playbackModel->selectLegibleMediaOption(index);
    });
}

double WebVideoFullscreenControllerContext::duration() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->duration() : 0;
}

double WebVideoFullscreenControllerContext::currentTime() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->currentTime() : 0;
}

double WebVideoFullscreenControllerContext::bufferedTime() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->bufferedTime() : 0;
}

bool WebVideoFullscreenControllerContext::isPlaying() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->isPlaying() : false;
}

float WebVideoFullscreenControllerContext::playbackRate() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->playbackRate() : 0;
}

Ref<TimeRanges> WebVideoFullscreenControllerContext::seekableRanges() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->seekableRanges() : TimeRanges::create();
}

bool WebVideoFullscreenControllerContext::canPlayFastReverse() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->canPlayFastReverse() : false;
}

Vector<MediaSelectionOption> WebVideoFullscreenControllerContext::audioMediaSelectionOptions() const
{
    ASSERT(isUIThread());
    if (m_playbackModel)
        return m_playbackModel->audioMediaSelectionOptions();
    return { };
}

uint64_t WebVideoFullscreenControllerContext::audioMediaSelectedIndex() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->audioMediaSelectedIndex() : -1;
}

Vector<MediaSelectionOption> WebVideoFullscreenControllerContext::legibleMediaSelectionOptions() const
{
    ASSERT(isUIThread());
    if (m_playbackModel)
        return m_playbackModel->legibleMediaSelectionOptions();
    return { };
}

uint64_t WebVideoFullscreenControllerContext::legibleMediaSelectedIndex() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->legibleMediaSelectedIndex() : -1;
}

bool WebVideoFullscreenControllerContext::externalPlaybackEnabled() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->externalPlaybackEnabled() : false;
}

WebPlaybackSessionModel::ExternalPlaybackTargetType WebVideoFullscreenControllerContext::externalPlaybackTargetType() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->externalPlaybackTargetType() : TargetTypeNone;
}

String WebVideoFullscreenControllerContext::externalPlaybackLocalizedDeviceName() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->externalPlaybackLocalizedDeviceName() : String();
}

bool WebVideoFullscreenControllerContext::wirelessVideoPlaybackDisabled() const
{
    ASSERT(isUIThread());
    return m_playbackModel ? m_playbackModel->wirelessVideoPlaybackDisabled() : true;
}

#pragma mark Other

void WebVideoFullscreenControllerContext::setUpFullscreen(HTMLVideoElement& videoElement, UIView *view, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(isMainThread());
    RetainPtr<UIView> viewRef = view;
    m_videoElement = &videoElement;
    m_playbackModel = WebPlaybackSessionModelMediaElement::create();
    m_playbackModel->addClient(*this);
    m_playbackModel->setMediaElement(m_videoElement.get());

    m_fullscreenModel = WebVideoFullscreenModelVideoElement::create();
    m_fullscreenModel->addClient(*this);
    m_fullscreenModel->setVideoElement(m_videoElement.get());

    bool allowsPictureInPicture = m_videoElement->webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);

    IntRect videoElementClientRect = elementRectInWindow(m_videoElement.get());
    FloatRect videoLayerFrame = FloatRect(FloatPoint(), videoElementClientRect.size());
    m_fullscreenModel->setVideoLayerFrame(videoLayerFrame);

    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    dispatch_async(dispatch_get_main_queue(), [protectedThis, this, videoElementClientRect, viewRef, mode, allowsPictureInPicture] {
        ASSERT(isUIThread());

        Ref<WebPlaybackSessionInterfaceAVKit> sessionInterface = WebPlaybackSessionInterfaceAVKit::create(*this);
        m_interface = WebVideoFullscreenInterfaceAVKit::create(sessionInterface.get());
        m_interface->setWebVideoFullscreenChangeObserver(this);
        m_interface->setWebVideoFullscreenModel(this);
        m_interface->setWebVideoFullscreenChangeObserver(this);

        m_videoFullscreenView = adoptNS([allocUIViewInstance() init]);
        
        m_interface->setupFullscreen(*m_videoFullscreenView.get(), videoElementClientRect, viewRef.get(), mode, allowsPictureInPicture);
    });
}

void WebVideoFullscreenControllerContext::exitFullscreen()
{
    ASSERT(WebThreadIsCurrent() || isMainThread());
    IntRect clientRect = elementRectInWindow(m_videoElement.get());
    RefPtr<WebVideoFullscreenControllerContext> protectedThis(this);
    dispatch_async(dispatch_get_main_queue(), [protectedThis, this, clientRect] {
        ASSERT(isUIThread());
        m_interface->exitFullscreen(clientRect);
    });
}

void WebVideoFullscreenControllerContext::requestHideAndExitFullscreen()
{
    ASSERT(isUIThread());
    m_interface->requestHideAndExitFullscreen();
}

@implementation WebVideoFullscreenController {
    RefPtr<WebVideoFullscreenControllerContext> _context;
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
    ASSERT(isMainThread());
    _context = WebVideoFullscreenControllerContext::create();
    _context->setController(self);
    _context->setUpFullscreen(*_videoElement.get(), view, mode);
}

- (void)exitFullscreen
{
    ASSERT(WebThreadIsCurrent() || isMainThread());
    _context->exitFullscreen();
}

- (void)requestHideAndExitFullscreen
{
    ASSERT(isUIThread());
    if (_context)
        _context->requestHideAndExitFullscreen();
}

- (void)didFinishFullscreen:(WebVideoFullscreenControllerContext*)context
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

#endif // PLATFORM(IOS)
