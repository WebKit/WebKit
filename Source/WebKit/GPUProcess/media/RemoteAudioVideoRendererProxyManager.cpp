/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteAudioVideoRendererProxyManager.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "AudioVideoRendererRemoteMessageReceiverMessages.h"
#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "RemoteAudioVideoRendererProxyManagerMessages.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteVideoFrameObjectHeap.h"
#if ENABLE(LINEAR_MEDIA_PLAYER)
#include "VideoReceiverEndpointManager.h"
#endif
#if USE(AVFOUNDATION)
#include <WebCore/AudioVideoRendererAVFObjC.h>
#else
#include <WebCore/AudioVideoRenderer.h>
#endif
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/MediaSamplesBlock.h>
#include <WebCore/PlatformLayer.h>

#include <wtf/LoggerHelper.h>
#if PLATFORM(COCOA)
#include <wtf/MachSendRightAnnotated.h>
#endif
#include <wtf/Markable.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_gpuConnectionToWebProcess.get()->connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_gpuConnectionToWebProcess.get()->connection(), completion)

namespace WebCore {

}

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioVideoRendererProxyManager);

RefPtr<AudioVideoRenderer> RemoteAudioVideoRendererProxyManager::createRenderer()
{
#if USE(AVFOUNDATION)
    return AudioVideoRendererAVFObjC::create(Ref { m_gpuConnectionToWebProcess.get()->logger() }, LoggerHelper::uniqueLogIdentifier());
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

RemoteAudioVideoRendererProxyManager::RemoteAudioVideoRendererProxyManager(GPUConnectionToWebProcess& connection)
    : m_videoFrameObjectHeap(connection.videoFrameObjectHeap())
    , m_gpuConnectionToWebProcess(connection)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier { LoggerHelper::uniqueLogIdentifier() }
    , m_logger { connection.logger() }
#endif
{
}

RemoteAudioVideoRendererProxyManager::~RemoteAudioVideoRendererProxyManager() = default;

void RemoteAudioVideoRendererProxyManager::ref() const
{
    m_gpuConnectionToWebProcess.get()->ref();
}

void RemoteAudioVideoRendererProxyManager::deref() const
{
    m_gpuConnectionToWebProcess.get()->deref();
}

std::optional<SharedPreferencesForWebProcess> RemoteAudioVideoRendererProxyManager::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

void RemoteAudioVideoRendererProxyManager::create(RemoteAudioVideoRendererIdentifier identifier, WebCore::HTMLMediaElementIdentifier mediaElementIdentifier, WebCore::MediaPlayerIdentifier playerIdentifier)
{
    MESSAGE_CHECK(!m_renderers.contains(identifier));

    RefPtr renderer = createRenderer();
    ASSERT(renderer);
    if (!renderer)
        return;
    RendererContext context {
        .renderer = renderer.releaseNonNull(),
        .mediaElementIdentifier = mediaElementIdentifier,
        .playerIdentifier = playerIdentifier
    };

    context.renderer->notifyWhenErrorOccurs([weakThis = WeakPtr { *this }, identifier](PlatformMediaError error) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::ErrorOccurred(error), identifier);
    });

    context.renderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }, identifier] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier)) {
#if PLATFORM(COCOA)
            protectedThis->contextFor(identifier).layerHostingContextManager.setVideoLayerSizeIfPossible();
#endif
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::FirstFrameAvailable(protectedThis->stateFor(identifier)), identifier);
        }
    });

    context.renderer->notifyWhenRequiresFlushToResume([weakThis = WeakPtr { *this }, identifier] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::RequiresFlushToResume(protectedThis->stateFor(identifier)), identifier);
    });

    context.renderer->notifyRenderingModeChanged([weakThis = WeakPtr { *this }, identifier] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->rendereringModeChanged(identifier);
    });

    context.renderer->notifySizeChanged([weakThis = WeakPtr { *this }, identifier](const MediaTime& time, FloatSize size) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::SizeChanged(time, size, protectedThis->stateFor(identifier)), identifier);
    });

    context.renderer->notifyEffectiveRateChanged([weakThis = WeakPtr { *this }, identifier](double) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::EffectiveRateChanged(protectedThis->stateFor(identifier)), identifier);
    });

    context.renderer->setTimeObserver(200_ms, [weakThis = WeakPtr { *this }, identifier](const MediaTime&) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(protectedThis->stateFor(identifier)), identifier);
    });

