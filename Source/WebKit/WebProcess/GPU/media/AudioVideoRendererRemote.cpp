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
#include <wtf/Borrow.h>
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

Ref<AudioVideoRendererRemote> AudioVideoRendererRemote::create(LoggerHelper* loggerHelper, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier, GPUProcessConnection& connection)
{
    assertIsMainThread();

    auto identifier = RemoteAudioVideoRendererIdentifier::generate();
    return adoptRef(*new AudioVideoRendererRemote(loggerHelper, connection, mediaElementIdentifier, playerIdentifier, identifier));
}

AudioVideoRendererRemote::AudioVideoRendererRemote(LoggerHelper* loggerHelper, GPUProcessConnection& connection, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier, RemoteAudioVideoRendererIdentifier identifier)
    : m_gpuProcessConnection(connection)
    , m_receiver(MessageReceiver::create(*this))
    , m_identifier(identifier)
    , m_stallRequest(NativePromiseRequest::create())
    , m_audioDecodingInContentProcess(WebProcess::singleton().sharedPreferencesForWebProcessValue().audioDecodingInContentProcessEnabled)
    , m_videoCodecsDecodedInContentProcess(WebProcess::singleton().sharedPreferencesForWebProcessValue().videoSoftwareDecodingInContentProcessEnabled ? WebProcess::singleton().videoCodecsDecodedInContentProcess() : Vector<FourCC> { })
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
            protectedThis->ensureWeakOnDispatcher([](auto& renderer) {
                assertIsCurrent(queueSingleton());
                if (renderer.m_errorCallback)
                    renderer.m_errorCallback(PlatformMediaError::MemoryError);
            });
            return;
        }
        auto reader = SharedTimebaseReader::create(WTF::move(*handle));
        if (!reader) {
            protectedThis->ensureWeakOnDispatcher([](auto& renderer) {
                assertIsCurrent(queueSingleton());
                if (renderer.m_errorCallback)
                    renderer.m_errorCallback(PlatformMediaError::MemoryError);
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

    ensureOnDispatcher([prepareSeekRequest = WTF::move(m_prepareSeekRequest), prepareSeekPromise = WTF::move(m_prepareSeekPromise), finishSeekRequest = WTF::move(m_finishSeekRequest), finishSeekPromise = WTF::move(m_finishSeekPromise), stallRequest = WTF::move(m_stallRequest), stallProducer = WTF::move(m_stallProducer), audioTrackStates = std::exchange(m_audioTrackStates, { }), videoTrackState = std::exchange(m_videoTrackState, nullptr)] mutable {
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
        for (auto& state : audioTrackStates.values())
            releaseDecoder(state);
        if (videoTrackState)
            releaseDecoder(*videoTrackState);
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
    ensureWeakOnDispatcher([](auto& renderer) {
        renderer.releaseAllDecoders();
        RefPtr gpuProcessConnection { renderer.m_gpuProcessConnection };
        if (!renderer.isGPURunning() || !gpuProcessConnection)
            return;
        gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Shutdown(renderer.m_identifier), 0);
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
        // A video decoder in the content process gets the minimum with each sample, and its output is in presentation order.
        if (auto& state = renderer.m_videoTrackState; state && state->mode != TrackState::Mode::Compressed && state->mode != TrackState::Mode::Failed)
            return;
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
    auto reference = RemoteVideoFrameReference::generateForAdd();
    callOnMainRunLoopAndWait([&] {
        if (m_isShutdown)
            return;
        auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteAudioVideoRendererProxyManager::CurrentVideoFrame(m_identifier, reference), 0);
        if (!sendResult.succeeded())
            return;

        auto [result] = sendResult.takeReply();
        if (!result)
            return;
        videoFrame = RemoteVideoFrameProxy::create(gpuProcessConnection->connection(), protect(gpuProcessConnection->videoFrameObjectHeapProxy()), reference, WTF::move(*result));
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
    if (type == TrackType::Video) {
        // As with AudioVideoRendererAVFObjC, a new video track replaces the previous one.
        ensureWeakOnDispatcher([trackIdentifier = *result](auto& renderer) {
            assertIsCurrent(queueSingleton());
            if (auto previousTrackIdentifier = renderer.m_videoTrackIdentifier; previousTrackIdentifier && *previousTrackIdentifier != trackIdentifier)
                renderer.releaseTrackState(*previousTrackIdentifier);
            renderer.m_videoTrackIdentifier = trackIdentifier;
        });
    }
    return *result;
}

void AudioVideoRendererRemote::removeTrack(TrackIdentifier trackIdentifier)
{
    ensureWeakOnDispatcher([trackIdentifier](auto& renderer) {
        renderer.releaseTrackState(trackIdentifier);
    });
    ensureOnDispatcherWithConnection([trackIdentifier](auto& renderer, auto& connection) {
        connection.send(Messages::RemoteAudioVideoRendererProxyManager::RemoveTrack(renderer.m_identifier, trackIdentifier), 0);
    });
}

void AudioVideoRendererRemote::enqueueSample(TrackIdentifier trackIdentifier, Ref<MediaSample>&& sample, std::optional<MediaTime> expectedMinimum)
{
    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).addPendingSample();
    }
    ensureWeakOnDispatcher([trackIdentifier, sample = WTF::move(sample), expectedMinimum](auto& renderer) mutable {
        if (!renderer.activeConnection())
            return;
        renderer.processSample(trackIdentifier, WTF::move(sample), expectedMinimum);
    });
}

bool AudioVideoRendererRemote::shouldDecodeInContentProcess(const TrackInfo& info, bool isProtected) const
{
    if (isProtected)
        return false;
#if ENABLE(ENCRYPTED_MEDIA)
    if (info.encryptionDataCollection())
        return false;
#endif
    if (info.isVideo())
        return m_videoCodecsDecodedInContentProcess.contains(info.codecName()) && !MediaSamplesDecoder::isHardwareDecodable(info);
    if (!m_audioDecodingInContentProcess)
        return false;
#if PLATFORM(COCOA)
    RefPtr audioInfo = dynamicDowncast<AudioInfo>(info);
    return audioInfo && !audioInfo->isDecoded();
#else
    return false;
#endif
}

AudioVideoRendererRemote::TrackState* AudioVideoRendererRemote::trackStateIfExists(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (isVideoTrack(trackIdentifier))
        return m_videoTrackState.get();
    auto iterator = m_audioTrackStates.find(trackIdentifier);
    return iterator == m_audioTrackStates.end() ? nullptr : iterator->value.ptr();
}

bool AudioVideoRendererRemote::isVideoTrack(TrackIdentifier trackIdentifier) const
{
    assertIsCurrent(queueSingleton());

    return m_videoTrackIdentifier == trackIdentifier;
}

template<typename Function>
void AudioVideoRendererRemote::forEachTrackState(NOESCAPE const Function& function)
{
    assertIsCurrent(queueSingleton());

    for (auto& state : m_audioTrackStates.values())
        function(state.get());
    if (m_videoTrackState)
        function(*m_videoTrackState);
}

AudioVideoRendererRemote::TrackState* AudioVideoRendererRemote::trackStateIfCurrent(TrackIdentifier trackIdentifier, uint64_t generation)
{
    auto* state = trackStateIfExists(trackIdentifier);
    return state && state->generation == generation ? state : nullptr;
}

AudioVideoRendererRemote::TrackState& AudioVideoRendererRemote::setTrackState(TrackIdentifier trackIdentifier, UniqueRef<TrackState>&& state)
{
    assertIsCurrent(queueSingleton());

    if (isVideoTrack(trackIdentifier)) {
        m_videoTrackState = state.moveToUniquePtr();
        return *m_videoTrackState;
    }
    return m_audioTrackStates.set(trackIdentifier, WTF::move(state)).iterator->value.get();
}

std::unique_ptr<AudioVideoRendererRemote::TrackState> AudioVideoRendererRemote::takeTrackState(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (isVideoTrack(trackIdentifier))
        return std::exchange(m_videoTrackState, nullptr);
    return m_audioTrackStates.take(trackIdentifier);
}

AudioVideoRendererRemote::TrackState& AudioVideoRendererRemote::trackStateForFormat(TrackIdentifier trackIdentifier, Ref<const TrackInfo>&& info, bool isProtected)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    if (!state)
        return createTrackState(trackIdentifier, WTF::move(info), isProtected);
    if (state->matches(info, isProtected))
        return *state;

    switch (state->mode) {
    case TrackState::Mode::Compressed:
        return createTrackState(trackIdentifier, WTF::move(info), isProtected);
    case TrackState::Mode::Decoding:
        if (!isProtected && protect(state->decoder)->canContinueWith(info))
            return replaceDecodingTrackState(trackIdentifier, WTF::move(info), isProtected, state->generation);
        // The new state is created from the first waiting sample once the decoder is drained.
        drainDecoder(trackIdentifier, *state, isProtected);
        return *state;
    case TrackState::Mode::Creating:
    case TrackState::Mode::Draining:
    case TrackState::Mode::Failed:
        return *state;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static MediaSamplesDecoder::Filters contentProcessDecoderFilters()
{
    return { MediaSamplesDecoder::Filter::InProcess, MediaSamplesDecoder::Filter::SoftwareAllowed, MediaSamplesDecoder::Filter::UseSharedMemory };
}

AudioVideoRendererRemote::TrackState& AudioVideoRendererRemote::createTrackState(TrackIdentifier trackIdentifier, Ref<const TrackInfo>&& info, bool isProtected)
{
    assertIsCurrent(queueSingleton());

    bool hardwareUnavailable = !isProtected && info->isVideo() && m_hardwareUnavailableVideoCodecs.contains(info->codecName());
    if (!hardwareUnavailable && !shouldDecodeInContentProcess(info, isProtected))
        return setTrackState(trackIdentifier, makeUniqueRef<TrackState>(WTF::move(info), isProtected, ++m_nextTrackStateGeneration, TrackState::Mode::Compressed));

    auto filters = contentProcessDecoderFilters();
    if (hardwareUnavailable)
        filters.add(MediaSamplesDecoder::Filter::ForceSoftware);
    if (!MediaSamplesDecoder::canDecode(info, filters)) {
        ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " cannot be decoded in the content process");
        failTrack(trackIdentifier, WTF::move(info), isProtected, PlatformMediaError::NotSupportedError, 0);
        return *trackStateIfExists(trackIdentifier);
    }

    ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " creating decoder");
    auto& state = setTrackState(trackIdentifier, makeUniqueRef<TrackState>(WTF::move(info), isProtected, ++m_nextTrackStateGeneration, TrackState::Mode::Creating));
    PacedMediaSamplesDecoder::create(state.info, filters, queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis { weakThis })
            return protectedThis->playbackPosition();
        return PacedMediaSamplesDecoder::invalidPlaybackPosition;
    })->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, generation = state.generation](auto&& result) {
        RefPtr protectedThis { weakThis };
        if (!protectedThis || !protectedThis->trackStateIfCurrent(trackIdentifier, generation))
            return;
        if (!result) {
            protectedThis->decoderFailed(trackIdentifier, result.error());
            return;
        }
        protectedThis->didCreateDecoder(trackIdentifier, WTF::move(*result));
    });
    return state;
}

