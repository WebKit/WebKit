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
#include "AudioVideoRendererRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "AudioVideoRendererRemoteMessageReceiverMessages.h"
#include "Logging.h"
#include "RemoteAudioVideoRendererIdentifier.h"
#include "RemoteAudioVideoRendererProxyManagerMessages.h"
#include "RemoteCDMInstance.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMSession.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include "RemoteVideoFrameProxyProperties.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/HostingContext.h>
#include <WebCore/LegacyCDM.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaSamplesBlock.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

#if PLATFORM(COCOA)
#include <WebCore/PixelBufferConformerCV.h>
#include <WebCore/VideoFrameCV.h>
#include <WebCore/VideoLayerManagerObjC.h>
#include <wtf/MachSendRightAnnotated.h>
#endif

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(AudioVideoRendererRemote::TrackState);

WorkQueue& AudioVideoRendererRemote::queueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("AudioVideoRendererRemote"_s);
    return workQueue.get();
}

Ref<AudioVideoRendererRemote> AudioVideoRendererRemote::create(LoggerHelper* loggerHelper, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier, VideoRendererPreferences preferences, GPUProcessConnection& connection)
{
    assertIsMainThread();

    auto identifier = RemoteAudioVideoRendererIdentifier::generate();
    return adoptRef(*new AudioVideoRendererRemote(loggerHelper, connection, mediaElementIdentifier, playerIdentifier, preferences, identifier));
}

AudioVideoRendererRemote::AudioVideoRendererRemote(LoggerHelper* loggerHelper, GPUProcessConnection& connection, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier, VideoRendererPreferences preferences, RemoteAudioVideoRendererIdentifier identifier)
    : m_gpuProcessConnection(connection)
    , m_receiver(MessageReceiver::create(*this))
    , m_identifier(identifier)
    , m_stallRequest(NativePromiseRequest::create())
    , m_decoderInContentProcess(preferences.contains(VideoRendererPreference::DecoderInContentProcess))
    , m_prepareSeekRequest(NativePromiseRequest::create())
    , m_finishSeekRequest(NativePromiseRequest::create())
#if PLATFORM(COCOA)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(loggerHelper->logger(), loggerHelper->logIdentifier()))
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(loggerHelper->logger())
    , m_logIdentifier(loggerHelper->logIdentifier())
#endif
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(loggerHelper);
#endif

    ALWAYS_LOG_WITH_THIS(this, LOGIDENTIFIER_WITH_THIS(this));

    connection.connection().addWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), queueSingleton(), m_receiver, m_identifier.toUInt64());
    connection.addClient(*this);

    connection.connection().sendWithAsyncReply(Messages::RemoteAudioVideoRendererProxyManager::CreateManager(identifier, mediaElementIdentifier, playerIdentifier), [weakThis = ThreadSafeWeakPtr { *this }](auto&& handle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!handle) {
            protectedThis->ensureOnDispatcher([protectedThis] {
                assertIsCurrent(queueSingleton());
                if (protectedThis->m_errorCallback)
                    protectedThis->m_errorCallback(PlatformMediaError::MemoryError);
            });
            return;
        }
        auto reader = SharedTimebaseReader::create(WTF::move(*handle));
        if (!reader) {
            protectedThis->ensureOnDispatcher([protectedThis] {
                assertIsCurrent(queueSingleton());
                if (protectedThis->m_errorCallback)
                    protectedThis->m_errorCallback(PlatformMediaError::MemoryError);
            });
            return;
        }
        Locker locker { protectedThis->m_lock };
        lazyInitialize(protectedThis->m_sharedTimebaseReader, WTF::move(reader));
    }, 0);
}

AudioVideoRendererRemote::~AudioVideoRendererRemote() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if PLATFORM(COCOA)
    m_videoLayerManager->didDestroyVideoLayer();
#endif

    ensureOnDispatcher([prepareSeekRequest = WTF::move(m_prepareSeekRequest), prepareSeekPromise = WTF::move(m_prepareSeekPromise), finishSeekRequest = WTF::move(m_finishSeekRequest), finishSeekPromise = WTF::move(m_finishSeekPromise), stallRequest = WTF::move(m_stallRequest), stallProducer = WTF::move(m_stallProducer), trackStates = WTF::move(m_trackStates)] mutable {
        if (prepareSeekRequest->hasCallback())
            prepareSeekRequest->disconnect();
        if (auto promise = std::exchange(prepareSeekPromise, std::nullopt))
            promise->reject(PlatformMediaError::Cancelled);
        if (finishSeekRequest->hasCallback())
            finishSeekRequest->disconnect();
        if (auto promise = std::exchange(finishSeekPromise, std::nullopt))
            promise->reject();
        if (stallRequest->hasCallback())
            stallRequest->disconnect();
        if (auto producer = std::exchange(stallProducer, std::nullopt))
            producer->reject(PlatformMediaError::Cancelled);
        for (auto& state : trackStates.values())
            flushDecoders(state);
        trackStates.clear();
    });

    if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get(); gpuProcessConnection && !m_gpuProcessConnectionShutDown) {
        ensureOnDispatcher([gpuProcessConnection, identifier = m_identifier] {
            gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Destroy(identifier), 0);
            gpuProcessConnection->connection().removeWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), identifier.toUInt64());
        });
    }

}

void AudioVideoRendererRemote::shutdown()
{
    if (m_isShutdown.exchange(true))
        return;
    ALWAYS_LOG(LOGIDENTIFIER);
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->releaseAllDecoders();
        RefPtr gpuProcessConnection = protectedThis->m_gpuProcessConnection.get();
        if (!protectedThis->isGPURunning() || !gpuProcessConnection)
            return;
        gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Shutdown(protectedThis->m_identifier), 0);
    });
}

void AudioVideoRendererRemote::setVolume(float volume)
{
    ensureOnDispatcherWithConnection([volume](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetVolume(renderer.m_identifier, volume), 0);
    });
}

void AudioVideoRendererRemote::setMuted(bool muted)
{
    ensureOnDispatcherWithConnection([muted](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetMuted(renderer.m_identifier, muted), 0);
    });
}

void AudioVideoRendererRemote::setPreservesPitchAndCorrectionAlgorithm(bool preservesPitch, std::optional<PitchCorrectionAlgorithm> algorithm)
{
    ensureOnDispatcherWithConnection([preservesPitch, algorithm = WTF::move(algorithm)](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetPreservesPitchAndCorrectionAlgorithm(renderer.m_identifier, preservesPitch, algorithm), 0);
    });
}

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
void AudioVideoRendererRemote::setOutputDeviceId(const String& deviceId)
{
    ensureOnDispatcherWithConnection([deviceId = deviceId.isolatedCopy()](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetOutputDeviceId(renderer.m_identifier, deviceId), 0);
    });
}
#endif

void AudioVideoRendererRemote::setIsVisible(bool visible)
{
    ensureOnDispatcherWithConnection([visible](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetIsVisible(renderer.m_identifier, visible), 0);
    });
}

void AudioVideoRendererRemote::setPresentationSize(const IntSize& size)
{
    ensureOnDispatcherWithConnection([size](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetPresentationSize(renderer.m_identifier, size), 0);
    });
}

void AudioVideoRendererRemote::setShouldMaintainAspectRatio(bool maintain)
{
    ensureOnDispatcherWithConnection([maintain](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetShouldMaintainAspectRatio(renderer.m_identifier, maintain), 0);
    });
}

void AudioVideoRendererRemote::renderingCanBeAcceleratedChanged(bool acceleratedRendering)
{
    ensureOnDispatcherWithConnection([acceleratedRendering](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::RenderingCanBeAcceleratedChanged(renderer.m_identifier, acceleratedRendering), 0);
    });
}

void AudioVideoRendererRemote::contentBoxRectChanged(const LayoutRect& rect)
{
    ensureOnDispatcherWithConnection([rect](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::ContentBoxRectChanged(renderer.m_identifier, rect), 0);
    });
}

void AudioVideoRendererRemote::notifyFirstFrameAvailable(Function<void()>&& callback)
{
    ensureOnDispatcherWithConnection([callback = WTF::move(callback)](auto& renderer, auto&) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_firstFrameAvailableCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&& callback)
{
    ensureOnDispatcherWithConnection([callback = WTF::move(callback)](auto& renderer, auto& connection) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_hasAvailableVideoFrameCallback = WTF::move(callback);
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::NotifyWhenHasAvailableVideoFrame(renderer.m_identifier, !!renderer.m_hasAvailableVideoFrameCallback), 0);
    });
}