#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr videoTarget = m_gpuConnectionToWebProcess.get()->videoReceiverEndpointManager().takeVideoTargetForMediaElementIdentifier(mediaElementIdentifier, playerIdentifier);
    context.renderer->setVideoTarget(videoTarget.get());
#endif

    m_renderers.set(identifier, WTFMove(context));

    m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(stateFor(identifier)), identifier);
}

void RemoteAudioVideoRendererProxyManager::shutdown(RemoteAudioVideoRendererIdentifier identifier)
{
    MESSAGE_CHECK(m_renderers.contains(identifier));

    if (auto iterator = m_renderers.find(identifier); iterator != m_renderers.end())
        m_renderers.remove(iterator);
}

RefPtr<WebCore::AudioVideoRenderer> RemoteAudioVideoRendererProxyManager::rendererFor(RemoteAudioVideoRendererIdentifier identifier) const
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(iterator != m_renderers.end(), m_gpuConnectionToWebProcess.get()->connection(), nullptr);
    return iterator->value.renderer.copyRef();
}

RefPtr<WebCore::AudioVideoRenderer> RemoteAudioVideoRendererProxyManager::rendererFor(std::optional<MediaPlayerIdentifier> playerIdentifier) const
{
    if (!playerIdentifier)
        return nullptr;

    for (auto& context : m_renderers.values()) {
        if (context.playerIdentifier == playerIdentifier)
            return context.renderer.copyRef();
    }
    return nullptr;
}

RemoteAudioVideoRendererProxyManager::RendererContext& RemoteAudioVideoRendererProxyManager::contextFor(RemoteAudioVideoRendererIdentifier identifier)
{
    auto iterator = m_renderers.find(identifier);
    ASSERT(iterator != m_renderers.end());
    return iterator->value;
}

void RemoteAudioVideoRendererProxyManager::setPreferences(RemoteAudioVideoRendererIdentifier identifier, WebCore::VideoRendererPreferences preferences)
{
    if (RefPtr renderer = rendererFor(identifier)) {
        renderer->setPreferences(preferences);
        contextFor(identifier).preferences = preferences;
    }
}

void RemoteAudioVideoRendererProxyManager::setHasProtectedVideoContent(RemoteAudioVideoRendererIdentifier identifier, bool hasProtected)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setHasProtectedVideoContent(hasProtected);
}

void RemoteAudioVideoRendererProxyManager::addTrack(RemoteAudioVideoRendererIdentifier identifier, WebCore::AudioVideoRenderer::TrackType type, CompletionHandler<void(Expected<TrackIdentifier, PlatformMediaError>)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(makeUnexpected(PlatformMediaError::NotSupportedError));
        return;
    }
    auto trackIdentifier = renderer->addTrack(type);
    completionHandler(trackIdentifier);
}

void RemoteAudioVideoRendererProxyManager::removeTrack(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->removeTrack(trackIdentifier);
}

void RemoteAudioVideoRendererProxyManager::requestMediaDataWhenReady(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;
    renderer->requestMediaDataWhenReady(trackIdentifier)->whenSettled(RunLoop::mainSingleton(), [identifier, trackIdentifier, weakThis = WeakPtr { *this }](auto result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return;
        protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::ReadyForMoreMediaData(trackIdentifier), identifier);
    });
}

void RemoteAudioVideoRendererProxyManager::enqueueSample(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier, WebCore::MediaSamplesBlock&& samplesBlock, std::optional<MediaTime> minimumPresentationTime, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(false);
        return;
    }
    if (RefPtr mediaSample = samplesBlock.toMediaSample()) {
        renderer->enqueueSample(trackIdentifier, mediaSample.releaseNonNull());
        completionHandler(renderer->isReadyForMoreSamples(trackIdentifier));
        return;
    }
    completionHandler(false);
}

void RemoteAudioVideoRendererProxyManager::notifyTimeReachedAndStall(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;
    renderer->notifyTimeReachedAndStall(time, [weakThis = WeakPtr { *this }, identifier](auto& time) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::StallTimeReached(time, protectedThis->stateFor(identifier)), identifier);
    });
}

