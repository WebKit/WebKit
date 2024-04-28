/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include "SourceBufferParserWebM.h"
#include "TimeRanges.h"
#include "VideoFrameMetadata.h"
#include "WebAVSampleBufferListener.h"
#include "WebMResourceClient.h"
#include <wtf/HashFunctions.h>
#include <wtf/LoggerHelper.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;

typedef struct __CVBuffer *CVPixelBufferRef;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioTrackPrivateWebM;
class FragmentedSharedBuffer;
class MediaDescription;
class MediaPlaybackTarget;
class MediaSample;
class MediaSampleAVFObjC;
class PixelBufferConformerCV;
class ResourceError;
class SharedBuffer;
class TextTrackRepresentation;
class TrackBuffer;
class VideoFrame;
class VideoMediaSampleRenderer;
class VideoLayerManagerObjC;
class VideoTrackPrivateWebM;
class WebCoreDecompressionSession;

class MediaPlayerPrivateWebM
    : public MediaPlayerPrivateInterface
    , public WebMResourceClientParent
    , public WebAVSampleBufferListenerClient
    , private LoggerHelper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateWebM(MediaPlayer*);
    ~MediaPlayerPrivateWebM();

    void ref() final { WebMResourceClientParent::ref(); }
    void deref() final { WebMResourceClientParent::deref(); }

    static void registerMediaEngine(MediaEngineRegistrar);
private:
    void load(const String&) final;

#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    
    // WebMResourceClientParent
    friend class WebMResourceClient;
    void dataReceived(const SharedBuffer&) final;
    void loadFailed(const ResourceError&) final;
    void loadFinished() final;

    void cancelLoad() final;

    PlatformLayer* platformLayer() const final;

    bool supportsPictureInPicture() const final { return true; }
    bool supportsFullscreen() const final { return true; }

    void play() final;
    void pause() final;
    bool paused() const final;
    bool timeIsProgressing() const final;

    FloatSize naturalSize() const final { return m_naturalSize; }

    bool hasVideo() const final { return m_hasVideo; }
    bool hasAudio() const final { return m_hasAudio; }

    void setPageIsVisible(bool, String&& sceneIdentifier) final;

    MediaTime timeFudgeFactor() const { return { 1, 10 }; }
    MediaTime currentTime() const final;
    MediaTime duration() const final { return m_duration; }
    MediaTime startTime() const final { return MediaTime::zeroTime(); }
    MediaTime initialTime() const final { return MediaTime::zeroTime(); }

    void setRateDouble(double) final;
    double rate() const final { return m_rate; }
    double effectiveRate() const final;

    void setVolume(float) final;
    void setMuted(bool) final;

    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }

    MediaTime maxTimeSeekable() const final { return duration(); }
    MediaTime minTimeSeekable() const final { return startTime(); }
    const PlatformTimeRanges& buffered() const final;

    void setBufferedRanges(PlatformTimeRanges);
    void updateBufferedFromTrackBuffers(bool);
    void updateDurationFromTrackBuffers();

    void setLoadingProgresssed(bool);
    bool didLoadingProgress() const final;

    RefPtr<NativeImage> nativeImageForCurrentTime() final;
    bool updateLastPixelBuffer();
    bool updateLastImage();
    void paint(GraphicsContext&, const FloatRect&) final;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) final;
#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    void willBeAskedToPaintGL() final;
#endif
    RefPtr<VideoFrame> videoFrameForCurrentTime() final;
    DestinationColorSpace colorSpace() final;

    void setNaturalSize(FloatSize);
    void setHasAudio(bool);
    void setHasVideo(bool);
    void setHasAvailableVideoFrame(bool);
    bool hasAvailableVideoFrame() const final { return m_hasAvailableVideoFrame; }
    void setDuration(MediaTime);
    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    void characteristicsChanged();

    bool shouldEnsureLayer() const;
    void setPresentationSize(const IntSize&) final;
    bool supportsAcceleratedRendering() const final { return true; }
    void acceleratedRenderingStateChanged() final;
    void updateDisplayLayerAndDecompressionSession();

    RetainPtr<PlatformLayer> createVideoFullscreenLayer() final;
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler) final;
    void setVideoFullscreenFrame(FloatRect) final;

    bool requiresTextTrackRepresentation() const final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;
        
    String engineDescription() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final { return MediaPlayer::MovieLoadType::Download; }
        
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
    bool wirelessVideoPlaybackDisabled() const final { return false; }
#endif

    void enqueueSample(Ref<MediaSample>&&, TrackID);
    void reenqueSamples(TrackID);
    void reenqueueMediaForTime(TrackBuffer&, TrackID, const MediaTime&);
    void notifyClientWhenReadyForMoreSamples(TrackID);

    void setMinimumUpcomingPresentationTime(TrackID, const MediaTime&);
    void clearMinimumUpcomingPresentationTime(TrackID);

    bool isReadyForMoreSamples(TrackID);
    void didBecomeReadyForMoreSamples(TrackID);
    void appendCompleted(bool);
    void provideMediaData(TrackID);
    void provideMediaData(TrackBuffer&, TrackID);

    void trackDidChangeSelected(VideoTrackPrivate&, bool);
    void trackDidChangeEnabled(AudioTrackPrivate&, bool);

    using InitializationSegment = SourceBufferParserWebM::InitializationSegment;
    void didParseInitializationData(InitializationSegment&&);
    void didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&&, TrackID, const String& mediaType);
    void didUpdateFormatDescriptionForTrackId(Ref<TrackInfo>&&, TrackID);

    void flush();
