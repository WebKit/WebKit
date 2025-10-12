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
#include "RemoteVideoFrameProxy.h"
#include "RemoteVideoFrameProxyProperties.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/HostingContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaSamplesBlock.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
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

WorkQueue& AudioVideoRendererRemote::queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("AudioVideoRendererRemote"_s));
    });
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

    connection.connection().send(Messages::RemoteAudioVideoRendererProxyManager::Create(identifier, mediaElementIdentifier, playerIdentifier), 0);
}

AudioVideoRendererRemote::~AudioVideoRendererRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if PLATFORM(COCOA)
    m_videoLayerManager->didDestroyVideoLayer();
#endif

    if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get(); gpuProcessConnection && !m_shutdown) {
        gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Shutdown(m_identifier), 0);
        gpuProcessConnection->connection().removeWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), m_identifier.toUInt64());
    }

    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request({ });
}

void AudioVideoRendererRemote::setVolume(float volume)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetVolume(m_identifier, volume), 0);
}

void AudioVideoRendererRemote::setMuted(bool muted)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetMuted(m_identifier, muted), 0);
}

void AudioVideoRendererRemote::setPreservesPitchAndCorrectionAlgorithm(bool preservesPitch, std::optional<PitchCorrectionAlgorithm> algorithm)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetPreservesPitchAndCorrectionAlgorithm(m_identifier, preservesPitch, algorithm), 0);
}

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
void AudioVideoRendererRemote::setOutputDeviceId(const String& deviceId)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetOutputDeviceId(m_identifier, deviceId), 0);
}
#endif

void AudioVideoRendererRemote::setIsVisible(bool visible)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetIsVisible(m_identifier, visible), 0);
}

void AudioVideoRendererRemote::setPresentationSize(const IntSize& size)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetPresentationSize(m_identifier, size), 0);
}

void AudioVideoRendererRemote::setShouldMaintainAspectRatio(bool maintain)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetShouldMaintainAspectRatio(m_identifier, maintain), 0);
}

void AudioVideoRendererRemote::acceleratedRenderingStateChanged(bool acceleratedRendering)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::AcceleratedRenderingStateChanged(m_identifier, acceleratedRendering), 0);
}

void AudioVideoRendererRemote::contentBoxRectChanged(const LayoutRect& rect)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::ContentBoxRectChanged(m_identifier, rect), 0);
}

void AudioVideoRendererRemote::notifyFirstFrameAvailable(Function<void()>&& callback)
{
    m_firstFrameAvailableCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&& callback)
{
    m_hasAvailableVideoFrameCallback = WTFMove(callback);
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::NotifyWhenHasAvailableVideoFrame(m_identifier, !!m_hasAvailableVideoFrameCallback), 0);
}

void AudioVideoRendererRemote::notifyWhenRequiresFlushToResume(Function<void()>&& callback)
{
    m_notifyWhenRequiresFlushToResumeCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::notifyRenderingModeChanged(Function<void()>&& callback)
{
    m_renderingModeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::expectMinimumUpcomingPresentationTime(const MediaTime& minimum)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::ExpectMinimumUpcomingPresentationTime(m_identifier, minimum), 0);
}

void AudioVideoRendererRemote::notifySizeChanged(Function<void(const MediaTime&, FloatSize)>&& callback)
{
    m_sizeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::setShouldDisableHDR(bool disable)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetShouldDisableHDR(m_identifier, disable), 0);
}

void AudioVideoRendererRemote::setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit& limit)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetPlatformDynamicRangeLimit(m_identifier, limit), 0);
}

void AudioVideoRendererRemote::setResourceOwner(const ProcessIdentity& processIdentity)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;


    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetResourceOwner(m_identifier, ProcessIdentity { processIdentity }), 0);
}

void AudioVideoRendererRemote::flushAndRemoveImage()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::FlushAndRemoveImage(m_identifier), 0);
}