void RemoteAudioVideoRendererProxyManager::cancelTimeReachedAction(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->cancelTimeReachedAction();
}

void RemoteAudioVideoRendererProxyManager::performTaskAtTime(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;
    renderer->performTaskAtTime(time, [weakThis = WeakPtr { *this }, time, identifier](auto) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::TaskTimeReached(time, protectedThis->stateFor(identifier)), identifier);
    });
}

void RemoteAudioVideoRendererProxyManager::flush(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->flush();
}

void RemoteAudioVideoRendererProxyManager::flushTrack(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->flushTrack(trackIdentifier);
}

void RemoteAudioVideoRendererProxyManager::applicationWillResignActive(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->applicationWillResignActive();
}

void RemoteAudioVideoRendererProxyManager::setSpatialTrackingInfo(RemoteAudioVideoRendererIdentifier identifier, bool prefersSpatialAudioExperience, WebCore::MediaPlayerSoundStageSize soundStage, const String& sceneIdentifier, const String& defaultLabel, const String& label)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setSpatialTrackingInfo(prefersSpatialAudioExperience, soundStage, sceneIdentifier, defaultLabel, label);
}

void RemoteAudioVideoRendererProxyManager::notifyWhenErrorOccurs(RemoteAudioVideoRendererIdentifier identifier, CompletionHandler<void(WebCore::PlatformMediaError)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(PlatformMediaError::NotSupportedError);
        return;
    }
    renderer->notifyWhenErrorOccurs([completionHandler = WTFMove(completionHandler)](auto error) mutable {
        completionHandler(error);
    });
}

// SynchronizerInterface
void RemoteAudioVideoRendererProxyManager::play(RemoteAudioVideoRendererIdentifier identifier, std::optional<MonotonicTime> hostTime)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->play(hostTime);
}

void RemoteAudioVideoRendererProxyManager::pause(RemoteAudioVideoRendererIdentifier identifier, std::optional<MonotonicTime> hostTime)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->pause(hostTime);
}

void RemoteAudioVideoRendererProxyManager::setRate(RemoteAudioVideoRendererIdentifier identifier, double rate)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setRate(rate);
}

void RemoteAudioVideoRendererProxyManager::stall(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->stall();
}

void RemoteAudioVideoRendererProxyManager::prepareToSeek(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->prepareToSeek();
}

void RemoteAudioVideoRendererProxyManager::seekTo(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time, CompletionHandler<void(WebCore::MediaTimePromise::Result&&)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(makeUnexpected(PlatformMediaError::NotSupportedError));
        return;
    }
    renderer->seekTo(time)->whenSettled(RunLoop::mainSingleton(), [protectedThis = Ref { *this }, identifier, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(protectedThis->stateFor(identifier)), identifier);
        completionHandler(WTFMove(result));
    });
}

void RemoteAudioVideoRendererProxyManager::setVolume(RemoteAudioVideoRendererIdentifier identifier, float volume)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setVolume(volume);
}

void RemoteAudioVideoRendererProxyManager::setMuted(RemoteAudioVideoRendererIdentifier identifier, bool muted)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setMuted(muted);
}

void RemoteAudioVideoRendererProxyManager::setPreservesPitchAndCorrectionAlgorithm(RemoteAudioVideoRendererIdentifier identifier, bool preservesPitch, std::optional<WebCore::MediaPlayerPitchCorrectionAlgorithm> algorithm)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setPreservesPitchAndCorrectionAlgorithm(preservesPitch, algorithm);
}

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
void RemoteAudioVideoRendererProxyManager::setOutputDeviceId(RemoteAudioVideoRendererIdentifier identifier, const String& deviceId)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setOutputDeviceId(deviceId);
}
#endif

void RemoteAudioVideoRendererProxyManager::setIsVisible(RemoteAudioVideoRendererIdentifier identifier, bool visible)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setIsVisible(visible);
}

void RemoteAudioVideoRendererProxyManager::setPresentationSize(RemoteAudioVideoRendererIdentifier identifier, const WebCore::IntSize& size)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setPresentationSize(size);
}

