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

#import "PlaybackSessionInterfaceLMK.h"
#import "WKSLinearMediaPlayer.h"
#import "WKSLinearMediaTypes.h"
#import <QuartzCore/CALayer.h>
#import <UIKit/UIKit.h>
#import <WebCore/AudioSession.h>
#import <WebCore/Color.h>
#import <WebCore/IntRect.h>
#import <WebCore/WebAVPlayerLayerView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UUID.h>
#import <wtf/text/MakeString.h>

#import "WebKitSwiftSoftLink.h"

@interface WKLinearMediaKitCaptionsLayer : CALayer {
    ThreadSafeWeakPtr<WebKit::VideoPresentationInterfaceLMK> _parent;
}
- (id)initWithParent:(WebKit::VideoPresentationInterfaceLMK&)parent;
@end

@implementation WKLinearMediaKitCaptionsLayer
- (id)initWithParent:(WebKit::VideoPresentationInterfaceLMK&)parent
{
    self = [super init];
    if (!self)
        return nil;

    _parent = parent;
    return self;
}

- (void)layoutSublayers
{
    [super layoutSublayers];
    if (RefPtr parent = _parent.get())
        parent->captionsLayerBoundsChanged(self.bounds);
}
@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoPresentationInterfaceLMK);

VideoPresentationInterfaceLMK::~VideoPresentationInterfaceLMK()
{
}

Ref<VideoPresentationInterfaceLMK> VideoPresentationInterfaceLMK::create(WebCore::PlaybackSessionInterfaceIOS& playbackSessionInterface)
{
    return adoptRef(*new VideoPresentationInterfaceLMK(playbackSessionInterface));
}

VideoPresentationInterfaceLMK::VideoPresentationInterfaceLMK(WebCore::PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : VideoPresentationInterfaceIOS { playbackSessionInterface }
{
}

WKSLinearMediaPlayer *VideoPresentationInterfaceLMK::linearMediaPlayer() const
{
    return playbackSessionInterface().linearMediaPlayer();
}

void VideoPresentationInterfaceLMK::setSpatialImmersive(bool immersive)
{
    linearMediaPlayer().spatialImmersive = immersive;
}

void VideoPresentationInterfaceLMK::setupFullscreen(const WebCore::FloatRect& initialRect, const WebCore::FloatSize& videoDimensions, UIView* parentView, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode, bool allowsPictureInPicturePlayback, bool standby, bool blocksReturnToFullscreenFromPictureInPicture)
{
    linearMediaPlayer().contentDimensions = videoDimensions;
    if (!linearMediaPlayer().enteredFromInline && playerViewController()) {
        playableViewController().automaticallyDockOnFullScreenPresentation = NO;
        playableViewController().dismissFullScreenOnExitingDocking = NO;
    }
    VideoPresentationInterfaceIOS::setupFullscreen(initialRect, videoDimensions, parentView, mode, allowsPictureInPicturePlayback, standby, blocksReturnToFullscreenFromPictureInPicture);
}

void VideoPresentationInterfaceLMK::finalizeSetup()
{
    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }] {
        if (RefPtr model = protectedThis->videoPresentationModel())
            model->didSetupFullscreen();
    });
}

void VideoPresentationInterfaceLMK::setupPlayerViewController()
{
    linearMediaPlayer().captionLayer = captionsLayer();

    ensurePlayableViewController();
}

void VideoPresentationInterfaceLMK::invalidatePlayerViewController()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_playerViewController = nil;
}

void VideoPresentationInterfaceLMK::presentFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().startObservingNowPlayingMetadata();
    [linearMediaPlayer() enterFullscreenWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (BOOL success, NSError *error) {
        if (auto* playbackSessionModel = this->playbackSessionModel()) {
            playbackSessionModel->setSpatialTrackingLabel(m_spatialTrackingLabel);
            playbackSessionModel->setSoundStageSize(WebCore::AudioSessionSoundStageSize::Large);

            playableViewController().prefersAutoDimming = playbackSessionModel->prefersAutoDimming();
        }
        completionHandler(success, error);
    }).get()];
}