AudioVideoRendererRemote::TrackState& AudioVideoRendererRemote::replaceDecodingTrackState(TrackIdentifier trackIdentifier, Ref<const TrackInfo>&& info, bool isProtected, uint64_t generation)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    RELEASE_ASSERT(state && state->mode == TrackState::Mode::Decoding);
    auto newState = makeUniqueRef<TrackState>(WTF::move(info), isProtected, generation, TrackState::Mode::Decoding, WTF::move(state->decoder));
    newState->decoderAwaitingReadiness = state->decoderAwaitingReadiness;
    return setTrackState(trackIdentifier, WTF::move(newState));
}

void AudioVideoRendererRemote::releaseDecoder(TrackState& state)
{
    if (RefPtr decoder = std::exchange(state.decoder, nullptr))
        decoder->flush();
}

RefPtr<GPUProcessConnection> AudioVideoRendererRemote::activeConnection()
{
    if (m_isShutdown || !isGPURunning())
        return nullptr;
    return m_gpuProcessConnection.get();
}

void AudioVideoRendererRemote::processSample(TrackIdentifier trackIdentifier, Ref<MediaSample>&& mediaSample, std::optional<MediaTime> expectedMinimum)
{
    assertIsCurrent(queueSingleton());

    if (isVideoTrack(trackIdentifier)) {
        if (m_videoAwaitingFlush || (m_videoAwaitingKeyframe && !mediaSample->isSync())) {
            removePendingSamples(trackIdentifier);
            return;
        }
        m_videoAwaitingKeyframe = false;
    }

    auto& converter = m_mediaSampleConverters.ensure(trackIdentifier, [] {
        return MediaSampleConverter();
    }).iterator->value;
    auto sample = converter.convert(mediaSample, MediaSampleConverter::SetTrackInfo::Yes);
    if (RefPtr info = sample->info(); info && info->isVideo()) {
        if (!isVideoTrack(trackIdentifier)) {
            // Only the most recently added video track is rendered.
            ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " is not the current video track, dropping sample");
            removePendingSamples(trackIdentifier);
            return;
        }
        if (m_keyframeNeeded && !mediaSample->isSync()) {
            ALWAYS_LOG(LOGIDENTIFIER, "Keyframe needed: but frame not keyframe");
            ASSERT_NOT_REACHED();
            removePendingSamples(trackIdentifier);
            return;
        }
        m_keyframeNeeded = false;
    }
    dispatchSample(trackIdentifier, { WTF::move(sample.get()), expectedMinimum });
}