void AudioVideoRendererRemote::notifyWhenRequiresFlushToResume(Function<void()>&& callback)
{
    ensureOnDispatcherWithConnection([callback = WTF::move(callback)](auto& renderer, auto&) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_notifyWhenRequiresFlushToResumeCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::notifyRenderingModeChanged(Function<void()>&& callback)
{
    ensureOnDispatcherWithConnection([callback = WTF::move(callback)](auto& renderer, auto&) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_renderingModeChangedCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::expectMinimumUpcomingPresentationTime(const MediaTime& minimum)
{
    ensureOnDispatcherWithConnection([minimum](auto& renderer, auto& connection) {
        assertIsCurrent(queueSingleton());
        bool hasVideoTrackState = false;
        for (auto& state : renderer.m_trackStates.values()) {
            if (!state->isVideo)
                continue;
            hasVideoTrackState = true;
            state->minimumUpcomingTime = minimum;
            renderer.deliverMinimumUpcomingTime(state);
        }
        if (!hasVideoTrackState)
            connection.send(Messages::RemoteAudioVideoRendererProxyManager::ExpectMinimumUpcomingPresentationTime(renderer.m_identifier, minimum), 0);
    });
}

void AudioVideoRendererRemote::notifySizeChanged(Function<void(const MediaTime&, FloatSize)>&& callback)
{
    ensureOnDispatcherWithConnection([callback = WTF::move(callback)](auto& renderer, auto&) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_sizeChangedCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::setShouldDisableHDR(bool disable)
{
    ensureOnDispatcherWithConnection([disable](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetShouldDisableHDR(renderer.m_identifier, disable), 0);
    });
}

void AudioVideoRendererRemote::setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit& limit)
{
    ensureOnDispatcherWithConnection([limit](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetPlatformDynamicRangeLimit(renderer.m_identifier, limit), 0);
    });
}

void AudioVideoRendererRemote::setResourceOwner(const ProcessIdentity& processIdentity)
{
    ensureOnDispatcherWithConnection([processIdentity = ProcessIdentity { processIdentity }](auto& renderer, auto& connection) mutable {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetResourceOwner(renderer.m_identifier, WTF::move(processIdentity)), 0);
    });
}

void AudioVideoRendererRemote::flushAndRemoveImage()
{
    ensureOnDispatcherWithConnection([](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::FlushAndRemoveImage(renderer.m_identifier), 0);
    });
}

RefPtr<VideoFrame> AudioVideoRendererRemote::currentVideoFrame() const
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return nullptr;

    RefPtr<VideoFrame> videoFrame;
    callOnMainRunLoopAndWait([&] {
        if (m_isShutdown)
            return;
        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteAudioVideoRendererProxyManager::CurrentVideoFrame(m_identifier), 0);
        if (!sendResult.succeeded())
            return;

        auto [result] = sendResult.takeReply();
        if (!result)
            return;
        videoFrame = RemoteVideoFrameProxy::create(gpuProcessConnection->connection(), protect(gpuProcessConnection->videoFrameObjectHeapProxy()), WTF::move(*result));
    });
    return videoFrame;
}

void AudioVideoRendererRemote::paintCurrentVideoFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    if (RefPtr videoFrame = currentVideoFrame())
        context.drawVideoFrame(*videoFrame, rect, ShouldDiscardAlpha::No);
}

RefPtr<NativeImage> AudioVideoRendererRemote::currentNativeImage() const
{
#if PLATFORM(COCOA)
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    RefPtr videoFrame = currentVideoFrame();
    if (!videoFrame)
        return nullptr;
    ASSERT(gpuProcessConnection);

    return protect(gpuProcessConnection->videoFrameObjectHeapProxy())->getNativeImage(*videoFrame);
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

Ref<AudioVideoRenderer::BitmapImagePromise> AudioVideoRendererRemote::currentBitmapImage() const
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (m_isShutdown || !isGPURunning() || !gpuProcessConnection)
        return BitmapImagePromise::createAndReject();

    return gpuProcessConnection->connection().sendWithPromisedReply(Messages::RemoteAudioVideoRendererProxyManager::CurrentBitmapImage(m_identifier))->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) -> Ref<BitmapImagePromise> {
        RefPtr protectedThis = weakThis.get();
        if (!result || !result.value() || !protectedThis)
            return BitmapImagePromise::createAndReject();

        if (auto bitmap = ShareableBitmap::create(WTF::move(**result)))
            return BitmapImagePromise::createAndResolve(bitmap.releaseNonNull());
        return BitmapImagePromise::createAndReject();
    });
}

std::optional<VideoPlaybackQualityMetrics> AudioVideoRendererRemote::videoPlaybackQualityMetrics()
{
    constexpr Seconds maximumPlaybackQualityMetricsSampleTimeDelta = 250_ms;
    constexpr Seconds minimumPlaybackQualityMetricsUpdateInterval = 250_ms;

    std::optional<Seconds> newInterval;
    std::optional<VideoPlaybackQualityMetrics> cached;
    Seconds timeSinceLastQuery;
    Seconds currentInterval;
    {
        Locker locker { m_lock };
        auto now = MonotonicTime::now();
        timeSinceLastQuery = now - m_lastPlaybackQualityMetricsQueryTime;
        if (!m_videoPlaybackMetricsUpdateInterval)
            newInterval = 1_s;
        else if (std::abs((timeSinceLastQuery - m_videoPlaybackMetricsUpdateInterval).value()) > maximumPlaybackQualityMetricsSampleTimeDelta.value())
            newInterval = std::max(timeSinceLastQuery, minimumPlaybackQualityMetricsUpdateInterval);
        m_lastPlaybackQualityMetricsQueryTime = now;
        if (newInterval)
            m_videoPlaybackMetricsUpdateInterval = *newInterval;
        currentInterval = m_videoPlaybackMetricsUpdateInterval;
        cached = m_cachedState.videoPlaybackQualityMetrics;
    }
    DEBUG_LOG(LOGIDENTIFIER, "timeSinceLastQuery=", timeSinceLastQuery.value(), "s interval=", currentInterval.value(), "s", newInterval ? " (interval updated)" : "", cached ? " cached=yes" : " cached=no");
    if (newInterval)
        updateVideoPlaybackMetricsUpdateInterval(*newInterval);
    return cached;
}

void AudioVideoRendererRemote::updateVideoPlaybackMetricsUpdateInterval(const Seconds& interval)
{
    DEBUG_LOG(LOGIDENTIFIER, "interval=", interval.value(), "s");
    ensureOnDispatcherWithConnection([identifier = m_identifier, interval](AudioVideoRendererRemote&, IPC::Connection& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetVideoPlaybackMetricsUpdateInterval(identifier, interval.value()), 0);
    });
}

PlatformLayer* AudioVideoRendererRemote::platformVideoLayer() const
{
#if PLATFORM(COCOA)
    Locker locker { m_lock };
    if (!m_videoLayer && m_layerHostingContext.contextID) {
        auto expandedVideoLayerSize = expandedIntSize(videoLayerSize());
        m_videoLayer = createVideoLayerRemote(const_cast<AudioVideoRendererRemote&>(*this), m_layerHostingContext.contextID, WebCore::MediaPlayer::VideoGravity::ResizeAspect, expandedVideoLayerSize);
        m_videoLayerManager->setVideoLayer(m_videoLayer.get(), expandedVideoLayerSize);
    }
    return m_videoLayerManager->videoInlineLayer();
#else
    return nullptr;
#endif
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
void AudioVideoRendererRemote::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    if (m_isShutdown) {
        completionHandler();
        return;
    }
#if PLATFORM(COCOA)
    Locker locker { m_lock };
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTF::move(completionHandler), nullptr);
#endif
}

void AudioVideoRendererRemote::setVideoFullscreenFrame(const FloatRect& frame)
{
    ensureOnDispatcherWithConnection([frame](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetVideoFullscreenFrame(renderer.m_identifier, frame), 0);
    });
}

void AudioVideoRendererRemote::isInFullscreenOrPictureInPictureChanged(bool inFullscreen)
{
    ensureOnDispatcherWithConnection([inFullscreen](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::IsInFullscreenOrPictureInPictureChanged(renderer.m_identifier, inFullscreen), 0);
    });
}
#endif

void AudioVideoRendererRemote::play(std::optional<MonotonicTime> hostTime)
{
    {
        Locker locker { m_lock };
        m_cachedState.paused = false;
    }
    ensureOnDispatcherWithConnection([hostTime](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::Play(renderer.m_identifier, hostTime), 0);
    });
}