RefPtr<VideoFrame> AudioVideoRendererRemote::currentVideoFrame() const
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return nullptr;

    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteAudioVideoRendererProxyManager::CurrentVideoFrame(m_identifier), 0);
    if (!sendResult.succeeded())
        return nullptr;

    auto [result] = sendResult.takeReply();
    if (result)
        return RemoteVideoFrameProxy::create(gpuProcessConnection->connection(), gpuProcessConnection->protectedVideoFrameObjectHeapProxy(), WTFMove(*result));
    return nullptr;
}

void AudioVideoRendererRemote::paintCurrentVideoFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    if (RefPtr videoFrame = currentVideoFrame())
        context.drawVideoFrame(*videoFrame, rect, ImageOrientation::Orientation::None, false);
}

RefPtr<NativeImage> AudioVideoRendererRemote::currentNativeImage() const
{
#if PLATFORM(COCOA)
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    RefPtr videoFrame = currentVideoFrame();
    if (!videoFrame)
        return nullptr;
    ASSERT(gpuProcessConnection);

    return gpuProcessConnection->protectedVideoFrameObjectHeapProxy()->getNativeImage(*videoFrame);
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

std::optional<VideoPlaybackQualityMetrics> AudioVideoRendererRemote::videoPlaybackQualityMetrics()
{
    return m_state.videoPlaybackQualityMetrics;
}

PlatformLayer* AudioVideoRendererRemote::platformVideoLayer() const
{
#if PLATFORM(COCOA)
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
#if PLATFORM(COCOA)
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), nullptr);
#endif
}

void AudioVideoRendererRemote::setVideoFullscreenFrame(const FloatRect& frame)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::FlushAndRemoveImage(m_identifier), 0);

}

void AudioVideoRendererRemote::isInFullscreenOrPictureInPictureChanged(bool inFullscreen)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::IsInFullscreenOrPictureInPictureChanged(m_identifier, inFullscreen), 0);
}
#endif

void AudioVideoRendererRemote::play(std::optional<MonotonicTime> hostTime)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    m_state.paused = false;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Play(m_identifier, hostTime), 0);
}

void AudioVideoRendererRemote::pause(std::optional<MonotonicTime> hostTime)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    m_state.paused = true;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Pause(m_identifier, hostTime), 0);
}

bool AudioVideoRendererRemote::paused() const
{
    return m_state.paused;
}

void AudioVideoRendererRemote::setRate(double rate)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetRate(m_identifier, rate), 0);
}

double AudioVideoRendererRemote::effectiveRate() const
{
    return m_state.effectiveRate;
}

void AudioVideoRendererRemote::stall()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    m_state.effectiveRate = 0;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Stall(m_identifier), 0);
}

void AudioVideoRendererRemote::prepareToSeek()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::PrepareToSeek(m_identifier), 0);
}

Ref<MediaTimePromise> AudioVideoRendererRemote::seekTo(const MediaTime& time)
{
    return invokeAsync(RunLoop::mainSingleton(), [protectedThis = Ref { *this }, this, time] {
        RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
        if (!isGPURunning() || !gpuProcessConnection)
            return MediaTimePromise::createAndReject(PlatformMediaError::Cancelled);

        m_state.currentTime = time;
        return gpuProcessConnection->connection().sendWithPromisedReply<MediaPromiseConverter>(Messages::RemoteAudioVideoRendererProxyManager::SeekTo(m_identifier, time), 0);
    });
}

bool AudioVideoRendererRemote::seeking() const
{
    return m_state.seeking;
}

void AudioVideoRendererRemote::setPreferences(VideoRendererPreferences preferences)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetPreferences(m_identifier, preferences), 0);
}

void AudioVideoRendererRemote::setHasProtectedVideoContent(bool isProtected)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetHasProtectedVideoContent(m_identifier, isProtected), 0);
}