void AudioVideoRendererRemote::dispatchSample(TrackIdentifier trackIdentifier, PendingSample&& pending)
{
    assertIsCurrent(queueSingleton());

    RefPtr info = pending.sample.info();
    if (!info) {
        removePendingSamples(trackIdentifier);
        return;
    }
    bool isProtected = pending.sample.isProtected();
    // Whether the sample remains pending until the GPU process or a later dispatch consumes it.
    bool isStillPending = [&] {
        Borrow state = trackStateForFormat(trackIdentifier, info.releaseNonNull(), isProtected);
        switch (state->mode) {
        case TrackState::Mode::Failed:
            return false;
        case TrackState::Mode::Creating:
        case TrackState::Mode::Draining:
            state->waiting.append(WTF::move(pending));
            return true;
        case TrackState::Mode::Compressed:
            sendSample(trackIdentifier, WTF::move(pending.sample), pending.expectedMinimum);
            return true;
        case TrackState::Mode::Decoding:
            protect(state->decoder)->decode(WTF::move(pending.sample), pending.expectedMinimum)->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, generation = state->generation](auto&& result) {
                RefPtr protectedThis { weakThis };
                if (!protectedThis || !protectedThis->trackStateIfCurrent(trackIdentifier, generation))
                    return;
                if (!result) {
                    if (result.error() != PlatformMediaError::Cancelled)
                        protectedThis->decoderFailed(trackIdentifier, result.error());
                    return;
                }
                protectedThis->sendDecodedSamples(trackIdentifier, WTF::move(*result));
            });
            updateDecoderReadiness(trackIdentifier);
            return false;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();
    if (!isStillPending)
        removePendingSamples(trackIdentifier);
}