void AudioVideoRendererRemote::pause(std::optional<MonotonicTime> hostTime)
{
    {
        Locker locker { m_lock };
        m_cachedState.paused = true;
    }
    ensureOnDispatcherWithConnection([hostTime](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::Pause(renderer.m_identifier, hostTime), 0);
    });
}

bool AudioVideoRendererRemote::paused() const
{
    Locker locker { m_lock };
    return m_cachedState.paused;
}

void AudioVideoRendererRemote::setRate(double rate)
{
    ensureOnDispatcherWithConnection([rate](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetRate(renderer.m_identifier, rate), 0);
    });
}

double AudioVideoRendererRemote::effectiveRate() const
{
    Locker locker { m_lock };
    return m_sharedTimebaseReader ? m_sharedTimebaseReader->currentRate() : 0.0;
}

void AudioVideoRendererRemote::stall()
{
    ensureOnDispatcherWithConnection([](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::Stall(renderer.m_identifier), 0);
    });
}

void AudioVideoRendererRemote::cancelPendingSeek()
{
    assertIsCurrent(queueSingleton());

    if (m_prepareSeekRequest->hasCallback())
        protect(m_prepareSeekRequest)->disconnect();
    if (auto promise = std::exchange(m_prepareSeekPromise, std::nullopt))
        promise->reject(PlatformMediaError::Cancelled);
    if (m_finishSeekRequest->hasCallback())
        protect(m_finishSeekRequest)->disconnect();
    if (auto promise = std::exchange(m_finishSeekPromise, std::nullopt))
        promise->reject();
}

Ref<MediaTimePromise> AudioVideoRendererRemote::prepareToSeek(const MediaTime& time)
{
    {
        Locker locker { m_lock };
        if (m_sharedTimebaseReader)
            m_sharedTimebaseReader->resetForTimeDiscontinuity();
        // A seek that crosses a previously-installed stall cap (forward past it
        // under positive rate, backward past it under negative) leaves the cap
        // describing a position now behind the playhead. Drop it; the player
        // computes a new cap via resetStallForTime() if needed.
        if (m_stallCap) {
            double rate = m_sharedTimebaseReader ? m_sharedTimebaseReader->currentRate() : 1.0;
            if (rate >= 0 ? *m_stallCap < time : *m_stallCap > time)
                m_stallCap.reset();
        }
        m_lastSeekTime = time;
    }
    m_seeking = true;
    return invokeAsync(queueSingleton(), [protectedThis = Ref { *this }, this, time] -> Ref<MediaTimePromise> {
        cancelPendingSeek();

        if (m_isShutdown) {
            m_seeking = false;
            return MediaTimePromise::createAndReject(PlatformMediaError::Shutdown);
        }

        RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection) {
            m_seeking = false;
            return MediaTimePromise::createAndReject(PlatformMediaError::IPCError);
        }

        assertIsCurrent(queueSingleton());
        m_prepareSeekPromise.emplace();
        Ref promise = m_prepareSeekPromise->promise();

        gpuProcessConnection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteAudioVideoRendererProxyManager::PrepareToSeek(m_identifier, time), 0)->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            assertIsCurrent(queueSingleton());
            protect(protectedThis->m_prepareSeekRequest)->complete();

            if (result && !result->isIndefinite())
                protectedThis->m_seeking = false;

            protectedThis->notifyDecodersOfTimeChange();
            if (auto producer = std::exchange(protectedThis->m_prepareSeekPromise, std::nullopt))
                producer->settle(WTF::move(result));
        })->track(protect(m_prepareSeekRequest));

        return promise;
    });
}

Ref<GenericPromise> AudioVideoRendererRemote::finishSeek(const MediaTime& time)
{
    // m_seeking is set/read on MediaSourcePrivateAVFObjC::queueSingleton() but
    // cleared on AudioVideoRendererRemote::queueSingleton(), so a superseding
    // seek (or a GPU-not-running path) can clear it out from under this call.
    // Treat that as a superseded seek and reject.
    if (!m_seeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "seek superseded before finishSeek; skipping");
        return GenericPromise::createAndReject();
    }
    return invokeAsync(queueSingleton(), [protectedThis = Ref { *this }, this, time] -> Ref<GenericPromise> {
        cancelPendingSeek();

        RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
        if (m_isShutdown || !isGPURunning() || !gpuProcessConnection) {
            m_seeking = false;
            return GenericPromise::createAndReject();
        }

        assertIsCurrent(queueSingleton());
        m_finishSeekPromise.emplace();
        Ref promise = m_finishSeekPromise->promise();

        gpuProcessConnection->connection().sendWithPromisedReply<WTF::GenericPromiseConverter>(Messages::RemoteAudioVideoRendererProxyManager::FinishSeek(m_identifier, time), 0)->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            assertIsCurrent(queueSingleton());
            protect(protectedThis->m_finishSeekRequest)->complete();

            protectedThis->m_seeking = false;

            protectedThis->notifyDecodersOfTimeChange();
            if (auto producer = std::exchange(protectedThis->m_finishSeekPromise, std::nullopt))
                producer->settle(WTF::move(result));
        })->track(protect(m_finishSeekRequest));

        return promise;
    });
}

bool AudioVideoRendererRemote::seeking() const
{
    return m_seeking;
}

void AudioVideoRendererRemote::setScreenReserved(bool reserved)
{
    ensureOnDispatcherWithConnection([reserved](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetScreenReserved(renderer.m_identifier, reserved), 0);
    });
}

void AudioVideoRendererRemote::setPreferences(VideoRendererPreferences preferences)
{
    ensureOnDispatcherWithConnection([preferences](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetPreferences(renderer.m_identifier, preferences), 0);
    });
}

void AudioVideoRendererRemote::setHasProtectedVideoContent(bool isProtected)
{
    ensureOnDispatcherWithConnection([isProtected](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetHasProtectedVideoContent(renderer.m_identifier, isProtected), 0);
    });
}

std::optional<AudioVideoRendererRemote::TrackIdentifier> AudioVideoRendererRemote::addTrack(TrackType type)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return std::nullopt;

    // the sendSync() call requires us to run on the connection's dispatcher, which is the main thread.
    std::expected<WebCore::SamplesRendererTrackIdentifier, WebCore::PlatformMediaError> result = makeUnexpected(PlatformMediaError::IPCError);
    callOnMainRunLoopAndWait([&] {
        if (m_isShutdown)
            return;
        // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteAudioVideoRendererProxyManager::AddTrack(m_identifier, type), 0);
        if (!sendResult.succeeded())
            return;
        result = std::get<0>(sendResult.takeReply());
    });
    if (!result)
        return std::nullopt;
    return *result;
}

void AudioVideoRendererRemote::removeTrack(TrackIdentifier trackIdentifier)
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier] {
        if (RefPtr protectedThis { weakThis })
            protectedThis->releaseTrackState(trackIdentifier);
    });
    ensureOnDispatcherWithConnection([trackIdentifier](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::RemoveTrack(renderer.m_identifier, trackIdentifier), 0);
    });
}

void AudioVideoRendererRemote::enqueueSample(TrackIdentifier trackIdentifier, Ref<MediaSample>&& sample, std::optional<MediaTime> expectedMinimum)
{
    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).sampleEnqueued();
    }
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, sample = WTF::move(sample), expectedMinimum] mutable {
        RefPtr protectedThis { weakThis };
        if (!protectedThis || protectedThis->m_isShutdown || !protectedThis->isGPURunning())
            return;
        protectedThis->processSample(trackIdentifier, WTF::move(sample), expectedMinimum);
    });
}

AudioVideoRendererRemote::TrackState& AudioVideoRendererRemote::trackState(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    return m_trackStates.ensure(trackIdentifier, [] {
        return makeUniqueRef<TrackState>();
    }).iterator->value.get();
}

void AudioVideoRendererRemote::flushDecoders(TrackState& state)
{
    if (RefPtr decoder = std::exchange(state.decoder, nullptr))
        decoder->flush();
    for (auto& draining : std::exchange(state.drainingDecoders, { }))
        protect(draining.decoder)->flush();
    state.held.clear();
}

RefPtr<GPUProcessConnection> AudioVideoRendererRemote::activeConnection()
{
    if (m_isShutdown || !isGPURunning())
        return nullptr;
    return m_gpuProcessConnection.get();
}