AudioVideoRendererRemote::TrackIdentifier AudioVideoRendererRemote::addTrack(TrackType type)
{
    // the sendSync() call requires us to run on the connection's dispatcher, which is the main thread.
    assertIsMainThread();
    // FIXME: Uses a new Connection for remote playback, and not the main GPUProcessConnection's one.
    // FIXME: m_mimeTypeCache is a main-thread only object.
    auto sendResult = m_gpuProcessConnection.get()->connection().sendSync(Messages::RemoteAudioVideoRendererProxyManager::AddTrack(m_identifier, type), 0);
    auto result = std::get<0>(sendResult.takeReplyOr(makeUnexpected(PlatformMediaError::IPCError)));
    ASSERT(!!result);
    return *result;
}

void AudioVideoRendererRemote::removeTrack(TrackIdentifier trackIdentifier)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::RemoveTrack(m_identifier, trackIdentifier), 0);
}

void AudioVideoRendererRemote::enqueueSample(TrackIdentifier trackIdentifier, Ref<MediaSample>&& sample, std::optional<MediaTime> expectedMinimum)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::EnqueueSample(m_identifier, trackIdentifier, MediaSamplesBlock::fromMediaSample(sample), expectedMinimum), 0);
    if (auto it = m_requestMediaDataWhenReadyData.find(trackIdentifier); it != m_requestMediaDataWhenReadyData.end())
        it->value.pendingSamples++;
}

bool AudioVideoRendererRemote::isReadyForMoreSamples(TrackIdentifier trackIdentifier)
{
    auto it = m_requestMediaDataWhenReadyData.find(trackIdentifier);
    return it != m_requestMediaDataWhenReadyData.end() && it->value.readyForMoreData();
}

void AudioVideoRendererRemote::requestMediaDataWhenReady(TrackIdentifier trackIdentifier, Function<void(TrackIdentifier)>&& callback)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    if (auto it = m_requestMediaDataWhenReadyData.find(trackIdentifier); it != m_requestMediaDataWhenReadyData.end())
        it->value.callback = WTFMove(callback);
    else {
        RequestMediaDataWhenReadyData data { .callback = WTFMove(callback) };
        m_requestMediaDataWhenReadyData.set(trackIdentifier, WTFMove(data));
    }
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::RequestMediaDataWhenReady(m_identifier, trackIdentifier), 0);
}

void AudioVideoRendererRemote::stopRequestingMediaData(TrackIdentifier trackIdentifier)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    if (auto it = m_requestMediaDataWhenReadyData.find(trackIdentifier); it != m_requestMediaDataWhenReadyData.end())
        it->value.callback = nullptr;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::StopRequestingMediaData(m_identifier, trackIdentifier), 0);
}

void AudioVideoRendererRemote::notifyTrackNeedsReenqueuing(TrackIdentifier trackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>&& callback)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    if (callback)
        m_trackNeedsReenqueuingCallbacks.set(trackIdentifier, WTFMove(callback));
    else
        m_trackNeedsReenqueuingCallbacks.remove(trackIdentifier);
}

bool AudioVideoRendererRemote::timeIsProgressing() const
{
    return m_state.timeIsProgressing;
}

void AudioVideoRendererRemote::notifyEffectiveRateChanged(Function<void(double)>&& callback)
{
    m_effectiveRateChangedCallback = WTFMove(callback);
}

MediaTime AudioVideoRendererRemote::currentTime() const
{
    return m_state.currentTime;
}

void AudioVideoRendererRemote::notifyTimeReachedAndStall(const MediaTime& time, Function<void(const MediaTime&)>&& callback)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    m_timeReachedAndStallCallback = WTFMove(callback);
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::NotifyTimeReachedAndStall(m_identifier, time), 0);
}

void AudioVideoRendererRemote::cancelTimeReachedAction()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    m_timeReachedAndStallCallback = { };
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::CancelTimeReachedAction(m_identifier), 0);
}

void AudioVideoRendererRemote::performTaskAtTime(const MediaTime& time, Function<void(const MediaTime&)>&& callback)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;
    m_performTaskAtTimeCallback = WTFMove(callback);
    m_performTaskAtTime = time;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::PerformTaskAtTime(m_identifier, time), 0);
}

