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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "GPUProcessConnection.h"
#include "LayerHostingContext.h"
#include "RemoteAudioVideoRendererIdentifier.h"
#include "RemoteAudioVideoRendererState.h"
#include "VideoLayerRemote.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/AudioVideoRenderer.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
struct HostingContext;
class VideoLayerManager;
}

namespace WebKit {

class AudioVideoRendererRemote final
    : public WebCore::AudioVideoRenderer
    , public VideoLayerRemoteParent
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    , public GPUProcessConnection::Client
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioVideoRendererRemote> {
public:
    static Ref<AudioVideoRendererRemote> create(LoggerHelper*, WebCore::HTMLMediaElementIdentifier, WebCore::MediaPlayerIdentifier, GPUProcessConnection&);
    ~AudioVideoRendererRemote();

    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    class MessageReceiver final : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
    public:
        static Ref<MessageReceiver> create(AudioVideoRendererRemote& parent)
        {
            return adoptRef(*new MessageReceiver(parent));
        }

    private:
        MessageReceiver(AudioVideoRendererRemote&);
        void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

        void firstFrameAvailable(RemoteAudioVideoRendererState);
        void hasAvailableVideoFrame(MediaTime, double, RemoteAudioVideoRendererState);
        void requiresFlushToResume(RemoteAudioVideoRendererState);
        void renderingModeChanged(RemoteAudioVideoRendererState);
        void sizeChanged(MediaTime, WebCore::FloatSize, RemoteAudioVideoRendererState);
        void trackNeedsReenqueuing(WebCore::SamplesRendererTrackIdentifier, MediaTime, RemoteAudioVideoRendererState);
        void effectiveRateChanged(RemoteAudioVideoRendererState);
        void stallTimeReached(MediaTime, RemoteAudioVideoRendererState);
        void taskTimeReached(MediaTime, RemoteAudioVideoRendererState);
        void errorOccurred(WebCore::PlatformMediaError);
        void requestMediaDataWhenReady(WebCore::SamplesRendererTrackIdentifier);
        void stateUpdate(RemoteAudioVideoRendererState);

#if PLATFORM(COCOA)
        void layerHostingContextChanged(RemoteAudioVideoRendererState, WebCore::HostingContext&&, const WebCore::FloatSize&);
#endif
        ThreadSafeWeakPtr<AudioVideoRendererRemote> m_parent;
    };

private:
    friend class MessageReceiver;
    AudioVideoRendererRemote(LoggerHelper*, GPUProcessConnection&, WebCore::HTMLMediaElementIdentifier, WebCore::MediaPlayerIdentifier, RemoteAudioVideoRendererIdentifier);

    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    void setVolume(float) final;
    void setMuted(bool) final;
    void setPreservesPitchAndCorrectionAlgorithm(bool, std::optional<PitchCorrectionAlgorithm>) final;
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    void setOutputDeviceId(const String&) final;
#endif

    void setIsVisible(bool) final;
    void setPresentationSize(const WebCore::IntSize&) final;
    void setShouldMaintainAspectRatio(bool) final;
    void acceleratedRenderingStateChanged(bool) final;
    void contentBoxRectChanged(const WebCore::LayoutRect&) final;
    void notifyFirstFrameAvailable(Function<void()>&&) final;
    void notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&&) final;
    void notifyWhenRequiresFlushToResume(Function<void()>&&) final;
    void notifyRenderingModeChanged(Function<void()>&&) final;
    void expectMinimumUpcomingPresentationTime(const MediaTime&) final;
    void notifySizeChanged(Function<void(const MediaTime&, WebCore::FloatSize)>&&) final;
    void setShouldDisableHDR(bool) final;
    void setPlatformDynamicRangeLimit(const WebCore::PlatformDynamicRangeLimit&) final;
    void setResourceOwner(const WebCore::ProcessIdentity&) final;
    void flushAndRemoveImage() final;
    RefPtr<WebCore::VideoFrame> currentVideoFrame() const final;
    void paintCurrentVideoFrameInContext(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    RefPtr<WebCore::NativeImage> currentNativeImage() const final;
    std::optional<WebCore::VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    PlatformLayer* platformVideoLayer() const final;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&&) final;
    void setVideoFullscreenFrame(const WebCore::FloatRect&) final;
    void isInFullscreenOrPictureInPictureChanged(bool) final;