void AudioVideoRendererRemote::processSample(TrackIdentifier trackIdentifier, Ref<MediaSample>&& sample, std::optional<MediaTime> expectedMinimum)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    if (state.mode == TrackState::Mode::Failed || state.awaitingFlush || (state.awaitingKeyframe && !sample->isSync())) {
        decrementPendingSamples(trackIdentifier);
        return;
    }
    state.awaitingKeyframe = false;

    auto& converter = m_mediaSampleConverters.ensure(trackIdentifier, [] {
        return MediaSampleConverter();
    }).iterator->value;
    auto converted = converter.convert(sample, MediaSampleConverter::SetTrackInfo::Yes);
    RefPtr info = converter.currentTrackInfo();
    state.isVideo = info->isVideo();

    if (info->isVideo()) {
        if (m_keyframeNeeded && !sample->isSync()) {
            ALWAYS_LOG(LOGIDENTIFIER, "Keyframe needed: but frame not keyframe");
            ASSERT_NOT_REACHED();
            decrementPendingSamples(trackIdentifier);
            return;
        }
        m_keyframeNeeded = false;
    }

    dispatchBlock(trackIdentifier, HeldBlock { WTF::move(converted.get()), expectedMinimum });
}

void AudioVideoRendererRemote::selectMode(TrackIdentifier trackIdentifier, const MediaSamplesBlock& block)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    Ref info = *block.info();
    bool isProtected = block.isProtected();
    state.selectedInfo = info.ptr();
    state.selectedProtected = isProtected;

    if (state.mode == TrackState::Mode::Decoding) {
        Ref decoder = *state.decoder;
        if (!isProtected && decoder->canContinueWith(info))
            return;
        state.decoder = nullptr;
        state.drainingDecoders.append(DrainingDecoder { decoder, false, std::exchange(state.held, { }) });
        state.mode = TrackState::Mode::Compressed;
        updateDecoderReadiness(trackIdentifier);
        ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), isProtected ? " content is protected" : " format changed", ", draining decoder");
        decoder->drain()->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, generation = state.generation, weakDecoder = ThreadSafeWeakPtr { decoder.get() }](auto&& result) {
            RefPtr protectedThis { weakThis };
            RefPtr protectedDecoder { weakDecoder };
            if (protectedThis && protectedDecoder)
                protectedThis->didDrainDecoder(trackIdentifier, generation, protectedDecoder.get(), WTF::move(result));
        });
    }
    beginMode(trackIdentifier, block);
}

static MediaSamplesDecoder::Filters contentProcessDecoderFilters()
{
    return { MediaSamplesDecoder::Filter::InProcess, MediaSamplesDecoder::Filter::SoftwareAllowed, MediaSamplesDecoder::Filter::UseSharedMemory };
}

void AudioVideoRendererRemote::beginMode(TrackIdentifier trackIdentifier, const MediaSamplesBlock& block)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    ASSERT(state.mode == TrackState::Mode::Compressed);
    state.waitingForSyncSample = false;
    if (!m_decoderInContentProcess)
        return;

    Ref info = *block.info();
    if (block.isProtected()) {
        ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " content is protected, compressed");
        return;
    }

    PacedMediaSamplesDecoder::Filters filters = contentProcessDecoderFilters();
    if (state.hardwareUnavailableCodecs.contains(info->codecName()))
        filters.add(MediaSamplesDecoder::Filter::ForceSoftware);
    if (!MediaSamplesDecoder::canDecode(info, filters)) {
        ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " cannot be decoded in the content process, compressed");
        return;
    }

    if (!block.isSync()) {
        state.waitingForSyncSample = true;
        ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " compressed until the next sync sample");
        return;
    }

    state.mode = TrackState::Mode::Creating;
    ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " creating decoder");
    PacedMediaSamplesDecoder::create(info, filters, queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis { weakThis };
        return protectedThis ? protectedThis->playbackPosition() : PacedMediaSamplesDecoder::PlaybackPosition { MediaTime::invalidTime(), 0 };
    })->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, creationID = ++state.creationID](auto&& result) {
        if (RefPtr protectedThis { weakThis })
            protectedThis->didCreateDecoder(trackIdentifier, creationID, WTF::move(result));
    });
}

void AudioVideoRendererRemote::didDrainDecoder(TrackIdentifier trackIdentifier, uint32_t generation, const PacedMediaSamplesDecoder* decoder, PacedMediaSamplesDecoder::DecodePromise::Result&& result)
{
    assertIsCurrent(queueSingleton());

    auto iterator = m_trackStates.find(trackIdentifier);
    if (iterator == m_trackStates.end() || iterator->value->generation != generation)
        return;
    auto& state = iterator->value.get();
    auto index = state.drainingDecoders.findIf([&](auto& draining) {
        return draining.decoder.ptr() == decoder;
    });
    if (index == notFound)
        return;

    if (!result) {
        if (result.error() == PlatformMediaError::Cancelled)
            return;
        decoderFailed(trackIdentifier, result.error());
        return;
    }

    auto& draining = state.drainingDecoders[index];
    for (auto& block : *result)
        draining.held.append({ WTF::move(block), std::nullopt, true });
    draining.isDrained = true;
    releaseDrainedOutput(trackIdentifier);
}

void AudioVideoRendererRemote::didCreateDecoder(TrackIdentifier trackIdentifier, uint32_t creationID, PacedMediaSamplesDecoder::CreatePromise::Result&& result)
{
    assertIsCurrent(queueSingleton());

    auto iterator = m_trackStates.find(trackIdentifier);
    if (iterator == m_trackStates.end())
        return;
    auto& state = iterator->value.get();
    if (state.mode != TrackState::Mode::Creating || state.creationID != creationID)
        return;

    if (!result) {
        decoderFailed(trackIdentifier, result.error());
        return;
    }

    state.decoder = result->ptr();
    state.mode = TrackState::Mode::Decoding;
    ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " decoding in the content process");
    updateDecoderReadiness(trackIdentifier);
    resumeWaitingBlocks(trackIdentifier);
    deliverMinimumUpcomingTime(state);
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::dispatchBlock(TrackIdentifier trackIdentifier, HeldBlock&& waitingBlock)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    auto& block = waitingBlock.block;
    bool shouldSelectMode = block.info() != state.selectedInfo.get() || block.isProtected() != state.selectedProtected || (state.waitingForSyncSample && block.isSync());
    if (state.mode != TrackState::Mode::Creating && state.mode != TrackState::Mode::Failed && shouldSelectMode)
        selectMode(trackIdentifier, block);

    switch (state.mode) {
    case TrackState::Mode::Failed:
        decrementPendingSamples(trackIdentifier);
        return;
    case TrackState::Mode::Creating:
        state.waiting.append(WTF::move(waitingBlock));
        return;
    case TrackState::Mode::Compressed:
        emitBlock(trackIdentifier, nullptr, WTF::move(waitingBlock));
        return;
    case TrackState::Mode::Decoding: {
        RefPtr decoder = state.decoder;
        decoder->decode(WTF::move(waitingBlock.block), waitingBlock.expectedMinimum)->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, generation = state.generation, weakDecoder = ThreadSafeWeakPtr { *decoder }](auto&& result) {
            RefPtr protectedThis { weakThis };
            RefPtr protectedDecoder { weakDecoder };
            if (protectedThis && protectedDecoder)
                protectedThis->didDecode(trackIdentifier, generation, protectedDecoder.get(), WTF::move(result));
        });
        updateDecoderReadiness(trackIdentifier);
        decrementPendingSamples(trackIdentifier);
        return;
    }
    }
}

void AudioVideoRendererRemote::resumeWaitingBlocks(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto waiting = std::exchange(trackState(trackIdentifier).waiting, { });
    for (auto& waitingBlock : waiting)
        dispatchBlock(trackIdentifier, WTF::move(waitingBlock));
}

void AudioVideoRendererRemote::dropPendingBlocks(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    auto compressedCount = [](const Vector<HeldBlock>& blocks) {
        return static_cast<size_t>(std::ranges::count_if(blocks, [](auto& held) {
            return !held.isDecoded;
        }));
    };
    size_t count = std::exchange(state.waiting, { }).size() + compressedCount(std::exchange(state.held, { }));
    for (auto& draining : state.drainingDecoders)
        count += compressedCount(std::exchange(draining.held, { }));
    if (!count)
        return;
    {
        Locker locker { m_lock };
        if (auto* readyState = readyForMoreDataStateIfExists(trackIdentifier)) {
            for (size_t i = 0; i < count; ++i)
                readyState->sampleReceived();
        }
    }
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::emitBlock(TrackIdentifier trackIdentifier, const PacedMediaSamplesDecoder* source, HeldBlock&& block)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    auto* held = &state.held;
    bool isFirst = state.drainingDecoders.isEmpty();
    if (source && source != state.decoder.get()) {
        auto index = state.drainingDecoders.findIf([&](auto& draining) {
            return draining.decoder.ptr() == source;
        });
        if (index == notFound)
            return;
        held = &state.drainingDecoders[index].held;
        isFirst = !index;
    }
    if (!isFirst) {
        held->append(WTF::move(block));
        return;
    }
    sendHeldBlock(trackIdentifier, WTF::move(block));
}