void AudioVideoRendererRemote::drainDecoder(TrackIdentifier trackIdentifier, TrackState& state, bool isProtected)
{
    assertIsCurrent(queueSingleton());
    ASSERT(state.mode == TrackState::Mode::Decoding);

    ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), isProtected ? " content is protected" : " format changed", ", draining decoder");
    state.mode = TrackState::Mode::Draining;
    updateDecoderReadiness(trackIdentifier);
    protect(state.decoder)->drain()->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, generation = state.generation](auto&& result) {
        RefPtr protectedThis { weakThis };
        if (!protectedThis || !protectedThis->trackStateIfCurrent(trackIdentifier, generation))
            return;
        if (!result) {
            if (result.error() != PlatformMediaError::Cancelled)
                protectedThis->decoderFailed(trackIdentifier, result.error());
            return;
        }
        protectedThis->didDrainDecoder(trackIdentifier, WTF::move(*result));
    });
}

void AudioVideoRendererRemote::didCreateDecoder(TrackIdentifier trackIdentifier, Ref<PacedMediaSamplesDecoder>&& decoder)
{
    assertIsCurrent(queueSingleton());

    {
        Borrow state = *trackStateIfExists(trackIdentifier);
        ASSERT(state->mode == TrackState::Mode::Creating);
        ALWAYS_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " decoding in the content process");
        state->decoder = WTF::move(decoder);
        state->mode = TrackState::Mode::Decoding;
    }
    updateDecoderReadiness(trackIdentifier);
    resumeWaitingSamples(trackIdentifier);
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::didDrainDecoder(TrackIdentifier trackIdentifier, Vector<MediaSamplesBlock>&& samples)
{
    assertIsCurrent(queueSingleton());

    auto state = takeTrackState(trackIdentifier);
    RELEASE_ASSERT(state);
    ASSERT(state->mode == TrackState::Mode::Draining);
    auto waiting = std::exchange(state->waiting, { });
    state = nullptr;

    sendDecodedSamples(trackIdentifier, WTF::move(samples));
    updateDecoderReadiness(trackIdentifier);
    for (auto& pending : borrow(waiting))
        dispatchSample(trackIdentifier, WTF::move(pending));
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::resumeWaitingSamples(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    if (!state)
        return;
    for (auto& pending : std::exchange(state->waiting, { }))
        dispatchSample(trackIdentifier, WTF::move(pending));
}

void AudioVideoRendererRemote::dropWaitingSamples(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    if (!state)
        return;
    if (size_t count = std::exchange(state->waiting, { }).size())
        removePendingSamples(trackIdentifier, count);
}

void AudioVideoRendererRemote::decoderFailed(TrackIdentifier trackIdentifier, PlatformMediaError error)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    if (!state || state->mode == TrackState::Mode::Failed)
        return;
    releaseDecoder(*state);
    failTrack(trackIdentifier, state->info.copyRef(), state->isProtected, error, state->waiting.size());
}

