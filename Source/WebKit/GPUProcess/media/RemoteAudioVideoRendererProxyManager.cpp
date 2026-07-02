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
#include "RemoteCDMFactoryProxy.h"
#include "RemoteCDMInstanceProxy.h"
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
#include <WebCore/SharedTimebase.h>

#include <wtf/LoggerHelper.h>
#if PLATFORM(COCOA)
#include <wtf/MachSendRightAnnotated.h>
#endif
#include <wtf/Markable.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_gpuConnectionToWebProcess.get()->connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_gpuConnectionToWebProcess.get()->connection(), completion)

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioVideoRendererProxyManager);

template<typename Message>
void RemoteAudioVideoRendererProxyManager::publishAndSend(RemoteAudioVideoRendererIdentifier identifier, Message&& message)
{
    auto iterator = m_renderers.find(identifier);
    if (iterator == m_renderers.end())
        return;
    m_gpuConnectionToWebProcess.get()->connection().send(std::forward<Message>(message), identifier);
}

RefPtr<AudioVideoRenderer> RemoteAudioVideoRendererProxyManager::createRenderer(UniqueRef<SharedTimebase>&& sharedTimebase)
{
#if USE(AVFOUNDATION)
    return AudioVideoRendererAVFObjC::create(Ref { m_gpuConnectionToWebProcess.get()->logger() }, LoggerHelper::uniqueLogIdentifier(), WTF::move(sharedTimebase));
#else
    UNUSED_PARAM(sharedTimebase);
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

ThreadSafeWeakPtrControlBlock& RemoteAudioVideoRendererProxyManager::controlBlock() const
{
    return m_gpuConnectionToWebProcess.get()->controlBlock();
}

std::optional<SharedPreferencesForWebProcess> RemoteAudioVideoRendererProxyManager::sharedPreferencesForWebProcess() const
{
    return m_gpuConnectionToWebProcess.get()->sharedPreferencesForWebProcess();
}

void RemoteAudioVideoRendererProxyManager::create(RemoteAudioVideoRendererIdentifier identifier, WebCore::HTMLMediaElementIdentifier mediaElementIdentifier, WebCore::MediaPlayerIdentifier playerIdentifier, CompletionHandler<void(std::optional<WebCore::SharedTimebaseHandle>)>&& completionHandler)
{
    MESSAGE_CHECK(!m_renderers.contains(identifier));

    auto sharedTimebase = SharedTimebase::create();
    if (!sharedTimebase) {
        completionHandler(std::nullopt);
        return;
    }
    auto handle = sharedTimebase->createHandle();
    if (!handle) {
        completionHandler(std::nullopt);
        return;
    }

    RefPtr renderer = createRenderer(makeUniqueRefFromNonNullUniquePtr(WTF::move(sharedTimebase)));
    ASSERT(renderer);
    if (!renderer) {
        completionHandler(std::nullopt);
        return;
    }

    RendererContext context {
        .renderer = renderer.releaseNonNull(),
        .mediaElementIdentifier = mediaElementIdentifier,
        .playerIdentifier = playerIdentifier,
    };

    context.renderer->notifyWhenErrorOccurs([weakThis = ThreadSafeWeakPtr { *this }, identifier](PlatformMediaError error) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::ErrorOccurred(error));
    });

    context.renderer->notifyFirstFrameAvailable([weakThis = ThreadSafeWeakPtr { *this }, identifier] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_renderers.contains(identifier))
            return;
#if PLATFORM(COCOA)
        protectedThis->contextFor(identifier).layerHostingContextManager.setVideoLayerSizeIfPossible();
#endif
        protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::FirstFrameAvailable(protectedThis->stateFor(identifier)));
    });

    context.renderer->notifyWhenRequiresFlushToResume([weakThis = ThreadSafeWeakPtr { *this }, identifier] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::RequiresFlushToResume(protectedThis->stateFor(identifier)));
    });

    context.renderer->notifyRenderingModeChanged([weakThis = ThreadSafeWeakPtr { *this }, identifier] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_renderers.contains(identifier))
            protectedThis->rendereringModeChanged(identifier);
    });

    context.renderer->notifySizeChanged([weakThis = ThreadSafeWeakPtr { *this }, identifier](const MediaTime& time, FloatSize size) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::SizeChanged(time, size, protectedThis->stateFor(identifier)));
    });

    context.renderer->notifyEffectiveRateChanged([weakThis = ThreadSafeWeakPtr { *this }, identifier](double) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::EffectiveRateChanged(protectedThis->stateFor(identifier)));
    });