void AudioVideoRendererRemote::sendHeldBlock(TrackIdentifier trackIdentifier, HeldBlock&& held)
{
    assertIsCurrent(queueSingleton());

    if (held.isDecoded)
        sendDecodedBlock(trackIdentifier, WTF::move(held.block));
    else
        sendBlock(trackIdentifier, WTF::move(held.block), held.expectedMinimum);
}

void AudioVideoRendererRemote::sendHeldBlocks(TrackIdentifier trackIdentifier, Vector<HeldBlock>&& blocks)
{
    assertIsCurrent(queueSingleton());

    for (auto& held : blocks)
        sendHeldBlock(trackIdentifier, WTF::move(held));
}

void AudioVideoRendererRemote::releaseDrainedOutput(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    while (!state.drainingDecoders.isEmpty()) {
        auto held = std::exchange(state.drainingDecoders.first().held, { });
        bool isDrained = state.drainingDecoders.first().isDrained;
        if (isDrained)
            state.drainingDecoders.removeAt(0);
        sendHeldBlocks(trackIdentifier, WTF::move(held));
        if (!isDrained)
            return;
    }
    sendHeldBlocks(trackIdentifier, std::exchange(state.held, { }));
    deliverMinimumUpcomingTime(state);
}

void AudioVideoRendererRemote::deliverMinimumUpcomingTime(TrackState& state)
{
    assertIsCurrent(queueSingleton());

    if (!state.minimumUpcomingTime)
        return;
    switch (state.mode) {
    case TrackState::Mode::Creating:
        return;
    case TrackState::Mode::Decoding:
    case TrackState::Mode::Failed:
        state.minimumUpcomingTime.reset();
        return;
    case TrackState::Mode::Compressed: {
        if (!state.drainingDecoders.isEmpty())
            return;
        auto minimum = *std::exchange(state.minimumUpcomingTime, std::nullopt);
        if (RefPtr gpuProcessConnection = activeConnection())
            gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::ExpectMinimumUpcomingPresentationTime(m_identifier, minimum), 0);
        return;
    }
    }
}

void AudioVideoRendererRemote::decrementPendingSamples(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    {
        Locker locker { m_lock };
        if (auto* readyState = readyForMoreDataStateIfExists(trackIdentifier))
            readyState->sampleReceived();
    }
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::sendBlock(TrackIdentifier trackIdentifier, MediaSamplesBlock&& block, std::optional<MediaTime> expectedMinimum)
{
    assertIsCurrent(queueSingleton());

    RefPtr gpuProcessConnection = activeConnection();
    if (!gpuProcessConnection)
        return;

    auto& state = trackState(trackIdentifier);
    if (block.info() != state.lastSentInfo.get()) {
        state.lastSentInfo = block.info();
        gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::NewTrackInfoForTrack(m_identifier, trackIdentifier, *state.lastSentInfo), 0);
    }
    block.setInfo(nullptr);
    gpuProcessConnection->connection().sendWithAsyncReplyOnDispatcher(Messages::RemoteAudioVideoRendererProxyManager::EnqueueSample(m_identifier, trackIdentifier, WTF::move(block), expectedMinimum), queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier](bool readyForMoreData) {
        RefPtr protectedThis { weakThis };
        if (!protectedThis)
            return;
        auto pendingSamples = [&] -> std::optional<size_t> {
            Locker locker { protectedThis->m_lock };
            auto* readyState = protectedThis->readyForMoreDataStateIfExists(trackIdentifier);
            if (!readyState)
                return std::nullopt;
            readyState->sampleReceived();
            readyState->setRemoteReadyForMoreData(readyForMoreData);
            return readyState->pendingSamples();
        }();
        if (!pendingSamples)
            return;
        if (!*pendingSamples && !readyForMoreData) {
            if (RefPtr gpuProcessConnection = protectedThis->activeConnection())
                gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::RequestMediaDataWhenReady(protectedThis->m_identifier, trackIdentifier), 0);
            return;
        }
        protectedThis->resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
    }, 0);
}

void AudioVideoRendererRemote::sendDecodedBlock(TrackIdentifier trackIdentifier, MediaSamplesBlock&& block)
{
    assertIsCurrent(queueSingleton());

    if (block.isEmpty() || !activeConnection())
        return;

    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).sampleEnqueued();
    }
    sendBlock(trackIdentifier, WTF::move(block), std::nullopt);
}

void AudioVideoRendererRemote::didDecode(TrackIdentifier trackIdentifier, uint32_t generation, const PacedMediaSamplesDecoder* decoder, PacedMediaSamplesDecoder::DecodePromise::Result&& result)
{
    assertIsCurrent(queueSingleton());

    auto iterator = m_trackStates.find(trackIdentifier);
    if (iterator == m_trackStates.end() || iterator->value->generation != generation)
        return;

    if (!result) {
        if (result.error() == PlatformMediaError::Cancelled)
            return;
        decoderFailed(trackIdentifier, result.error());
        return;
    }
    for (auto& block : *result)
        emitBlock(trackIdentifier, decoder, { WTF::move(block), std::nullopt, true });
}

void AudioVideoRendererRemote::decoderFailed(TrackIdentifier trackIdentifier, PlatformMediaError error)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    if (state.mode == TrackState::Mode::Failed)
        return;

    ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " decoding in the content process failed: ", error);
    state.mode = TrackState::Mode::Failed;
    ++state.creationID;
    ++state.generation;
    state.minimumUpcomingTime.reset();
    dropPendingBlocks(trackIdentifier);
    flushDecoders(state);
    updateDecoderReadiness(trackIdentifier);
    sendFlushTrack(trackIdentifier);
    if (m_errorCallback)
        m_errorCallback(state.isVideo ? PlatformMediaError::VideoDecodingError : PlatformMediaError::AudioDecodingError);
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::hardwareDecoderUnavailable()
{
    assertIsCurrent(queueSingleton());

    bool hasCompressedVideo = false;
    bool fellBack = false;
    for (auto trackIdentifier : copyToVector(m_trackStates.keys())) {
        auto& state = trackState(trackIdentifier);
        if (!state.isVideo || state.mode != TrackState::Mode::Compressed || !state.lastSentInfo)
            continue;
        hasCompressedVideo = true;

        Ref info = *state.lastSentInfo;
        if (!m_decoderInContentProcess || !MediaSamplesDecoder::canDecode(info, contentProcessDecoderFilters() | MediaSamplesDecoder::Filter::ForceSoftware))
            continue;

        ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " has no hardware decoder, decoding in the content process");
        state.hardwareUnavailableCodecs.append(info->codecName());
        state.selectedInfo = nullptr;
        ++state.generation;
        dropPendingBlocks(trackIdentifier);
        flushDecoders(state);
        sendFlushTrack(trackIdentifier);
        requestVideoReenqueue(trackIdentifier);
        resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
        fellBack = true;
    }

    if (!hasCompressedVideo || fellBack)
        return;
    if (m_errorCallback)
        m_errorCallback(PlatformMediaError::VideoDecodingError);
}

void AudioVideoRendererRemote::requestVideoReenqueue(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    if (!m_notifyWhenRequiresFlushToResumeCallback) {
        state.awaitingKeyframe = true;
        return;
    }
    state.awaitingFlush = true;
    {
        Locker locker { m_lock };
        if (auto* readyState = readyForMoreDataStateIfExists(trackIdentifier))
            readyState->setAwaitingFlush(true);
    }
    m_notifyWhenRequiresFlushToResumeCallback();
}

void AudioVideoRendererRemote::willFlushTrack(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto iterator = m_trackStates.find(trackIdentifier);
    if (iterator == m_trackStates.end())
        return;
    auto& state = iterator->value.get();
    ++state.generation;
    state.awaitingFlush = false;
    state.minimumUpcomingTime.reset();
    {
        Locker locker { m_lock };
        if (auto* readyState = readyForMoreDataStateIfExists(trackIdentifier))
            readyState->setAwaitingFlush(false);
    }
    dropPendingBlocks(trackIdentifier);
    for (auto& draining : std::exchange(state.drainingDecoders, { }))
        protect(draining.decoder)->flush();
    if (RefPtr decoder = state.decoder)
        decoder->flush();
    updateDecoderReadiness(trackIdentifier);
}