void RemoteAudioVideoRendererProxyManager::setShouldMaintainAspectRatio(RemoteAudioVideoRendererIdentifier identifier, bool maintain)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setShouldMaintainAspectRatio(maintain);
}

void RemoteAudioVideoRendererProxyManager::renderingCanBeAcceleratedChanged(RemoteAudioVideoRendererIdentifier identifier, bool renderingIsAccelerated)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->renderingCanBeAcceleratedChanged(renderingIsAccelerated);
}

void RemoteAudioVideoRendererProxyManager::contentBoxRectChanged(RemoteAudioVideoRendererIdentifier identifier, const WebCore::LayoutRect& rect)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->contentBoxRectChanged(rect);
}

void RemoteAudioVideoRendererProxyManager::notifyWhenHasAvailableVideoFrame(WebKit::RemoteAudioVideoRendererIdentifier identifier, bool notify)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;
    if (!notify) {
        renderer->notifyWhenHasAvailableVideoFrame({ });
        return;
    }
    renderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }, identifier](auto presentationTime, auto clockTime) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::HasAvailableVideoFrame(presentationTime, clockTime, protectedThis->stateFor(identifier)), identifier);
    });
}

void RemoteAudioVideoRendererProxyManager::expectMinimumUpcomingPresentationTime(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->expectMinimumUpcomingPresentationTime(time);
}

void RemoteAudioVideoRendererProxyManager::setShouldDisableHDR(RemoteAudioVideoRendererIdentifier identifier, bool disable)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setShouldDisableHDR(disable);
}

void RemoteAudioVideoRendererProxyManager::setPlatformDynamicRangeLimit(RemoteAudioVideoRendererIdentifier identifier, const WebCore::PlatformDynamicRangeLimit& limit)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setPlatformDynamicRangeLimit(limit);
}

void RemoteAudioVideoRendererProxyManager::setResourceOwner(RemoteAudioVideoRendererIdentifier identifier, const WebCore::ProcessIdentity& resourceOwner)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setResourceOwner(resourceOwner);
}

void RemoteAudioVideoRendererProxyManager::flushAndRemoveImage(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->flushAndRemoveImage();
}

void RemoteAudioVideoRendererProxyManager::currentVideoFrame(RemoteAudioVideoRendererIdentifier identifier, CompletionHandler<void(std::optional<RemoteVideoFrameProxy::Properties>)>&& completionHandler) const
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(std::nullopt);
        return;
    }
    std::optional<RemoteVideoFrameProxy::Properties> result;
    if (RefPtr videoFrame = renderer->currentVideoFrame())
        result = m_videoFrameObjectHeap->add(videoFrame.releaseNonNull());
    completionHandler(WTFMove(result));
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
void RemoteAudioVideoRendererProxyManager::setVideoFullscreenFrame(RemoteAudioVideoRendererIdentifier identifier, const WebCore::FloatRect& frame)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setVideoFullscreenFrame(frame);
}

void RemoteAudioVideoRendererProxyManager::isInFullscreenOrPictureInPictureChanged(RemoteAudioVideoRendererIdentifier identifier, bool isInFullscreen)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->isInFullscreenOrPictureInPictureChanged(isInFullscreen);
}
#endif

void RemoteAudioVideoRendererProxyManager::setTextTrackRepresentation(RemoteAudioVideoRendererIdentifier identifier, WebCore::TextTrackRepresentation* textRepresentation)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setTextTrackRepresentation(textRepresentation);
}

void RemoteAudioVideoRendererProxyManager::syncTextTrackBounds(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->syncTextTrackBounds();
}

RemoteAudioVideoRendererState RemoteAudioVideoRendererProxyManager::stateFor(RemoteAudioVideoRendererIdentifier identifier) const
{
    RefPtr renderer = rendererFor(identifier);
    ASSERT(renderer);
    if (!renderer)
        return { };
    return {
        .currentTime = renderer->currentTime(),
        .paused = renderer->paused(),
        .seeking = renderer->seeking(),
        .timeIsProgressing = renderer->timeIsProgressing(),
        .effectiveRate = renderer->effectiveRate(),
        .videoPlaybackQualityMetrics = renderer->videoPlaybackQualityMetrics()
    };
}