void AudioVideoRendererRemote::flush()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::Flush(m_identifier), 0);
}

void AudioVideoRendererRemote::flushTrack(TrackIdentifier identifier)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::FlushTrack(m_identifier, identifier), 0);
}

void AudioVideoRendererRemote::applicationWillResignActive()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::ApplicationWillResignActive(m_identifier), 0);
}

void AudioVideoRendererRemote::notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&& callback)
{
    m_errorCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::setSpatialTrackingInfo(bool prefersSpatialAudioExperience , SoundStageSize stage, const String& sceneIdentifier, const String& defaultLabel, const String& label)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetSpatialTrackingInfo(m_identifier, prefersSpatialAudioExperience, stage, sceneIdentifier, defaultLabel, label), 0);
}

void AudioVideoRendererRemote::ensureOnDispatcherSync(Function<void()>&& function)
{
    callOnMainRunLoopAndWait(WTFMove(function));
}

void AudioVideoRendererRemote::ensureOnDispatcher(Function<void()>&& function)
{
    ensureOnMainThread(WTFMove(function));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioVideoRendererRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

void AudioVideoRendererRemote::updateCacheState(const RemoteAudioVideoRendererState& state)
{
    m_state = state;
}

void AudioVideoRendererRemote::requestHostingContext(LayerHostingContextCallback&& completionHandler)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection) {
        completionHandler({ });
        return;
    }

    if (m_layerHostingContext.contextID) {
        completionHandler(m_layerHostingContext);
        return;
    }

    m_layerHostingContextRequests.append(WTFMove(completionHandler));
    gpuProcessConnection->connection().sendWithAsyncReply(Messages::RemoteAudioVideoRendererProxyManager::RequestHostingContext(m_identifier), [weakThis = ThreadSafeWeakPtr { *this }] (WebCore::HostingContext context) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setLayerHostingContext(WTFMove(context));
    }, m_identifier);
}

WebCore::HostingContext AudioVideoRendererRemote::hostingContext() const
{
    return m_layerHostingContext;
}

void AudioVideoRendererRemote::setLayerHostingContext(WebCore::HostingContext&& hostingContext)
{
    if (m_layerHostingContext.contextID == hostingContext.contextID)
        return;

    m_layerHostingContext = WTFMove(hostingContext);
#if PLATFORM(COCOA)
    m_videoLayer = nullptr;
#endif

    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request(m_layerHostingContext);
}

bool AudioVideoRendererRemote::inVideoFullscreenOrPictureInPicture() const
{
#if PLATFORM(COCOA) && ENABLE(VIDEO_PRESENTATION_MODE)
    return !!m_videoLayerManager->videoFullscreenLayer();
#else
    return false;
#endif
}

#if PLATFORM(COCOA)
void AudioVideoRendererRemote::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!isGPURunning() || !gpuProcessConnection)
        return;

    m_videoLayerSize = size;
    gpuProcessConnection->connection().send(Messages::RemoteAudioVideoRendererProxyManager::SetVideoLayerSizeFenced(m_identifier, size, WTFMove(sendRightAnnotated)), m_identifier);
}
#endif

void AudioVideoRendererRemote::notifyVideoLayerSizeChanged(Function<void(const MediaTime&, FloatSize)>&& callback)
{
    m_videoLayerSizeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererRemote::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection.get() == &connection);
    m_shutdown = true;
    connection.connection().send(Messages::RemoteAudioVideoRendererProxyManager::Shutdown(m_identifier), 0);
    connection.connection().removeWorkQueueMessageReceiver(Messages::AudioVideoRendererRemoteMessageReceiver::messageReceiverName(), m_identifier.toUInt64());
    if (m_errorCallback)
        m_errorCallback(PlatformMediaError::IPCError);
}

AudioVideoRendererRemote::MessageReceiver::MessageReceiver(AudioVideoRendererRemote& parent)
    : m_parent(parent)
{
}