#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr videoTarget = m_gpuConnectionToWebProcess.get()->videoReceiverEndpointManager().takeVideoTargetForMediaElementIdentifier(mediaElementIdentifier, playerIdentifier);
    context.renderer->setVideoTarget(videoTarget.get());
#endif

    m_renderers.set(identifier, WTF::move(context));

    publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(stateFor(identifier)));
    completionHandler(WTF::move(handle));
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
    if (auto trackIdentifier = renderer->addTrack(type)) {
        renderer->notifyTrackNeedsReenqueuing(*trackIdentifier, [weakThis = ThreadSafeWeakPtr { *this }, identifier](TrackIdentifier trackIdentifier, const MediaTime& time) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::TrackNeedsReenqueuing(trackIdentifier, time, protectedThis->stateFor(identifier)));
        });

        completionHandler(*trackIdentifier);
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::NotSupportedError));
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
    renderer->requestMediaDataWhenReady(trackIdentifier)->whenSettled(RunLoop::mainSingleton(), [identifier, trackIdentifier, weakThis = ThreadSafeWeakPtr { *this }](auto result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return;
        protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::ReadyForMoreMediaData(trackIdentifier));
    });
}

MediaSampleConverter& RemoteAudioVideoRendererProxyManager::converterFor(RendererContext& context, TrackIdentifier trackIdentifier)
{
    return context.converters.ensure(trackIdentifier, [] {
        return WebCore::MediaSampleConverter { };
    }).iterator->value;
}

void RemoteAudioVideoRendererProxyManager::newTrackInfoForTrack(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier, Ref<WebCore::TrackInfo>&& info)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());

    MESSAGE_CHECK(m_renderers.contains(identifier));
    converterFor(contextFor(identifier), trackIdentifier).setTrackInfo(WTF::move(info));
}

void RemoteAudioVideoRendererProxyManager::enqueueSample(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier, WebCore::MediaSamplesBlock&& samplesBlock, std::optional<MediaTime> minimumPresentationTime, CompletionHandler<void(bool)>&& completionHandler)
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK_COMPLETION(iterator != m_renderers.end(), completionHandler(false));

    auto& converter = converterFor(iterator->value, trackIdentifier);
    MESSAGE_CHECK_COMPLETION(!!converter.currentTrackInfo(), completionHandler(false));
    if (RefPtr mediaSample = converter.convert(WTF::move(samplesBlock))) {
        iterator->value.renderer->enqueueSample(trackIdentifier, mediaSample.releaseNonNull(), minimumPresentationTime);
        completionHandler(iterator->value.renderer->isReadyForMoreSamples(trackIdentifier));
        return;
    }
    completionHandler(false);
}

void RemoteAudioVideoRendererProxyManager::notifyTimeReachedAndStall(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time, CompletionHandler<void(WebCore::MediaTimePromise::Result&&)>&& completionHandler)
{
    if (RefPtr renderer = rendererFor(identifier)) {
        renderer->notifyTimeReachedAndStall(time)->whenSettled(RunLoop::mainSingleton(), WTF::move(completionHandler));
        return;
    }
    completionHandler(makeUnexpected(PlatformMediaError::NotSupportedError));
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
    renderer->performTaskAtTime(time, [weakThis = ThreadSafeWeakPtr { *this }, time, identifier](auto) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::TaskTimeReached(time, protectedThis->stateFor(identifier)));
    });
}

void RemoteAudioVideoRendererProxyManager::flush(RemoteAudioVideoRendererIdentifier identifier)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->flush();
}

void RemoteAudioVideoRendererProxyManager::flushTrack(RemoteAudioVideoRendererIdentifier identifier, TrackIdentifier trackIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(false);
        return;
    }
    renderer->flushTrack(trackIdentifier);
    completionHandler(renderer->isReadyForMoreSamples(trackIdentifier));
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
    renderer->notifyWhenErrorOccurs([completionHandler = WTF::move(completionHandler)](auto error) mutable {
        completionHandler(error);
    });
}

void RemoteAudioVideoRendererProxyManager::installTimeObserver(RemoteAudioVideoRendererIdentifier identifier, Seconds interval)
{
    auto iterator = m_renderers.find(identifier);
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;
    context.renderer->setTimeObserver(interval, [weakThis = ThreadSafeWeakPtr { *this }, identifier](const MediaTime&) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!protectedThis->m_renderers.contains(identifier))
            return;
        protectedThis->maybeUpdateCachedVideoMetrics(identifier);
    });
}