void VideoPresentationInterfaceLMK::dismissFullscreen(bool animated, Function<void(BOOL, NSError *)>&& completionHandler)
{
    playbackSessionInterface().stopObservingNowPlayingMetadata();
    [linearMediaPlayer() exitFullscreenWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (BOOL success, NSError *error) {
        if (auto* playbackSessionModel = this->playbackSessionModel()) {
            playbackSessionModel->setSpatialTrackingLabel(nullString());
            playbackSessionModel->setSoundStageSize(WebCore::AudioSessionSoundStageSize::Automatic);

            playbackSessionModel->setPrefersAutoDimming(playableViewController().prefersAutoDimming);
        }
        completionHandler(success, error);
    }).get()];
}

void VideoPresentationInterfaceLMK::enterExternalPlayback(CompletionHandler<void(bool, UIViewController *)>&& enterHandler, CompletionHandler<void(bool)>&& exitHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (linearMediaPlayer().presentationState != WKSLinearMediaPresentationStateInline || linearMediaPlayer().isImmersiveVideo) {
        enterHandler(false, nil);
        exitHandler(false);
        return;
    }

    setupPlayerViewController();
    m_exitExternalPlaybackHandler = WTFMove(exitHandler);
    playbackSessionInterface().startObservingNowPlayingMetadata();

    // Puts the player into `enteringExternal` state.
    [linearMediaPlayer() enterExternalPlaybackWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, handler = WTFMove(enterHandler)] (BOOL success, NSError *error) mutable {
        if (auto* playbackSessionModel = this->playbackSessionModel()) {
            playbackSessionModel->setSpatialTrackingLabel(m_spatialTrackingLabel);
            playbackSessionModel->setSoundStageSize(WebCore::AudioSessionSoundStageSize::Large);
        }

        handler(success, [m_playerViewController viewController]);
    }).get()];

    // The playerIdentifier must be set before the videoReceiverEndpoint is set. Once this interface receives notice
    // that `didSetPlayerIdentifier`, the player transitions to `external` state.
    playbackSessionInterface().playbackSessionModel()->setPlayerIdentifierForVideoElement();
}

void VideoPresentationInterfaceLMK::exitExternalPlayback()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(m_exitExternalPlaybackHandler);
    auto exitHandler = std::exchange(m_exitExternalPlaybackHandler, nullptr);

    WKSLinearMediaPresentationState presentationState = linearMediaPlayer().presentationState;
    if (presentationState != WKSLinearMediaPresentationStateEnteringExternal && presentationState != WKSLinearMediaPresentationStateExternal) {
        if (exitHandler)
            exitHandler(false);

        return;
    }

    playbackSessionInterface().stopObservingNowPlayingMetadata();
    [linearMediaPlayer() exitExternalPlaybackWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, handler = WTFMove(exitHandler)] (BOOL success, NSError *error) mutable {
        if (auto* playbackSessionModel = this->playbackSessionModel()) {
            playbackSessionModel->setSpatialTrackingLabel(nullString());
            playbackSessionModel->setSoundStageSize(WebCore::AudioSessionSoundStageSize::Automatic);
        }
        invalidatePlayerViewController();

        if (RefPtr model = this->videoPresentationModel())
            model->didExitExternalPlayback();

        if (handler)
            handler(success);
    }).get()];
}

bool VideoPresentationInterfaceLMK::cleanupExternalPlayback()
{
    WKSLinearMediaPresentationState presentationState = linearMediaPlayer().presentationState;
    if (presentationState != WKSLinearMediaPresentationStateEnteringExternal && presentationState != WKSLinearMediaPresentationStateExternal)
        return false;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    exitExternalPlayback();
    return true;
}