#if PLATFORM(IOS_FAMILY)
    void flushIfNeeded();
#endif
    void flushTrack(TrackID);
    void flushVideo();
    void flushAudio(AVSampleBufferAudioRenderer*);

    void addTrackBuffer(TrackID, RefPtr<MediaDescription>&&);

    void ensureLayer();
    void ensureDecompressionSession();
    void addAudioRenderer(TrackID);
    void removeAudioRenderer(TrackID);

    void destroyLayer();
    void destroyDecompressionSession();
    void destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer>);
    void destroyAudioRenderers();
    void clearTracks();
        
    void registerNotifyWhenHasAvailableVideoFrame();
        
    void startVideoFrameMetadataGathering() final;
    void stopVideoFrameMetadataGathering() final;
    std::optional<VideoFrameMetadata> videoFrameMetadata() final { return std::exchange(m_videoFrameMetadata, { }); }
    void setResourceOwner(const ProcessIdentity& resourceOwner) final { m_resourceOwner = resourceOwner; }

    void checkNewVideoFrameMetadata(CMTime);

    // WebAVSampleBufferListenerParent
    // Methods are called on the WebMResourceClient's WorkQueue
    void videoRendererDidReceiveError(WebSampleBufferVideoRendering *, NSError *) final;
    void audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *) final;
    void videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *, bool isReadyForDisplay) final;

    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaPlayerPrivateWebM"_s; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;

    friend class MediaPlayerFactoryWebM;
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void maybeFinishLoading();

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    RetainPtr<id> m_durationObserver;
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
    RefPtr<NativeImage> m_lastImage;
    std::unique_ptr<PixelBufferConformerCV> m_rgbConformer;
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;
    RefPtr<WebMResourceClient> m_resourceClient;

    Vector<RefPtr<VideoTrackPrivateWebM>> m_videoTracks;
    Vector<RefPtr<AudioTrackPrivateWebM>> m_audioTracks;
    StdUnorderedMap<TrackID, UniqueRef<TrackBuffer>> m_trackBufferMap;
    PlatformTimeRanges m_buffered;

    RefPtr<VideoMediaSampleRenderer> m_videoRenderer;
    StdUnorderedMap<TrackID, RetainPtr<AVSampleBufferAudioRenderer>> m_audioRenderers;
    Ref<SourceBufferParserWebM> m_parser;
    const Ref<WTF::WorkQueue> m_appendQueue;

    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
#if !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    bool m_hasBeenAskedToPaintGL { false };
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_shouldPlayToTarget { false };
#endif
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    std::unique_ptr<VideoLayerManagerObjC> m_videoLayerManager;
    RetainPtr<id> m_videoFrameMetadataGatheringObserver;
    bool m_isGatheringVideoFrameMetadata { false };
    std::optional<VideoFrameMetadata> m_videoFrameMetadata;
    uint64_t m_lastConvertedSampleCount { 0 };
    uint64_t m_sampleCount { 0 };
    ProcessIdentity m_resourceOwner;
    std::unique_ptr<BinarySemaphore> m_hasAvailableVideoFrameSemaphore;

    FloatSize m_naturalSize;
    MediaTime m_currentTime;
    MediaTime m_duration;
    double m_rate { 1 };

    bool isEnabledVideoTrackID(TrackID) const;
    std::optional<TrackID> m_enabledVideoTrackID;
    std::atomic<uint32_t> m_abortCalled { 0 };
    uint32_t m_pendingAppends { 0 };
#if PLATFORM(IOS_FAMILY)
    bool m_displayLayerWasInterrupted { false };
#endif
    bool m_hasAudio { false };
    bool m_hasVideo { false };
    bool m_hasAvailableVideoFrame { false };
    bool m_visible { false };
    mutable bool m_loadingProgressed { false };
    bool m_loadFinished { false };
    bool m_delayedIdle { false };
    bool m_errored { false };
    bool m_processingInitializationSegment { false };
    Ref<WebAVSampleBufferListener> m_listener;

    // Seek logic support
    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final;
    void seekInternal();
    void maybeCompleteSeek();
    MediaTime clampTimeToLastSeekTime(const MediaTime&) const;
    bool shouldBePlaying() const;

    bool m_isPlaying { false };
    RetainPtr<id> m_timeJumpedObserver;
    Timer m_seekTimer;
    MediaTime m_lastSeekTime;
    std::optional<SeekTarget> m_pendingSeek;
    enum SeekState {
        Seeking,
        WaitingForAvailableFame,
        SeekCompleted,
    };
    SeekState m_seekState { SeekCompleted };
    bool m_isSynchronizerSeeking { false };
};

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