// SynchronizerInterface
void RemoteAudioVideoRendererProxyManager::play(RemoteAudioVideoRendererIdentifier identifier, std::optional<MonotonicTime> hostTime)
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK(iterator != m_renderers.end());
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;

    context.renderer->play(hostTime);
}

void RemoteAudioVideoRendererProxyManager::pause(RemoteAudioVideoRendererIdentifier identifier, std::optional<MonotonicTime> hostTime)
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK(iterator != m_renderers.end());
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;

    context.renderer->pause(hostTime);
}

void RemoteAudioVideoRendererProxyManager::setRate(RemoteAudioVideoRendererIdentifier identifier, double rate)
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK(iterator != m_renderers.end());
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;

    context.renderer->setRate(rate);
}

void RemoteAudioVideoRendererProxyManager::stall(RemoteAudioVideoRendererIdentifier identifier)
{
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK(iterator != m_renderers.end());
    auto& context = iterator->value;

    context.renderer->stall();
}

void RemoteAudioVideoRendererProxyManager::prepareToSeek(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& seekTime, CompletionHandler<void(WebCore::MediaTimePromise::Result&&)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(makeUnexpected(PlatformMediaError::NotSupportedError));
        return;
    }
    renderer->prepareToSeek(seekTime)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, identifier, completionHandler = WTF::move(completionHandler)](auto&& result) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(protectedThis->stateFor(identifier)));
        completionHandler(WTF::move(result));
    });
}

void RemoteAudioVideoRendererProxyManager::finishSeek(RemoteAudioVideoRendererIdentifier identifier, const MediaTime& time, CompletionHandler<void(GenericPromise::Result&&)>&& completionHandler)
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(makeUnexpected(GenericPromise::RejectValueType { }));
        return;
    }
    renderer->finishSeek(time)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, identifier, completionHandler = WTF::move(completionHandler)](auto&& result) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::StateUpdate(protectedThis->stateFor(identifier)));
        completionHandler(WTF::move(result));
    });
}

void RemoteAudioVideoRendererProxyManager::setScreenReserved(RemoteAudioVideoRendererIdentifier identifier, bool reserved)
{
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setScreenReserved(reserved);
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
    contextFor(identifier).isGatheringVideoFrameMetadata = notify;
    if (!notify) {
        renderer->notifyWhenHasAvailableVideoFrame({ });
        return;
    }
    renderer->notifyWhenHasAvailableVideoFrame([weakThis = ThreadSafeWeakPtr { *this }, identifier](auto presentationTime, auto clockTime) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_renderers.contains(identifier))
            return;
        // Per-frame consumers (e.g. requestVideoFrameCallback) need the metrics
        // for this exact frame. Fetch them and ship them with the IPC so the
        // receiver doesn't have to read a stale shared cache. This path is
        // independent of the cadence-based metrics push.
        auto metrics = protectedThis->contextFor(identifier).renderer->videoPlaybackQualityMetrics();
        protectedThis->publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::HasAvailableVideoFrame(presentationTime, clockTime, protectedThis->stateFor(identifier), WTF::move(metrics)));
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

void RemoteAudioVideoRendererProxyManager::setVideoPlaybackMetricsUpdateInterval(RemoteAudioVideoRendererIdentifier identifier, double interval)
{
    DEBUG_LOG(LOGIDENTIFIER, identifier.loggingString(), " interval=", interval, "s");
    auto iterator = m_renderers.find(identifier);
    MESSAGE_CHECK(iterator != m_renderers.end());
    auto& context = iterator->value;

    static const Seconds metricsAdvanceUpdate = 0.25_s;
    updateCachedVideoMetrics(identifier);
    context.videoPlaybackMetricsUpdateInterval = Seconds(interval);
    context.nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + Seconds(interval) - metricsAdvanceUpdate;

    // The metrics push is the only consumer of the periodic time observer; install it on
    // demand and tear it down when no longer needed.
    if (interval > 0)
        installTimeObserver(identifier, metricsAdvanceUpdate);
    else
        context.renderer->setTimeObserver(0_s, { });
}

void RemoteAudioVideoRendererProxyManager::maybeUpdateCachedVideoMetrics(RemoteAudioVideoRendererIdentifier identifier)
{
    auto iterator = m_renderers.find(identifier);
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;
    // While rVFC is gathering metadata the WebContent cache is refreshed per
    // frame via HasAvailableVideoFrame; the cadence-based push would just
    // duplicate work, so skip it.
    if (context.isGatheringVideoFrameMetadata)
        return;
    if (context.renderer->paused() || !context.videoPlaybackMetricsUpdateInterval || MonotonicTime::now() < context.nextPlaybackQualityMetricsUpdateTime)
        return;
    updateCachedVideoMetrics(identifier);
}