void AudioVideoRendererRemote::failTrack(TrackIdentifier trackIdentifier, Ref<const TrackInfo>&& info, bool isProtected, PlatformMediaError error, size_t droppedSamples)
{
    assertIsCurrent(queueSingleton());

    ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " decoding in the content process failed: ", error);
    setTrackState(trackIdentifier, makeUniqueRef<TrackState>(WTF::move(info), isProtected, ++m_nextTrackStateGeneration, TrackState::Mode::Failed));
    updateDecoderReadiness(trackIdentifier);
    sendFlushTrack(trackIdentifier);
    if (m_errorCallback)
        m_errorCallback(isVideoTrack(trackIdentifier) ? PlatformMediaError::VideoDecodingError : PlatformMediaError::AudioDecodingError);
    removePendingSamples(trackIdentifier, droppedSamples);
}

void AudioVideoRendererRemote::hardwareDecoderUnavailable()
{
    assertIsCurrent(queueSingleton());

    if (!m_videoTrackIdentifier)
        return;
    auto trackIdentifier = *m_videoTrackIdentifier;
    auto* state = trackStateIfExists(trackIdentifier);
    if (!state || state->mode != TrackState::Mode::Compressed)
        return;
    auto iterator = m_lastSentInfos.find(trackIdentifier);
    if (iterator == m_lastSentInfos.end())
        return;

    Ref info = iterator->value;
    if (!MediaSamplesDecoder::canDecode(info, contentProcessDecoderFilters() | MediaSamplesDecoder::Filter::ForceSoftware)) {
        if (m_errorCallback)
            m_errorCallback(PlatformMediaError::VideoDecodingError);
        return;
    }

    ERROR_LOG(LOGIDENTIFIER, "track ", trackIdentifier.toUInt64(), " codec ", info->codecName(), " has no hardware decoder, decoding in the content process");
    if (!m_hardwareUnavailableVideoCodecs.contains(info->codecName()))
        m_hardwareUnavailableVideoCodecs.append(info->codecName());
    // The next sample creates a new state that decodes in the content process.
    dropWaitingSamples(trackIdentifier);
    if (auto removedState = takeTrackState(trackIdentifier))
        releaseDecoder(*removedState);
    updateDecoderReadiness(trackIdentifier);
    sendFlushTrack(trackIdentifier);
    requestVideoReenqueue(trackIdentifier);
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::requestVideoReenqueue(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (!m_notifyWhenRequiresFlushToResumeCallback) {
        m_videoAwaitingKeyframe = true;
        return;
    }
    setVideoAwaitingFlush(trackIdentifier, true);
    m_notifyWhenRequiresFlushToResumeCallback();
}

void AudioVideoRendererRemote::setVideoAwaitingFlush(TrackIdentifier trackIdentifier, bool awaitingFlush)
{
    assertIsCurrent(queueSingleton());

    m_videoAwaitingFlush = awaitingFlush;
    Locker locker { m_lock };
    if (auto iterator = m_readyForMoreDataStates.find(trackIdentifier); iterator != m_readyForMoreDataStates.end())
        iterator->value.setAwaitingFlush(awaitingFlush);
}

void AudioVideoRendererRemote::resetVideoFallbackState()
{
    assertIsCurrent(queueSingleton());

    m_hardwareUnavailableVideoCodecs.clear();
    m_videoAwaitingKeyframe = false;
    if (m_videoTrackIdentifier)
        setVideoAwaitingFlush(*m_videoTrackIdentifier, false);
    else
        m_videoAwaitingFlush = false;
}

void AudioVideoRendererRemote::removePendingSamples(TrackIdentifier trackIdentifier, size_t count)
{
    assertIsCurrent(queueSingleton());

    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).removePendingSamples(count);
    }
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::sendSample(TrackIdentifier trackIdentifier, MediaSamplesBlock&& sample, std::optional<MediaTime> expectedMinimum)
{
    assertIsCurrent(queueSingleton());

    RefPtr gpuProcessConnection = activeConnection();
    if (!gpuProcessConnection)
        return;

    if (RefPtr info = sample.info()) {
        auto iterator = m_lastSentInfos.find(trackIdentifier);
        if (iterator == m_lastSentInfos.end() || iterator->value.ptr() != info.get()) {
            m_lastSentInfos.set(trackIdentifier, Ref { *info });
            gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::NewTrackInfoForTrack(m_identifier, trackIdentifier, *info), 0);
        }
    }
    sample.setInfo(nullptr);
    gpuProcessConnection->connection().sendWithAsyncReplyOnDispatcher(Messages::RemoteAudioVideoRendererProxyManager::EnqueueSample(m_identifier, trackIdentifier, WTF::move(sample), expectedMinimum), queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier](bool readyForMoreData) {
        RefPtr protectedThis { weakThis };
        if (!protectedThis)
            return;
        auto pendingSampleCount = [&] {
            Locker locker { protectedThis->m_lock };
            auto& state = protectedThis->readyForMoreDataState(trackIdentifier);
            state.removePendingSamples();
            state.setRemoteReadyForMoreData(readyForMoreData);
            return state.pendingSampleCount();
        }();
        if (!pendingSampleCount && !readyForMoreData) {
            if (RefPtr gpuProcessConnection = protectedThis->activeConnection())
                gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::RequestMediaDataWhenReady(protectedThis->m_identifier, trackIdentifier), 0);
            return;
        }
        protectedThis->resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
    }, 0);
}