void AudioVideoRendererRemote::MessageReceiver::firstFrameAvailable(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state] {
            parent->updateCacheState(state);
            if (parent->m_firstFrameAvailableCallback)
                parent->m_firstFrameAvailableCallback();
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::hasAvailableVideoFrame(MediaTime time, double clockTime, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, time, clockTime, state] {
            parent->updateCacheState(state);
            if (parent->m_hasAvailableVideoFrameCallback)
                parent->m_hasAvailableVideoFrameCallback(time, clockTime);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::requiresFlushToResume(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state] {
            parent->updateCacheState(state);
            if (parent->m_notifyWhenRequiresFlushToResumeCallback)
                parent->m_notifyWhenRequiresFlushToResumeCallback();
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::renderingModeChanged(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state] {
            parent->updateCacheState(state);
            if (parent->m_renderingModeChangedCallback)
                parent->m_renderingModeChangedCallback();
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::sizeChanged(MediaTime time, WebCore::FloatSize size, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, time, size, state] {
            parent->updateCacheState(state);
            parent->m_naturalSize = size;
            if (parent->m_sizeChangedCallback)
                parent->m_sizeChangedCallback(time, size);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::trackNeedsReenqueuing(TrackIdentifier trackIdentifier, MediaTime time, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, trackIdentifier, time, state] {
            parent->updateCacheState(state);
            auto iterator = parent->m_trackNeedsReenqueuingCallbacks.find(trackIdentifier);
            if (iterator == parent->m_trackNeedsReenqueuingCallbacks.end() || !iterator->value)
                return;
            iterator->value(trackIdentifier, time);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::effectiveRateChanged(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state] {
            parent->updateCacheState(state);
            if (parent->m_effectiveRateChangedCallback)
                parent->m_effectiveRateChangedCallback(parent->m_state.effectiveRate);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::stallTimeReached(MediaTime time, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, time, state] {
            parent->updateCacheState(state);
            if (parent->m_timeReachedAndStallCallback)
                parent->m_timeReachedAndStallCallback(time);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::taskTimeReached(MediaTime time, RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, time, state] {
            parent->updateCacheState(state);
            if (parent->m_performTaskAtTimeCallback && time == parent->m_performTaskAtTime)
                parent->m_performTaskAtTimeCallback(time);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::errorOccurred(WebCore::PlatformMediaError error)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, error] {
            if (parent->m_errorCallback)
                parent->m_errorCallback(error);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::requestMediaDataWhenReady(TrackIdentifier trackIdentifier)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, trackIdentifier] {
            auto iterator = parent->m_requestMediaDataWhenReadyData.find(trackIdentifier);
            if (iterator == parent->m_requestMediaDataWhenReadyData.end() || !iterator->value.callback)
                return;
            iterator->value.pendingSamples = 0;
            iterator->value.callback(trackIdentifier);
        });
    }
}

void AudioVideoRendererRemote::MessageReceiver::stateUpdate(RemoteAudioVideoRendererState state)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state] {
            parent->updateCacheState(state);
        });
    }
}

#if PLATFORM(COCOA)
void AudioVideoRendererRemote::MessageReceiver::layerHostingContextChanged(RemoteAudioVideoRendererState state, WebCore::HostingContext&& inlineLayerHostingContext, const WebCore::FloatSize& videoLayerSize)
{
    if (RefPtr parent = m_parent.get()) {
        parent->ensureOnDispatcher([parent, state, hostingContext = WTFMove(inlineLayerHostingContext), videoLayerSize]() mutable {
            if (!hostingContext.contextID) {
                parent->m_videoLayer = nullptr;
                parent->m_videoLayerManager->didDestroyVideoLayer();
                return;
            }
            parent->m_videoLayerSize = videoLayerSize;
            parent->updateCacheState(state);
            parent->setLayerHostingContext(WTFMove(hostingContext));
            if (parent->m_videoLayerSizeChangedCallback)
                parent->m_videoLayerSizeChangedCallback(state.currentTime, videoLayerSize);
        });
    }
}
#endif

} // namespace WebKit

#endif