void RemoteAudioVideoRendererProxyManager::updateCachedVideoMetrics(RemoteAudioVideoRendererIdentifier identifier)
{
    auto iterator = m_renderers.find(identifier);
    if (iterator == m_renderers.end())
        return;
    auto& context = iterator->value;
    context.nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + context.videoPlaybackMetricsUpdateInterval;
    auto metrics = context.renderer->videoPlaybackQualityMetrics();
    if (!metrics) {
        DEBUG_LOG(LOGIDENTIFIER, identifier.loggingString(), " no metrics available");
        return;
    }
    DEBUG_LOG(LOGIDENTIFIER, identifier.loggingString(), " total=", metrics->totalVideoFrames, " dropped=", metrics->droppedVideoFrames, " corrupted=", metrics->corruptedVideoFrames, " displayComposited=", metrics->displayCompositedVideoFrames, " frameDelay=", metrics->totalFrameDelay);
    publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::UpdatePlaybackQualityMetrics(WTF::move(*metrics)));
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
    completionHandler(WTF::move(result));
}

void RemoteAudioVideoRendererProxyManager::currentBitmapImage(RemoteAudioVideoRendererIdentifier identifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler) const
{
    RefPtr renderer = rendererFor(identifier);
    if (!renderer) {
        completionHandler(std::nullopt);
        return;
    }
    renderer->currentBitmapImage()->whenSettled(RunLoop::mainSingleton(), [completionHandler = WTF::move(completionHandler)](auto&& result) mutable {
        if (!result) {
            completionHandler(std::nullopt);
            return;
        }
        completionHandler((*result)->createHandle());
    });
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
    auto iterator = m_renderers.find(identifier);
    if (iterator == m_renderers.end())
        return { };
    return {
        .paused = iterator->value.renderer->paused(),
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
        publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::LayerHostingContextChanged(state, *maybeHostingContext, context.layerHostingContextManager.videoLayerSize()));
#endif
    publishAndSend(identifier, Messages::AudioVideoRendererRemoteMessageReceiver::RenderingModeChanged(state));
}

#if PLATFORM(COCOA)
void RemoteAudioVideoRendererProxyManager::setVideoLayerSize(RemoteAudioVideoRendererIdentifier identifier, const WebCore::FloatSize& size)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString(), ", size: ", size.width(), "x", size.height());

    MESSAGE_CHECK(m_renderers.contains(identifier));

    auto& context = contextFor(identifier);
    context.layerHostingContextManager.setVideoLayerSize(size);
}

void RemoteAudioVideoRendererProxyManager::setVideoLayerSizeFenced(RemoteAudioVideoRendererIdentifier identifier, const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString(), size.width(), "x", size.height());

    MESSAGE_CHECK(m_renderers.contains(identifier));

    auto& context = contextFor(identifier);
    context.layerHostingContextManager.setVideoLayerSizeFenced(size, WTF::MachSendRightAnnotated { sendRightAnnotated }, [&] {
        context.renderer->setVideoLayerSizeFenced(size, WTF::move(sendRightAnnotated));
    });
}
#endif

void RemoteAudioVideoRendererProxyManager::requestHostingContext(RemoteAudioVideoRendererIdentifier identifier, LayerHostingContextCallback&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());
#if PLATFORM(COCOA)
    MESSAGE_CHECK_COMPLETION(m_renderers.contains(identifier), completionHandler({ }));
    contextFor(identifier).layerHostingContextManager.requestHostingContext(WTF::move(completionHandler));
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
    if (RefPtr cdmSession = m_gpuConnectionToWebProcess.get()->legacyCdmFactoryProxy().getSession(*instanceId))
        renderer->setCDMSession(protect(cdmSession->session()).get());
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
    if (RefPtr instanceProxy = protect(m_gpuConnectionToWebProcess.get()->cdmFactoryProxy())->getInstance(*instanceId))
        renderer->setCDMInstance(&instanceProxy->instance());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "Unable to find CDMInstance: ", instanceId->loggingString());
}

void RemoteAudioVideoRendererProxyManager::setInitData(RemoteAudioVideoRendererIdentifier identifier, Ref<WebCore::SharedBuffer> initData, CompletionHandler<void(Expected<void, WebCore::PlatformMediaError>)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, identifier.loggingString());
    if (RefPtr renderer = rendererFor(identifier))
        renderer->setInitData(initData)->whenSettled(RunLoop::mainSingleton(), WTF::move(completionHandler));
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

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION

#endif