void VideoPresentationInterfaceLMK::didSetPlayerIdentifier()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    WKSLinearMediaPresentationState presentationState = linearMediaPlayer().presentationState;
    if (presentationState == WKSLinearMediaPresentationStateEnteringExternal)
        [linearMediaPlayer() completeEnterExternalPlayback];
    else if (presentationState == WKSLinearMediaPresentationStateExternal) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "playerIdentifier changed while in externalPlayback");
        exitExternalPlayback();
    }
}

void VideoPresentationInterfaceLMK::didSetVideoReceiverEndpoint()
{
    if (linearMediaPlayer().presentationState != WKSLinearMediaPresentationStateExternal)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (RefPtr model = this->videoPresentationModel())
        model->didEnterExternalPlayback();
}

UIViewController *VideoPresentationInterfaceLMK::playerViewController() const
{
    return [m_playerViewController viewController];
}

void VideoPresentationInterfaceLMK::setContentDimensions(const WebCore::FloatSize& contentDimensions)
{
    linearMediaPlayer().contentDimensions = contentDimensions;
}

void VideoPresentationInterfaceLMK::setShowsPlaybackControls(bool showsPlaybackControls)
{
    linearMediaPlayer().showsPlaybackControls = showsPlaybackControls;
}

CALayer *VideoPresentationInterfaceLMK::captionsLayer()
{
    if (m_captionsLayer)
        return m_captionsLayer.get();

    m_captionsLayer = adoptNS([[WKLinearMediaKitCaptionsLayer alloc] initWithParent:*this]);
    [m_captionsLayer setName:@"Captions Layer"];

    m_spatialTrackingLayer = adoptNS([[CALayer alloc] init]);
    [m_spatialTrackingLayer setSeparatedState:kCALayerSeparatedStateTracked];
    m_spatialTrackingLabel = makeString(createVersion4UUIDString());
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    if (prefersSpatialAudioExperience())
        [m_spatialTrackingLayer setValue:m_spatialTrackingLabel.createNSString().get() forKeyPath:@"separatedOptions.AudioTether"];
    else
#endif
        [m_spatialTrackingLayer setValue:m_spatialTrackingLabel.createNSString().get() forKeyPath:@"separatedOptions.STSLabel"];
    [m_captionsLayer addSublayer:m_spatialTrackingLayer.get()];

    return m_captionsLayer.get();
}

void VideoPresentationInterfaceLMK::captionsLayerBoundsChanged(const WebCore::FloatRect& bounds)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    [m_spatialTrackingLayer setPosition:bounds.center()];
#endif
    if (RefPtr model = videoPresentationModel())
        model->setTextTrackRepresentationBounds(enclosingIntRect(bounds));
}

void VideoPresentationInterfaceLMK::setupCaptionsLayer(CALayer *, const WebCore::FloatSize& initialSize)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [captionsLayer() removeFromSuperlayer];
    [captionsLayer() setAnchorPoint:CGPointZero];
    [captionsLayer() setBounds:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
    [CATransaction commit];
}

WKSPlayableViewControllerHost *VideoPresentationInterfaceLMK::playableViewController()
{
    ensurePlayableViewController();
    return m_playerViewController.get();
}

void VideoPresentationInterfaceLMK::ensurePlayableViewController()
{
    if (m_playerViewController)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_playerViewController = [linearMediaPlayer() makeViewController];
    [m_playerViewController viewController].view.alpha = 0;

    if (auto* playbackSessionModel = playbackSessionInterface().playbackSessionModel())
        [m_playerViewController setPrefersAutoDimming:playbackSessionModel->prefersAutoDimming()];
}

void VideoPresentationInterfaceLMK::swapFullscreenModesWith(VideoPresentationInterfaceIOS& otherInterfaceIOS)
{
    auto& otherInterface = static_cast<VideoPresentationInterfaceLMK&>(otherInterfaceIOS);
    std::swap(m_playerViewController, otherInterface.m_playerViewController);

    auto currentMode = mode();
    auto previousMode = otherInterface.mode();

    setMode(previousMode, true);
    otherInterface.setMode(currentMode, true);
}

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