void RemoteAudioVideoRendererProxyManager::rendereringModeChanged(RemoteAudioVideoRendererIdentifier identifier)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());

    MESSAGE_CHECK(m_renderers.contains(identifier));

    auto state = stateFor(identifier);

#if PLATFORM(COCOA)
    auto& context = contextFor(identifier);

    bool canShowWhileLocked = false;
#if PLATFORM(IOS_FAMILY)
    canShowWhileLocked = context.preferences.contains(VideoRendererPreference::CanShowWhileLocked);
#endif

    // See webkit.org/b/299655
    SUPPRESS_FORWARD_DECL_ARG if (auto maybeHostingContext = context.layerHostingContextManager.createHostingContextIfNeeded(context.renderer->platformVideoLayer(), canShowWhileLocked))
        m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::LayerHostingContextChanged(state, *maybeHostingContext, context.layerHostingContextManager.videoLayerSize()), identifier);
#endif
    m_gpuConnectionToWebProcess.get()->connection().send(Messages::AudioVideoRendererRemoteMessageReceiver::RenderingModeChanged(state), identifier);
}

#if PLATFORM(COCOA)
void RemoteAudioVideoRendererProxyManager::setVideoLayerSizeFenced(RemoteAudioVideoRendererIdentifier identifier, const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString(), size.width(), "x", size.height());

    MESSAGE_CHECK(m_renderers.contains(identifier));

    auto& context = contextFor(identifier);
    context.layerHostingContextManager.setVideoLayerSizeFenced(size, WTF::MachSendRightAnnotated { sendRightAnnotated }, [&] {
        context.renderer->setVideoLayerSizeFenced(size, WTFMove(sendRightAnnotated));
    });
}
#endif

void RemoteAudioVideoRendererProxyManager::requestHostingContext(RemoteAudioVideoRendererIdentifier identifier, LayerHostingContextCallback&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());
#if PLATFORM(COCOA)
    MESSAGE_CHECK_COMPLETION(m_renderers.contains(identifier), completionHandler({ }));
    contextFor(identifier).layerHostingContextManager.requestHostingContext(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void RemoteAudioVideoRendererProxyManager::setLegacyCDMSession(RemoteAudioVideoRendererIdentifier identifier, std::optional<RemoteLegacyCDMSessionIdentifier> instanceId)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());

    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;

    if (!instanceId) {
        renderer->setCDMSession(nullptr);
        return;
    }
    if (RefPtr cdmSession = m_gpuConnectionToWebProcess.get()->protectedLegacyCdmFactoryProxy()->getSession(*instanceId))
        renderer->setCDMSession(cdmSession->protectedSession().get());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "Unable to find LegacyCDMSession: ", instanceId->loggingString());
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteAudioVideoRendererProxyManager::setCDMInstance(RemoteAudioVideoRendererIdentifier identifier, std::optional<RemoteCDMInstanceIdentifier> instanceId)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());

    RefPtr renderer = rendererFor(identifier);
    if (!renderer)
        return;

    if (!instanceId) {
        renderer->setCDMInstance(nullptr);
        return;
    }
    if (RefPtr instanceProxy = m_gpuConnectionToWebProcess.get()->protectedCdmFactoryProxy()->getInstance(*instanceId))
        renderer->setCDMInstance(&instanceProxy->instance());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "Unable to find CDMInstance: ", instanceId->loggingString());
}

void RemoteAudioVideoRendererProxyManager::setInitData(RemoteAudioVideoRendererIdentifier identifier, Ref<WebCore::SharedBuffer> initData, CompletionHandler<void(Expected<void, WebCore::PlatformMediaError>)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setInitData(initData)->whenSettled(RunLoop::mainSingleton(), WTFMove(completionHandler));
}

void RemoteAudioVideoRendererProxyManager::attemptToDecrypt(RemoteAudioVideoRendererIdentifier identifier)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());
    if (RefPtr renderer = rendererFor(identifier))
        renderer->attemptToDecrypt();
}

#endif


#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemoteAudioVideoRendererProxyManager::logChannel() const
{
    return WebKit2LogMedia;
}
#endif

} // namespace WebKit

#endif