void AudioVideoRendererRemote::releaseTrackState(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (auto state = m_trackStates.take(trackIdentifier))
        flushDecoders(*state);
    m_mediaSampleConverters.remove(trackIdentifier);
    {
        Locker locker { m_lock };
        m_readyForMoreDataStates.remove(trackIdentifier);
    }
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::releaseAllDecoders()
{
    assertIsCurrent(queueSingleton());

    auto trackStates = std::exchange(m_trackStates, { });
    for (auto& state : trackStates.values())
        flushDecoders(state);
    Locker locker { m_lock };
    for (auto& readyState : m_readyForMoreDataStates.values())
        readyState.setDecoderReadyForMoreData(true);
}

void AudioVideoRendererRemote::notifyDecodersOfTimeChange()
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis { weakThis };
        if (!protectedThis)
            return;
        assertIsCurrent(queueSingleton());
        for (auto& state : protectedThis->m_trackStates.values()) {
            if (RefPtr decoder = state->decoder)
                decoder->currentTimeDidChange();
            for (auto& draining : state->drainingDecoders)
                protect(draining.decoder)->currentTimeDidChange();
        }
    });
}

void AudioVideoRendererRemote::updateDecoderReadiness(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto& state = trackState(trackIdentifier);
    RefPtr decoder = state.decoder;
    bool isReady = !decoder || decoder->isReadyForMoreMediaData();
    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).setDecoderReadyForMoreData(isReady);
    }
    if (isReady || state.decoderAwaitingReadiness.get() == decoder)
        return;

    state.decoderAwaitingReadiness = decoder;
    decoder->requestMediaDataWhenReady()->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, weakDecoder = ThreadSafeWeakPtr { *decoder }](auto&&) {
        RefPtr protectedThis { weakThis };
        RefPtr protectedDecoder { weakDecoder };
        if (!protectedThis || !protectedDecoder)
            return;
        assertIsCurrent(queueSingleton());
        auto iterator = protectedThis->m_trackStates.find(trackIdentifier);
        if (iterator == protectedThis->m_trackStates.end() || iterator->value->decoderAwaitingReadiness.get() != protectedDecoder)
            return;
        iterator->value->decoderAwaitingReadiness = nullptr;
        protectedThis->updateDecoderReadiness(trackIdentifier);
        protectedThis->resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
    });
}

bool AudioVideoRendererRemote::isReadyForMoreSamples(TrackIdentifier trackIdentifier)
{
    Locker locker { m_lock };
    auto* readyState = readyForMoreDataStateIfExists(trackIdentifier);
    return !readyState || readyState->isReadyForMoreData();
}

Ref<AudioVideoRenderer::RequestPromise> AudioVideoRendererRemote::requestMediaDataWhenReady(TrackIdentifier trackIdentifier)
{
    return invokeAsync(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier] {
        assertIsCurrent(queueSingleton());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return RequestPromise::createAndReject(PlatformMediaError::Cancelled);
        if (protectedThis->m_isShutdown)
            return RequestPromise::createAndReject(PlatformMediaError::Shutdown);
        if (protectedThis->isReadyForMoreSamples(trackIdentifier))
            return RequestPromise::createAndResolve(trackIdentifier);
        auto addResult = protectedThis->m_requestMediaDataWhenReadyDataPromises.set(trackIdentifier, makeUnique<RequestPromise::AutoRejectProducer>(PlatformMediaError::Cancelled));
        return addResult.iterator->value->promise();
    });
}

void AudioVideoRendererRemote::notifyTrackNeedsReenqueuing(TrackIdentifier trackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>&& callback)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, trackIdentifier, callback = WTF::move(callback)]() mutable {
        assertIsCurrent(queueSingleton());
        if (protectedThis->m_isShutdown)
            return;
        if (callback)
            protectedThis->m_trackNeedsReenqueuingCallbacks.set(trackIdentifier, WTF::move(callback));
        else
            protectedThis->m_trackNeedsReenqueuingCallbacks.remove(trackIdentifier);
    });
}

bool AudioVideoRendererRemote::timeIsProgressing() const
{
    Locker locker { m_lock };
    return m_sharedTimebaseReader && m_sharedTimebaseReader->currentRate();
}

void AudioVideoRendererRemote::notifyEffectiveRateChanged(Function<void(double)>&& callback)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, callback = WTF::move(callback)]() mutable {
        assertIsCurrent(queueSingleton());
        if (protectedThis->m_isShutdown)
            return;
        protectedThis->m_effectiveRateChangedCallback = WTF::move(callback);
    });
}

MediaTime AudioVideoRendererRemote::currentTime() const
{
    return playbackPosition().time;
}

PacedMediaSamplesDecoder::PlaybackPosition AudioVideoRendererRemote::playbackPosition() const
{
    Locker locker { m_lock };
    if (m_seeking)
        return { m_lastSeekTime, 0 };
    if (!m_sharedTimebaseReader)
        return { MediaTime::zeroTime(), 0 };
    auto time = m_sharedTimebaseReader->currentTime();
    if (m_stallCap && time >= *m_stallCap)
        return { *m_stallCap, 0 };
    return { time, m_sharedTimebaseReader->currentRate() };
}

Ref<MediaTimePromise> AudioVideoRendererRemote::notifyTimeReachedAndStall(const MediaTime& time)
{
    {
        Locker locker { m_lock };
        m_stallCap = time;
    }
    notifyDecodersOfTimeChange();
    return invokeAsync(queueSingleton(), [protectedThis = Ref { *this }, this, time] -> Ref<MediaTimePromise> {
        assertIsCurrent(queueSingleton());
        if (m_stallRequest->hasCallback())
            protect(m_stallRequest)->disconnect();
        if (auto previous = std::exchange(m_stallProducer, std::nullopt))
            previous->reject(PlatformMediaError::Cancelled);

        if (m_isShutdown)
            return MediaTimePromise::createAndReject(PlatformMediaError::Shutdown);

        RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return MediaTimePromise::createAndReject(PlatformMediaError::IPCError);

        m_stallProducer.emplace();
        Ref promise = m_stallProducer->promise();
        gpuProcessConnection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteAudioVideoRendererProxyManager::NotifyTimeReachedAndStall(m_identifier, time), 0)->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](MediaTimePromise::Result&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            assertIsCurrent(queueSingleton());
            protect(protectedThis->m_stallRequest)->complete();
            if (auto producer = std::exchange(protectedThis->m_stallProducer, std::nullopt))
                producer->settle(WTF::move(result));
        })->track(protect(m_stallRequest));
        return promise;
    });
}

void AudioVideoRendererRemote::cancelTimeReachedAction()
{
    {
        Locker locker { m_lock };
        m_stallCap.reset();
    }
    notifyDecodersOfTimeChange();
    ensureOnDispatcherWithConnection([](auto& renderer, auto& connection) {
        assertIsCurrent(queueSingleton());
        if (renderer.m_stallRequest->hasCallback())
            protect(renderer.m_stallRequest)->disconnect();
        if (auto producer = std::exchange(renderer.m_stallProducer, std::nullopt))
            producer->reject(PlatformMediaError::Cancelled);
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::CancelTimeReachedAction(renderer.m_identifier), 0);
    });
}

void AudioVideoRendererRemote::performTaskAtTime(const MediaTime& time, Function<void(const MediaTime&)>&& callback)
{
    ensureOnDispatcherWithConnection([time, callback = WTF::move(callback)](auto& renderer, auto& connection) mutable {
        assertIsCurrent(queueSingleton());
        renderer.m_performTaskAtTimeCallback = WTF::move(callback);
        renderer.m_performTaskAtTime = time;
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::PerformTaskAtTime(renderer.m_identifier, time), 0);
    });
}

void AudioVideoRendererRemote::flush()
{
    ASSERT_NOT_REACHED();
}

void AudioVideoRendererRemote::flushTrack(TrackIdentifier identifier)
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }, identifier] {
        RefPtr protectedThis { weakThis };
        if (!protectedThis)
            return;
        protectedThis->willFlushTrack(identifier);
        protectedThis->sendFlushTrack(identifier);
    });
}

