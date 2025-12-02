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
class CDMInstance;
class LegacyCDMSession;
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
        void readyForMoreMediaData(WebCore::SamplesRendererTrackIdentifier);
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
    void renderingCanBeAcceleratedChanged(bool) final;
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
    Ref<RequestPromise> requestMediaDataWhenReady(TrackIdentifier) final;
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
    WebCore::FloatSize videoLayerSize() const;
    void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&) final;
#endif
    void notifyVideoLayerSizeChanged(Function<void(const MediaTime&, WebCore::FloatSize)>&& callback) final;
    // VideoLayerRemoteParent
    bool inVideoFullscreenOrPictureInPicture() const final;
    WebCore::FloatSize naturalSize() const final;

#if ENABLE(ENCRYPTED_MEDIA)
    void setCDMInstance(WebCore::CDMInstance*) final;
    Ref<WebCore::MediaPromise> setInitData(Ref<WebCore::SharedBuffer>) final;
    void attemptToDecrypt() final;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setCDMSession(WebCore::LegacyCDMSession*) final;
#endif

    // Logger
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const final { return "AudioVideoRendererRemote"_s; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
#endif

    void setState(RemoteAudioVideoRendererState);

    void ensureOnDispatcher(Function<void()>&&);
    void ensureOnDispatcherSync(NOESCAPE Function<void()>&&);
    void ensureOnDispatcherWithConnection(Function<void(AudioVideoRendererRemote&, IPC::Connection&)>&&);
    static WorkQueue& queueSingleton();
    bool isGPURunning() const { return !m_shutdown; }

    void updateCacheState(const RemoteAudioVideoRendererState&);
    class ReadyForMoreDataState {
    public:
        static constexpr size_t kMaxPendingSample = 20;
        bool isReadyForMoreData() const { return m_pendingSamples < kMaxPendingSample && m_remoteReadyForMoreData; }
        void setRemoteReadyForMoreData(bool ready) { m_remoteReadyForMoreData = ready; }
        void sampleEnqueued() { m_pendingSamples++; }
        void sampleReceived() { m_pendingSamples--; }
        size_t pendingSamples() const { return m_pendingSamples; }
    private:
        size_t m_pendingSamples { 0 };
        bool m_remoteReadyForMoreData { true };
    };
    ReadyForMoreDataState& readyForMoreDataState(TrackIdentifier);
    void resolveRequestMediaDataWhenReadyIfNeeded(TrackIdentifier);

    const ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    const Ref<MessageReceiver> m_receiver;
    const RemoteAudioVideoRendererIdentifier m_identifier;

    std::atomic<bool> m_shutdown { false };

    mutable Lock m_lock;
    RemoteAudioVideoRendererState m_state WTF_GUARDED_BY_LOCK(m_lock);

    Function<void(WebCore::PlatformMediaError)> m_errorCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void()> m_firstFrameAvailableCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&, double)> m_hasAvailableVideoFrameCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void()> m_notifyWhenRequiresFlushToResumeCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void()> m_renderingModeChangedCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&, WebCore::FloatSize)> m_sizeChangedCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&)> m_currentTimeDidChangeCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(double)> m_effectiveRateChangedCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&)> m_timeReachedAndStallCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&)> m_performTaskAtTimeCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    MediaTime m_performTaskAtTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Function<void(const MediaTime&, WebCore::FloatSize)> m_videoLayerSizeChangedCallback WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    HashMap<TrackIdentifier, ReadyForMoreDataState> m_readyForMoreDataStates WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<TrackIdentifier, std::unique_ptr<RequestPromise::AutoRejectProducer>> m_requestMediaDataWhenReadyDataPromises WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    HashMap<TrackIdentifier, Function<void(TrackIdentifier, const MediaTime&)>> m_trackNeedsReenqueuingCallbacks WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    Vector<LayerHostingContextCallback> m_layerHostingContextRequests WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    WebCore::HostingContext m_layerHostingContext WTF_GUARDED_BY_LOCK(m_lock);
    WebCore::FloatSize m_naturalSize WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_seeking { false };
    MediaTime m_lastSeekTime; // Always call on the renderer's client thread.
#if PLATFORM(COCOA)
    const UniqueRef<WebCore::VideoLayerManager> m_videoLayerManager WTF_GUARDED_BY_LOCK(m_lock);
    mutable PlatformLayerContainer m_videoLayer WTF_GUARDED_BY_LOCK(m_lock);
    WebCore::FloatSize m_videoLayerSize WTF_GUARDED_BY_LOCK(m_lock);
#endif
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

}

#endif