void AudioVideoRendererRemote::sendDecodedSamples(TrackIdentifier trackIdentifier, Vector<MediaSamplesBlock>&& samples)
{
    assertIsCurrent(queueSingleton());

    for (auto& sample : borrow(samples)) {
        if (sample.isEmpty() || !activeConnection())
            continue;
        {
            Locker locker { m_lock };
            readyForMoreDataState(trackIdentifier).addPendingSample();
        }
        sendSample(trackIdentifier, WTF::move(sample), std::nullopt);
    }
}

void AudioVideoRendererRemote::willFlushTrack(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    if (isVideoTrack(trackIdentifier) && m_videoAwaitingFlush)
        setVideoAwaitingFlush(trackIdentifier, false);
    dropWaitingSamples(trackIdentifier);
    auto* state = trackStateIfExists(trackIdentifier);
    if (!state)
        return;
    switch (state->mode) {
    case TrackState::Mode::Creating:
    case TrackState::Mode::Draining:
        if (auto removedState = takeTrackState(trackIdentifier))
            releaseDecoder(*removedState);
        break;
    case TrackState::Mode::Decoding:
        // Keep the decoder; the new generation discards the results of samples submitted before the flush.
        protect(state->decoder)->flush();
        replaceDecodingTrackState(trackIdentifier, state->info.copyRef(), state->isProtected, ++m_nextTrackStateGeneration);
        break;
    case TrackState::Mode::Compressed:
    case TrackState::Mode::Failed:
        break;
    }
    updateDecoderReadiness(trackIdentifier);
}