void AudioVideoRendererRemote::sendFlushTrack(TrackIdentifier identifier)
{
    assertIsCurrent(queueSingleton());

    RefPtr gpuProcessConnection = activeConnection();
    if (!gpuProcessConnection)
        return;

    // Wait on the GPU's reply (its post-flush isReadyForMoreSamples) before updating
    // readiness. IPC ordering guarantees this lands after any in-flight EnqueueSample
    // replies, so a stale reply can't clobber it. If the GPU is not ready it has armed
    // its ready-for-more callback, so a ReadyForMoreMediaData will follow.
    gpuProcessConnection->connection().sendWithAsyncReplyOnDispatcher(Messages::RemoteAudioVideoRendererProxyManager::FlushTrack(m_identifier, identifier), queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, identifier](bool isReadyForMoreMediaData) {
        RefPtr protectedThis { weakThis };
        if (!protectedThis)
            return;
        {
            Locker locker { protectedThis->m_lock };
            auto* readyState = protectedThis->readyForMoreDataStateIfExists(identifier);
            if (!readyState)
                return;
            readyState->setRemoteReadyForMoreData(isReadyForMoreMediaData);
        }
        if (!isReadyForMoreMediaData) {
            if (RefPtr gpuProcessConnection = protectedThis->activeConnection())
                gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::RequestMediaDataWhenReady(protectedThis->m_identifier, identifier), 0);
            return;
        }
        protectedThis->resolveRequestMediaDataWhenReadyIfNeeded(identifier);
    }, 0);
}

void AudioVideoRendererRemote::applicationWillResignActive()
{
    ensureOnDispatcherWithConnection([](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::ApplicationWillResignActive(renderer.m_identifier), 0);
    });
}

void AudioVideoRendererRemote::notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&& callback)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, callback = WTF::move(callback)]() mutable {
        assertIsCurrent(queueSingleton());
        if (protectedThis->m_isShutdown) {
            callback(PlatformMediaError::Shutdown);
            return;
        }
        protectedThis->m_errorCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::setSpatialTrackingInfo(bool prefersSpatialAudioExperience , SoundStageSize stage, const String& sceneIdentifier, const String& defaultLabel, const String& label)
{
    ensureOnDispatcherWithConnection([prefersSpatialAudioExperience, stage, sceneIdentifier = sceneIdentifier.isolatedCopy(), defaultLabel = defaultLabel.isolatedCopy(), label = label.isolatedCopy()](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetSpatialTrackingInfo(renderer.m_identifier, prefersSpatialAudioExperience, stage, sceneIdentifier, defaultLabel, label), 0);
    });
}

void AudioVideoRendererRemote::ensureOnDispatcherSync(NOESCAPE Function<void()>&& function)
{
    if (queueSingleton().isCurrent())
        function();
    else
        queueSingleton().dispatchSync(WTF::move(function));
}

void AudioVideoRendererRemote::ensureOnDispatcher(Function<void()>&& function)
{
    if (queueSingleton().isCurrent())
        function();
    else
        queueSingleton().dispatch(WTF::move(function));
}

void AudioVideoRendererRemote::ensureOnDispatcherWithConnection(Function<void(AudioVideoRendererRemote&, IPC::Connection&)>&& function)
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }, function = WTF::move(function)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_isShutdown)
            return;
        RefPtr gpuProcessConnection = protectedThis->m_gpuProcessConnection.get();
        if (!protectedThis->isGPURunning() || !gpuProcessConnection)
            return;
        function(*protectedThis, gpuProcessConnection->connection());
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioVideoRendererRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

void AudioVideoRendererRemote::updateCacheState(const RemoteAudioVideoRendererState& state)
{
    constexpr Seconds playbackQualityMetricsTimeout = 30_s;
    bool shouldDisableMetrics = false;
    {
        Locker locker { m_lock };
        m_cachedState.paused = state.paused;
        if (m_videoPlaybackMetricsUpdateInterval && (MonotonicTime::now() - m_lastPlaybackQualityMetricsQueryTime) > playbackQualityMetricsTimeout) {
            m_videoPlaybackMetricsUpdateInterval = 0_s;
            shouldDisableMetrics = true;
        }
    }
    if (shouldDisableMetrics)
        updateVideoPlaybackMetricsUpdateInterval(0_s);
}

AudioVideoRendererRemote::ReadyForMoreDataState& AudioVideoRendererRemote::readyForMoreDataState(TrackIdentifier trackIdentifier)
{
    assertIsHeld(m_lock);
    auto addResult = m_readyForMoreDataStates.add(trackIdentifier, ReadyForMoreDataState { });
    return addResult.iterator->value;
}

AudioVideoRendererRemote::ReadyForMoreDataState* AudioVideoRendererRemote::readyForMoreDataStateIfExists(TrackIdentifier trackIdentifier)
{
    assertIsHeld(m_lock);
    auto iterator = m_readyForMoreDataStates.find(trackIdentifier);
    return iterator == m_readyForMoreDataStates.end() ? nullptr : &iterator->value;
}

void AudioVideoRendererRemote::resolveRequestMediaDataWhenReadyIfNeeded(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (!isReadyForMoreSamples(trackIdentifier))
        return;
    auto iterator = m_requestMediaDataWhenReadyDataPromises.find(trackIdentifier);
    if (iterator == m_requestMediaDataWhenReadyDataPromises.end())
        return;
    m_requestMediaDataWhenReadyDataPromises.take(iterator)->resolve(trackIdentifier);
}

Ref<AudioVideoRenderer::HostingContextPromise> AudioVideoRendererRemote::requestHostingContext()
{
    return invokeAsync(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return HostingContextPromise::createAndReject();
        assertIsCurrent(queueSingleton());

        // FIXME: should it be called on the main thread???
        RefPtr gpuProcessConnection = protectedThis->m_gpuProcessConnection.get();
        if (protectedThis->m_isShutdown || !protectedThis->isGPURunning() || !gpuProcessConnection)
            return HostingContextPromise::createAndReject();

        auto layerHostingContext = [&] {
            Locker locker { protectedThis->m_lock };
            return protectedThis->m_layerHostingContext;
        }();
        if (layerHostingContext.contextID)
            return HostingContextPromise::createAndResolve(WTF::move(layerHostingContext));

        HostingContextPromise::AutoRejectProducer producer;
        Ref promise = producer.promise();
        gpuProcessConnection->connection().sendWithAsyncReplyOnDispatcher(Messages::RemoteAudioVideoRendererProxyManager::RequestHostingContext(protectedThis->m_identifier), queueSingleton(), [weakThis, producer = WTF::move(producer)] (WebCore::HostingContext context) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !context.contextID)
                return;
            assertIsCurrent(queueSingleton());
            protectedThis->setLayerHostingContext(WebCore::HostingContext { context });
            producer.resolve(WTF::move(context));
        }, 0);
        return promise;
    });
}

WebCore::HostingContext AudioVideoRendererRemote::hostingContext() const
{
    Locker locker { m_lock };
    return m_layerHostingContext;
}

void AudioVideoRendererRemote::setLayerHostingContext(WebCore::HostingContext&& hostingContext)
{
    assertIsCurrent(queueSingleton());

    Locker locker { m_lock };
    if (m_layerHostingContext.contextID == hostingContext.contextID)
        return;
    m_layerHostingContext = WTF::move(hostingContext);
#if PLATFORM(COCOA)
    m_videoLayer = nullptr;
#endif
}

bool AudioVideoRendererRemote::inVideoFullscreenOrPictureInPicture() const
{
#if PLATFORM(COCOA) && ENABLE(VIDEO_PRESENTATION_MODE)
    Locker locker { m_lock };
    return !!m_videoLayerManager->videoFullscreenLayer();
#else
    return false;
#endif
}

WebCore::FloatSize AudioVideoRendererRemote::naturalSize() const
{
    Locker locker { m_lock };
    return m_naturalSize;
}

#if ENABLE(ENCRYPTED_MEDIA)
void AudioVideoRendererRemote::setCDMInstance(CDMInstance* instance)
{
    std::optional<RemoteCDMInstanceIdentifier> identifier;
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        identifier = remoteInstance->identifier();

    ensureOnDispatcherWithConnection([identifier = WTF::move(identifier)](auto& renderer, auto& connection) mutable {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetCDMInstance(renderer.m_identifier, WTF::move(identifier)), 0);
    });
}

Ref<MediaPromise> AudioVideoRendererRemote::setInitData(Ref<SharedBuffer> initData)
{
    return invokeAsync(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, initData = WTF::move(initData)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);
        if (protectedThis->m_isShutdown)
            return MediaPromise::createAndReject(PlatformMediaError::Shutdown);
        RefPtr gpuProcessConnection = protectedThis->m_gpuProcessConnection.get();
        if (!protectedThis->isGPURunning() || !gpuProcessConnection)
            return MediaPromise::createAndReject(PlatformMediaError::IPCError);

        return gpuProcessConnection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteAudioVideoRendererProxyManager::SetInitData(protectedThis->m_identifier, WTF::move(initData)), 0);
    });
}