#endif

    void play(std::optional<MonotonicTime>) final;
    void pause(std::optional<MonotonicTime>) final;
    bool paused() const final;
    void setRate(double) final;
    double effectiveRate() const final;
    void stall() final;
    void prepareToSeek() final;
    Ref<WebCore::MediaTimePromise> seekTo(const MediaTime&) final;
    bool seeking() const final;

    void setPreferences(WebCore::VideoRendererPreferences) final;
    void setHasProtectedVideoContent(bool) final;

    TrackIdentifier addTrack(TrackType) final;
    void removeTrack(TrackIdentifier) final;

    void enqueueSample(TrackIdentifier, Ref<WebCore::MediaSample>&&, std::optional<MediaTime>) final;
    bool isReadyForMoreSamples(TrackIdentifier) final;
    void requestMediaDataWhenReady(TrackIdentifier, Function<void(TrackIdentifier)>&&) final;
    void stopRequestingMediaData(TrackIdentifier) final;
    void notifyTrackNeedsReenqueuing(TrackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>&&) final;

    bool timeIsProgressing() const final;
    void notifyEffectiveRateChanged(Function<void(double)>&&) final;
    MediaTime currentTime() const final;
    void notifyTimeReachedAndStall(const MediaTime&, Function<void(const MediaTime&)>&&) final;
    void cancelTimeReachedAction() final;
    void performTaskAtTime(const MediaTime&, Function<void(const MediaTime&)>&&) final;

    void flush() final;
    void flushTrack(TrackIdentifier) final;

    void applicationWillResignActive() final;

    void notifyWhenErrorOccurs(Function<void(WebCore::PlatformMediaError)>&&) final;

    using SoundStageSize = WebCore::MediaPlayerSoundStageSize;
    void setSpatialTrackingInfo(bool, SoundStageSize, const String&, const String&, const String&) final;

    // Remote Layers
    using LayerHostingContextCallback = CompletionHandler<void(WebCore::HostingContext)>;
    void requestHostingContext(LayerHostingContextCallback&&) final;
    WebCore::HostingContext hostingContext() const final;
    void setLayerHostingContext(WebCore::HostingContext&&);
#if PLATFORM(COCOA)
    WebCore::FloatSize videoLayerSize() const { return m_videoLayerSize; };
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&) final;
#endif
    void notifyVideoLayerSizeChanged(Function<void(const MediaTime&, WebCore::FloatSize)>&& callback) final;
    // VideoLayerRemoteParent
    bool inVideoFullscreenOrPictureInPicture() const final;
    WebCore::FloatSize naturalSize() const final { return m_naturalSize; }

    // Logger
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const final { return "AudioVideoRendererRemote"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
#endif

    void ensureOnDispatcher(Function<void()>&&);
    void ensureOnDispatcherSync(Function<void()>&&);
    static WorkQueue& queueSingleton();
    bool isGPURunning() const { return !m_shutdown; }

    void updateCacheState(const RemoteAudioVideoRendererState&);

    const ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    const Ref<MessageReceiver> m_receiver;
    RemoteAudioVideoRendererIdentifier m_identifier;

    bool m_shutdown { false };

    RemoteAudioVideoRendererState m_state;

    Function<void(WebCore::PlatformMediaError)> m_errorCallback;
    Function<void()> m_firstFrameAvailableCallback;
    Function<void(const MediaTime&, double)> m_hasAvailableVideoFrameCallback;
    Function<void()> m_notifyWhenRequiresFlushToResumeCallback;
    Function<void()> m_renderingModeChangedCallback;
    Function<void(const MediaTime&, WebCore::FloatSize)> m_sizeChangedCallback;
    Function<void(const MediaTime&)> m_currentTimeDidChangeCallback;
    Function<void(double)> m_effectiveRateChangedCallback;
    Function<void(const MediaTime&)> m_timeReachedAndStallCallback;
    Function<void(const MediaTime&)> m_performTaskAtTimeCallback;
    MediaTime m_performTaskAtTime;
    Function<void(const MediaTime&, WebCore::FloatSize)> m_videoLayerSizeChangedCallback;

    static constexpr size_t kMaxPendingSample = 10;
    struct RequestMediaDataWhenReadyData {
        bool readyForMoreData() const { return pendingSamples < kMaxPendingSample; }
        size_t pendingSamples { kMaxPendingSample };
        Function<void(TrackIdentifier)> callback;
    };
    HashMap<TrackIdentifier, RequestMediaDataWhenReadyData> m_requestMediaDataWhenReadyData;
    HashMap<TrackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>> m_trackNeedsReenqueuingCallbacks;

    Vector<LayerHostingContextCallback> m_layerHostingContextRequests;
    WebCore::HostingContext m_layerHostingContext;
    WebCore::FloatSize m_naturalSize;
#if PLATFORM(COCOA)
    const UniqueRef<WebCore::VideoLayerManager> m_videoLayerManager;
    mutable PlatformLayerContainer m_videoLayer;
    WebCore::FloatSize m_videoLayerSize;
#endif
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

}

#endif