void AudioVideoRendererRemote::releaseTrackState(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    bool hasTrackState = !!trackStateIfExists(trackIdentifier);
    if (hasTrackState) {
        dropWaitingSamples(trackIdentifier);
        if (auto state = takeTrackState(trackIdentifier))
            releaseDecoder(*state);
    }
    if (isVideoTrack(trackIdentifier)) {
        resetVideoFallbackState();
        m_videoTrackIdentifier = std::nullopt;
    }
    if (!hasTrackState)
        return;
    m_lastSentInfos.remove(trackIdentifier);
    {
        Locker locker { m_lock };
        if (auto iterator = m_readyForMoreDataStates.find(trackIdentifier); iterator != m_readyForMoreDataStates.end())
            iterator->value.setDecoderReadyForMoreData(true);
    }
    resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
}

void AudioVideoRendererRemote::releaseAllDecoders()
{
    assertIsCurrent(queueSingleton());

    auto audioTrackStates = std::exchange(m_audioTrackStates, { });
    for (auto& state : audioTrackStates.values())
        releaseDecoder(state);
    if (auto videoTrackState = std::exchange(m_videoTrackState, nullptr))
        releaseDecoder(*videoTrackState);
    m_lastSentInfos.clear();
    resetVideoFallbackState();
    Locker locker { m_lock };
    for (auto& readyState : m_readyForMoreDataStates.values())
        readyState.setDecoderReadyForMoreData(true);
}

void AudioVideoRendererRemote::notifyDecodersOfTimeChange()
{
    ensureWeakOnDispatcher([](auto& renderer) {
        assertIsCurrent(queueSingleton());
        renderer.forEachTrackState([](auto& state) {
            if (RefPtr decoder = state.decoder)
                decoder->currentTimeDidChange();
        });
    });
}

void AudioVideoRendererRemote::updateDecoderReadiness(TrackIdentifier trackIdentifier)
{
    assertIsCurrent(queueSingleton());

    auto* state = trackStateIfExists(trackIdentifier);
    RefPtr decoder = state && state->mode == TrackState::Mode::Decoding ? state->decoder : nullptr;
    bool isReady = !decoder || decoder->isReadyForMoreMediaData();
    {
        Locker locker { m_lock };
        readyForMoreDataState(trackIdentifier).setDecoderReadyForMoreData(isReady);
    }
    if (isReady || state->decoderAwaitingReadiness.get() == decoder)
        return;

    state->decoderAwaitingReadiness = decoder;
    decoder->requestMediaDataWhenReady()->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackIdentifier, weakDecoder = ThreadSafeWeakPtr { *decoder }](auto&&) {
        RefPtr protectedThis { weakThis };
        RefPtr decoder { weakDecoder };
        if (!protectedThis || !decoder)
            return;
        assertIsCurrent(queueSingleton());
        auto* state = protectedThis->trackStateIfExists(trackIdentifier);
        if (!state || state->decoderAwaitingReadiness.get() != decoder)
            return;
        state->decoderAwaitingReadiness = nullptr;
        protectedThis->updateDecoderReadiness(trackIdentifier);
        protectedThis->resolveRequestMediaDataWhenReadyIfNeeded(trackIdentifier);
    });
}