void AudioVideoRendererRemote::attemptToDecrypt()
{
    ensureOnDispatcherWithConnection([](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::AttemptToDecrypt(renderer.m_identifier), 0);
    });
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void AudioVideoRendererRemote::setCDMSession(LegacyCDMSession* session)
{
    std::optional<RemoteLegacyCDMSessionIdentifier> identifier;
    if (auto* remoteSession = dynamicDowncast<RemoteLegacyCDMSession>(session))
        identifier = remoteSession->identifier();

    ensureOnDispatcherWithConnection([identifier = WTF::move(identifier)](auto& renderer, auto& connection) mutable {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetLegacyCDMSession(renderer.m_identifier, WTF::move(identifier)), 0);
    });
}
#endif


#if PLATFORM(COCOA)

WebCore::FloatSize AudioVideoRendererRemote::videoLayerSize() const
{
    Locker locker { m_lock };
    return m_videoLayerSize;
}

void AudioVideoRendererRemote::setVideoLayerSize(const WebCore::FloatSize& size)
{
    {
        Locker locker { m_lock };
        m_videoLayerSize = size;
    }

    ensureOnDispatcherWithConnection([size](auto& renderer, auto& connection) mutable {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetVideoLayerSize(renderer.m_identifier, size), 0);
    });
}

void AudioVideoRendererRemote::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    {
        Locker locker { m_lock };
        m_videoLayerSize = size;
    }

    ensureOnDispatcherWithConnection([size, sendRightAnnotated = WTF::move(sendRightAnnotated)](auto& renderer, auto& connection) mutable {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::SetVideoLayerSizeFenced(renderer.m_identifier, size, WTF::move(sendRightAnnotated)), 0);
    });
}
#endif

void AudioVideoRendererRemote::notifyVideoLayerSizeChanged(Function<void(const MediaTime&, FloatSize)>&& callback)
{
    ensureOnDispatcher([protectedThis = Ref { *this }, callback = WTF::move(callback)]() mutable {
        assertIsCurrent(queueSingleton());
        if (protectedThis->m_isShutdown)
            return;
        protectedThis->m_videoLayerSizeChangedCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection.get() == &connection);
    m_gpuProcessConnectionShutDown = true;
    ensureOnDispatcher([connection = Ref { connection }, identifier = m_identifier, protectedThis = Ref { *this }] {
        connection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Destroy(identifier), 0);
        connection->connection().removeWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), identifier.toUInt64());
        assertIsCurrent(queueSingleton());
        protectedThis->releaseAllDecoders();
        if (protectedThis->m_errorCallback)
            protectedThis->m_errorCallback(PlatformMediaError::IPCError);
    });
}

AudioVideoRendererRemote::MessageReceiver::MessageReceiver(AudioVideoRendererRemote& parent)
    : m_parent(parent)
{
}

void AudioVideoRendererRemote::MessageReceiver::firstFrameAvailable(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        if (parent->m_firstFrameAvailableCallback)
            parent->m_firstFrameAvailableCallback();
    }
}

void AudioVideoRendererRemote::MessageReceiver::hasAvailableVideoFrame(MediaTime time, double clockTime, RemoteAudioVideoRendererState state, std::optional<VideoPlaybackQualityMetrics> metrics)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        if (metrics) {
            Locker locker { parent->m_lock };
            parent->m_cachedState.videoPlaybackQualityMetrics = WTF::move(metrics);
        }
        if (parent->m_hasAvailableVideoFrameCallback)
            parent->m_hasAvailableVideoFrameCallback(time, clockTime);
    }
}

void AudioVideoRendererRemote::MessageReceiver::requiresFlushToResume(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        if (parent->m_notifyWhenRequiresFlushToResumeCallback)
            parent->m_notifyWhenRequiresFlushToResumeCallback();
    }
}

void AudioVideoRendererRemote::MessageReceiver::renderingModeChanged(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        if (parent->m_renderingModeChangedCallback)
            parent->m_renderingModeChangedCallback();
    }
}

void AudioVideoRendererRemote::MessageReceiver::sizeChanged(MediaTime time, WebCore::FloatSize size, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        {
            Locker locker { parent->m_lock };
            parent->m_naturalSize = size;
        }
        if (parent->m_sizeChangedCallback)
            parent->m_sizeChangedCallback(time, size);
    }
}

void AudioVideoRendererRemote::MessageReceiver::trackNeedsReenqueuing(TrackIdentifier trackIdentifier, MediaTime time, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        auto iterator = parent->m_trackNeedsReenqueuingCallbacks.find(trackIdentifier);
        if (iterator == parent->m_trackNeedsReenqueuingCallbacks.end() || !iterator->value)
            return;
        iterator->value(trackIdentifier, time);
    }
}

void AudioVideoRendererRemote::MessageReceiver::effectiveRateChanged(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        parent->notifyDecodersOfTimeChange();
        if (!parent->m_effectiveRateChangedCallback)
            return;
        double effectiveRate = 0.0;
        {
            Locker locker { parent->m_lock };
            if (parent->m_sharedTimebaseReader)
                effectiveRate = parent->m_sharedTimebaseReader->currentRate();
        }
        parent->m_effectiveRateChangedCallback(effectiveRate);
    }
}

void AudioVideoRendererRemote::MessageReceiver::taskTimeReached(MediaTime time, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        parent->updateCacheState(state);
        if (parent->m_performTaskAtTimeCallback && time == parent->m_performTaskAtTime)
            parent->m_performTaskAtTimeCallback(time);
    }
}

void AudioVideoRendererRemote::MessageReceiver::errorOccurred(WebCore::PlatformMediaError error)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        if (error == PlatformMediaError::HardwareDecoderUnavailable) {
            parent->hardwareDecoderUnavailable();
            return;
        }
        if (parent->m_errorCallback)
            parent->m_errorCallback(error);
    }
}

void AudioVideoRendererRemote::MessageReceiver::readyForMoreMediaData(TrackIdentifier trackIdentifier)
{
    if (RefPtr parent = m_parent.get()) {
        {
            Locker locker { parent->m_lock };
            auto* readyState = parent->readyForMoreDataStateIfExists(trackIdentifier);
            if (!readyState)
                return;
            readyState->setRemoteReadyForMoreData(true);
        }
        parent->resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
    }
}

void AudioVideoRendererRemote::MessageReceiver::stateUpdate(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get())
        parent->updateCacheState(state);
}

void AudioVideoRendererRemote::MessageReceiver::updatePlaybackQualityMetrics(WebCore::VideoPlaybackQualityMetrics metrics)
{
    if (RefPtr parent = m_parent.get()) {
        DEBUG_LOG_WITH_THIS(parent.get(), LOGIDENTIFIER_WITH_THIS(parent.get()), "total=", metrics.totalVideoFrames, " dropped=", metrics.droppedVideoFrames, " corrupted=", metrics.corruptedVideoFrames, " displayComposited=", metrics.displayCompositedVideoFrames, " frameDelay=", metrics.totalFrameDelay);
        Locker locker { parent->m_lock };
        parent->m_cachedState.videoPlaybackQualityMetrics = WTF::move(metrics);
    }
}

#if PLATFORM(COCOA)
void AudioVideoRendererRemote::MessageReceiver::layerHostingContextChanged(RemoteAudioVideoRendererState state, WebCore::HostingContext&& hostingContext, const WebCore::FloatSize& videoLayerSize)
{
    if (RefPtr parent = m_parent.get()) {
        assertIsCurrent(queueSingleton());
        if (!hostingContext.contextID) {
            Locker locker { parent->m_lock };
            parent->m_videoLayer = nullptr;
            parent->m_videoLayerManager->didDestroyVideoLayer();
            return;
        }
        {
            Locker locker { parent->m_lock };
            parent->m_videoLayerSize = videoLayerSize;
        }
        parent->updateCacheState(state);
        parent->setLayerHostingContext(WTF::move(hostingContext));
        if (!parent->m_videoLayerSizeChangedCallback)
            return;
        MediaTime currentTime;
        {
            Locker locker { parent->m_lock };
            if (parent->m_sharedTimebaseReader)
                currentTime = parent->m_sharedTimebaseReader->currentTime();
        }
        parent->m_videoLayerSizeChangedCallback(currentTime, videoLayerSize);
    }
}
#endif

} // namespace WebKit

#endif