bool AudioVideoRendererRemote::isReadyForMoreSamples(TrackIdentifier trackIdentifier)
{
    Locker locker { m_lock };
    return readyForMoreDataState(trackIdentifier).isReadyForMoreData();
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
    ensureWeakOnDispatcher([trackIdentifier, callback = WTF::move(callback)](auto& renderer) mutable {
        assertIsCurrent(queueSingleton());
        if (renderer.m_isShutdown)
            return;
        if (callback)
            renderer.m_trackNeedsReenqueuingCallbacks.set(trackIdentifier, WTF::move(callback));
        else
            renderer.m_trackNeedsReenqueuingCallbacks.remove(trackIdentifier);
    });
}

bool AudioVideoRendererRemote::timeIsProgressing() const
{
    Locker locker { m_lock };
    return m_sharedTimebaseReader && m_sharedTimebaseReader->currentRate();
}

void AudioVideoRendererRemote::notifyEffectiveRateChanged(Function<void(double)>&& callback)
{
    ensureWeakOnDispatcher([callback = WTF::move(callback)](auto& renderer) mutable {
        assertIsCurrent(queueSingleton());
        if (renderer.m_isShutdown)
            return;
        renderer.m_effectiveRateChangedCallback = WTF::move(callback);
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
    ensureWeakOnDispatcher([identifier](auto& renderer) {
        renderer.willFlushTrack(identifier);
        renderer.sendFlushTrack(identifier);
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
            protectedThis->readyForMoreDataState(identifier).setRemoteReadyForMoreData(isReadyForMoreMediaData);
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
    ensureWeakOnDispatcher([callback = WTF::move(callback)](auto& renderer) mutable {
        assertIsCurrent(queueSingleton());
        if (renderer.m_isShutdown) {
            callback(PlatformMediaError::Shutdown);
            return;
        }
        renderer.m_errorCallback = WTF::move(callback);
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

void AudioVideoRendererRemote::ensureWeakOnDispatcher(Function<void(AudioVideoRendererRemote&)>&& function)
{
    ensureOnDispatcher([weakThis = ThreadSafeWeakPtr { *this }, function = WTF::move(function)] {
        if (RefPtr protectedThis { weakThis })
            function(*protectedThis);
    });
}

void AudioVideoRendererRemote::ensureOnDispatcherWithConnection(Function<void(AudioVideoRendererRemote&, IPC::Connection&)>&& function)
{
    ensureWeakOnDispatcher([function = WTF::move(function)](auto& renderer) {
        if (RefPtr gpuProcessConnection = renderer.activeConnection())
            function(renderer, gpuProcessConnection->connection());
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
    ensureWeakOnDispatcher([callback = WTF::move(callback)](auto& renderer) mutable {
        assertIsCurrent(queueSingleton());
        if (renderer.m_isShutdown)
            return;
        renderer.m_videoLayerSizeChangedCallback = WTF::move(callback);
    });
}

void AudioVideoRendererRemote::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection.get() == &connection);
    m_gpuProcessConnectionShutDown = true;
    // The destructor skips this cleanup once the connection is shut down, so it must run even if the renderer is destroyed first.
    ensureOnDispatcher([connection = Ref { connection }, identifier = m_identifier] {
        connection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Destroy(identifier), 0);
        connection->connection().removeWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), identifier.toUInt64());
    });
    ensureWeakOnDispatcher([](auto& renderer) {
        assertIsCurrent(queueSingleton());
        renderer.releaseAllDecoders();
        if (renderer.m_errorCallback)
            renderer.m_errorCallback(PlatformMediaError::IPCError);
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
            parent->readyForMoreDataState(trackIdentifier).setRemoteReadyForMoreData(true);
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
