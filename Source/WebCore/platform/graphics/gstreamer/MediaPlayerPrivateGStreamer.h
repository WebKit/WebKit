/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
 * Copyright (C) 2009, 2019 Igalia S.L
 * Copyright (C) 2015, 2019 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "AbortableTaskQueue.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include "ImageOrientation.h"
#include "Logging.h"
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include "PlatformMediaResourceLoader.h"
#include "TrackPrivateBaseGStreamer.h"
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>
#include <wtf/Atomics.h>
#include <wtf/Condition.h>
#include <wtf/DataMutex.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>

typedef struct _GstMpegtsSection GstMpegtsSection;

#if USE(GSTREAMER_GL)
// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>
#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#undef GST_USE_UNSTABLE_API
#endif

#if USE(TEXTURE_MAPPER)
#if USE(NICOSIA)
#include "NicosiaContentLayer.h"
#else
#include "TextureMapperPlatformLayerProxyProvider.h"
#endif
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "CDMProxy.h"
#endif

typedef struct _GstStreamVolume GstStreamVolume;
typedef struct _GstVideoInfo GstVideoInfo;

namespace WebCore {

class BitmapTexture;
class GLContext;
class GraphicsContext;
class GraphicsContextGL;
class IntSize;
class IntRect;

#if USE(TEXTURE_MAPPER)
class TextureMapperPlatformLayerProxy;
#endif

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class AudioSourceProviderGStreamer;
#endif

class AudioTrackPrivateGStreamer;
class InbandMetadataTextTrackPrivateGStreamer;
class InbandTextTrackPrivateGStreamer;
class VideoTrackPrivateGStreamer;

#if USE(TEXTURE_MAPPER_DMABUF)
class GBMBufferSwapchain;
#endif

enum class TextureMapperFlags : uint16_t;

void registerWebKitGStreamerElements();

// Use eager initialization for the WeakPtrFactory since we construct WeakPtrs on another thread.
class MediaPlayerPrivateGStreamer
    : public MediaPlayerPrivateInterface
    , public CanMakeWeakPtr<MediaPlayerPrivateGStreamer, WeakPtrFactoryInitialization::Eager>
    , public RefCounted<MediaPlayerPrivateGStreamer>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
#if USE(TEXTURE_MAPPER)
#if USE(NICOSIA)
    , public Nicosia::ContentLayer::Client
#else
    , public PlatformLayer
#endif
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateGStreamer(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamer();

    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

    static void registerMediaEngine(MediaEngineRegistrar);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    bool hasVideo() const final { return m_hasVideo; }
    bool hasAudio() const final { return m_hasAudio; }
    void load(const String &url) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() final;
    void prepareToPlay() final;
    void play() override;
    void pause() override;
    bool paused() const final;
    bool ended() const final;
    bool seeking() const override { return m_isSeeking; }
    void seekToTarget(const SeekTarget&) override;
    void setRate(float) override;
    double rate() const final;
    void setPreservesPitch(bool) final;
    void setPreload(MediaPlayer::Preload) final;
    FloatSize naturalSize() const final;
    void setVolume(float) final;
    float volume() const final;
    void setMuted(bool) final;
    MediaPlayer::NetworkState networkState() const final;
    MediaPlayer::ReadyState readyState() const final;
    void setPageIsVisible(bool visible, String&&) final { m_visible = visible; }
    void setVisibleInViewport(bool isVisible) final;
    void setPresentationSize(const IntSize&) final;
    // Prefer MediaTime based methods over float based.
    float duration() const final { return durationMediaTime().toFloat(); }
    double durationDouble() const final { return durationMediaTime().toDouble(); }
    MediaTime durationMediaTime() const override;
    float currentTime() const final { return currentMediaTime().toFloat(); }
    double currentTimeDouble() const final { return currentMediaTime().toDouble(); }
    MediaTime currentMediaTime() const override;
    const PlatformTimeRanges& buffered() const override;
    float maxTimeSeekable() const final { return maxMediaTimeSeekable().toFloat(); }
    MediaTime maxMediaTimeSeekable() const override;
    double minTimeSeekable() const final { return minMediaTimeSeekable().toFloat(); }
    MediaTime minMediaTimeSeekable() const final { return MediaTime::zeroTime(); }
    bool didLoadingProgress() const final;
    unsigned long long totalBytes() const final;
    std::optional<bool> isCrossOrigin(const SecurityOrigin&) const final;
    void simulateAudioInterruption() final;
#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() final;
#endif
    void paint(GraphicsContext&, const FloatRect&) final;
    DestinationColorSpace colorSpace() final;
    bool supportsFullscreen() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    unsigned decodedFrameCount() const final;
    unsigned droppedFrameCount() const final;
    void acceleratedRenderingStateChanged() final;
    bool performTaskAtMediaTime(Function<void()>&&, const MediaTime&) override;
    void isLoopingChanged() final;

#if USE(TEXTURE_MAPPER)
    PlatformLayer* platformLayer() const override;
#if PLATFORM(WIN)
    // FIXME: Accelerated rendering has not been implemented for WinCairo yet.
    bool supportsAcceleratedRendering() const override { return false; }
#else
    bool supportsAcceleratedRendering() const override { return true; }
#endif
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    bool waitingForKey() const final;

    void handleProtectionEvent(GstEvent*);
#endif

    RefPtr<VideoFrame> videoFrameForCurrentTime() override;

    void updateEnabledVideoTrack();
    void updateEnabledAudioTrack();
    void playbin3SendSelectStreamsIfAppropriate();

    void updateVideoOrientation(const GstTagList*);

    // Append pipeline interface
    // FIXME: Use the client interface pattern, AppendPipeline does not need the full interface to this class just for this function.
    bool handleNeedContextMessage(GstMessage*);

    void handleStreamCollectionMessage(GstMessage*);
    void handleMessage(GstMessage*);

    void triggerRepaint(GRefPtr<GstSample>&&);
#if USE(GSTREAMER_GL)
    void flushCurrentBuffer();
#endif

    void handleTextSample(GstSample*, const char* streamId);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const char* logClassName() const override { return "MediaPlayerPrivateGStreamer"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const override;

    const void* mediaPlayerLogIdentifier() { return logIdentifier(); }
    const Logger& mediaPlayerLogger() { return logger(); }
#endif

    // This AbortableTaskQueue must be aborted everytime a flush is sent downstream from the main thread
    // to avoid deadlocks from threads in the playback pipeline waiting for the main thread.
    AbortableTaskQueue& sinkTaskQueue() { return m_sinkTaskQueue; }

    String codecForStreamId(const String& streamId);

protected:
    enum MainThreadNotification {
        VideoChanged = 1 << 0,
        VideoCapsChanged = 1 << 1,
        AudioChanged = 1 << 2,
        VolumeChanged = 1 << 3,
        MuteChanged = 1 << 4,
        TextChanged = 1 << 5,
        StreamCollectionChanged = 1 << 7
    };

    enum class PlaybackRatePausedState {
        ManuallyPaused, // Initialization or user explicitly paused. This takes preference over RatePaused. You don't
                        // transition from Manually to Rate Paused unless there is a play while rate == 0.
        RatePaused, // Pipeline was playing and rate was set to zero.
        ShouldMoveToPlaying, // Pipeline was paused because of zero rate and it should be playing. This is not a
                             // definitive state, just an operational transition from RatePaused to Playing to keep the
                             // pipeline state changes contained in updateStates.
        Playing, // Pipeline is playing and it should be.
    };

    static bool isAvailable();

    virtual void durationChanged();
    virtual void sourceSetup(GstElement*);
    virtual bool changePipelineState(GstState);
    virtual void updatePlaybackRate();

#if USE(GSTREAMER_HOLEPUNCH)
    GstElement* createHolePunchVideoSink();
    void pushNextHolePunchBuffer();
    bool shouldIgnoreIntrinsicSize() final { return true; }
#endif

#if USE(TEXTURE_MAPPER_DMABUF)
    GstElement* createVideoSinkDMABuf();
#endif
#if USE(GSTREAMER_GL)
    GstElement* createVideoSinkGL();
#endif

#if USE(TEXTURE_MAPPER)
    void pushTextureToCompositor();
#if USE(NICOSIA)
    void swapBuffersIfNeeded() final;
#else
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const final;
    void swapBuffersIfNeeded() final;
#endif
#endif

#if USE(TEXTURE_MAPPER_DMABUF)
    void pushDMABufToCompositor();
#endif

    GstElement* videoSink() const { return m_videoSink.get(); }

    void setStreamVolumeElement(GstStreamVolume*);

    GstElement* pipeline() const { return m_pipeline.get(); }

    void repaint();
    void cancelRepaint(bool destroying = false);

    static void repaintCancelledCallback(MediaPlayerPrivateGStreamer*);

    void notifyPlayerOfVolumeChange();
    void notifyPlayerOfMute();

    static void volumeChangedCallback(MediaPlayerPrivateGStreamer*);
    static void muteChangedCallback(MediaPlayerPrivateGStreamer*);

    void pausedTimerFired();

    template <typename TrackPrivateType> void notifyPlayerOfTrack();

    void ensureAudioSourceProvider();
    void checkPlayingConsistency();

    virtual bool doSeek(const SeekTarget& position, float rate);
    void invalidateCachedPosition() const;
    void ensureSeekFlags();

    static void sourceSetupCallback(MediaPlayerPrivateGStreamer*, GstElement*);

    void timeChanged(const MediaTime&); // If MediaTime is valid, indicates that a seek has completed.
    void loadingFailed(MediaPlayer::NetworkState, MediaPlayer::ReadyState = MediaPlayer::ReadyState::HaveNothing, bool forceNotifications = false);
    void loadStateChanged();

#if USE(TEXTURE_MAPPER)
    void updateTextureMapperFlags();
#endif

    void setCachedPosition(const MediaTime&) const;

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    ThreadSafeWeakPtr<MediaPlayer> m_player;
    String m_referrer;
    mutable MediaTime m_cachedPosition;
    mutable bool m_isCachedPositionValid { false };
    mutable MediaTime m_cachedDuration;
    bool m_canFallBackToLastFinishedSeekPosition { false };
    bool m_isChangingRate { false };
    bool m_didDownloadFinish { false };
    bool m_didErrorOccur { false };
    mutable bool m_isEndReached { false };
    mutable std::optional<bool> m_isLiveStream;

    // m_isPaused represents:
    // A) In MSE streams, whether playback or pause has last been requested with pause() and play(),
    //    defaulting to true before playback starts.
    // B) In live streams, whether at an unspecified point in time after the main thread tick in
    //    which play() or pause() are called, whether the playback was paused or resumed.
    // C) In regular non-live streams, it represents whether playback has ended with a EOS with
    //    looping set to false since the pipeline successfully pre-rolled.
    //
    // FIXME m_isPaused should represent something useful and consistent for all the possible cases
    // (regular playback, live playback, MSE, WebRTC) or be deleted from MediaPlayerPrivateGStreamer.
    // https://bugs.webkit.org/show_bug.cgi?id=260385
    bool m_isPaused { true };
    float m_playbackRate { 1 };
    PlaybackRatePausedState m_playbackRatePausedState { PlaybackRatePausedState::ManuallyPaused };
    GstState m_currentState { GST_STATE_NULL };
    GstState m_oldState { GST_STATE_NULL };
    GstState m_requestedState { GST_STATE_VOID_PENDING };
    bool m_shouldResetPipeline { false };
    bool m_isSeeking { false };
    bool m_isSeekPending { false };
    SeekTarget m_seekTarget;
    GRefPtr<GstElement> m_source { nullptr };
    bool m_areVolumeAndMuteInitialized { false };

#if USE(TEXTURE_MAPPER)
    OptionSet<TextureMapperFlags> m_textureMapperFlags;
#endif

    GRefPtr<GstStreamVolume> m_volumeElement;
    GRefPtr<GstElement> m_audioSink;
    GRefPtr<GstElement> m_videoSink;
    GRefPtr<GstElement> m_pipeline;
    IntSize m_size;

    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    mutable MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };

    mutable Lock m_sampleMutex;
    GRefPtr<GstSample> m_sample;

    mutable FloatSize m_videoSize;
    bool m_isUsingFallbackVideoSink { false };
    bool m_canRenderingBeAccelerated { false };

    bool m_isBeingDestroyed WTF_GUARDED_BY_LOCK(m_drawLock) { false };

    ImageOrientation m_videoSourceOrientation;

#if ENABLE(ENCRYPTED_MEDIA)
    Lock m_cdmAttachmentLock;
    Condition m_cdmAttachmentCondition;
    RefPtr<CDMInstanceProxy> m_cdmInstance;

    Lock m_protectionMutex; // Guards access to m_handledProtectionEvents.
    HashSet<uint32_t> m_handledProtectionEvents;

    bool m_isWaitingForKey { false };
#endif

    std::optional<GstVideoDecoderPlatform> m_videoDecoderPlatform;
    GstSeekFlags m_seekFlags;

    String errorMessage() const override { return m_errorMessage; }

    void incrementDecodedVideoFramesCount() { m_decodedVideoFrames++; }
    uint64_t decodedVideoFramesCount() const { return m_decodedVideoFrames; }

    bool updateVideoSinkStatistics();

private:
    class TaskAtMediaTimeScheduler {
    public:
        enum PlaybackDirection {
            Forward, // Schedule when targetTime <= currentTime. Used on forward playback, when playbackRate >= 0.
            Backward // Schedule when targetTime >= currentTime. Used on backward playback, when playbackRate < 0.
        };
        void setTask(Function<void()>&& task, const MediaTime& targetTime, PlaybackDirection playbackDirection)
        {
            m_targetTime = targetTime;
            m_task = WTFMove(task);
            m_playbackDirection = playbackDirection;
        }
        std::optional<Function<void()>> checkTaskForScheduling(const MediaTime& currentTime)
        {
            if (!m_targetTime.isValid() || !currentTime.isFinite()
                || (m_playbackDirection == Forward && currentTime < m_targetTime)
                || (m_playbackDirection == Backward && currentTime > m_targetTime))
                return std::optional<Function<void()>>();
            m_targetTime = MediaTime::invalidTime();
            return WTFMove(m_task);
        }
    private:
        MediaTime m_targetTime = MediaTime::invalidTime();
        PlaybackDirection m_playbackDirection = Forward;
        Function<void()> m_task = Function<void()>();
    };

    bool isPlayerShuttingDown() const { return m_isPlayerShuttingDown.load(); }
    MediaTime maxTimeLoaded() const;
    bool setVideoSourceOrientation(ImageOrientation);
    MediaTime platformDuration() const;
    bool isMuted() const;
    void commitLoad();
    void fillTimerFired();
    void didEnd();
    void setPlaybackFlags(bool isMediaStream);

    GstElement* createVideoSink();
    GstElement* createAudioSink();
    GstElement* audioSink() const { return m_audioSink.get(); }

    bool isMediaStreamPlayer() const;

    friend class MediaPlayerFactoryGStreamer;
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void syncOnClock(bool sync);

    GstClockTime gstreamerPositionFromSinks() const;
    MediaTime playbackPosition() const;

    virtual void updateStates();
    void finishSeek();
    virtual void didPreroll() { }

    void createGSTPlayBin(const URL&);

    bool loadNextLocation();
    void mediaLocationChanged(GstMessage*);

    virtual void updateDownloadBufferingFlag();
    void processBufferingStats(GstMessage*);
    void updateBufferingStatus(GstBufferingMode, double percentage);
    void updateMaxTimeLoaded(double percentage);

#if USE(GSTREAMER_MPEGTS)
    void processMpegTsSection(GstMpegtsSection*);
#endif

    void processTableOfContents(GstMessage*);
    void processTableOfContentsEntry(GstTocEntry*);

    String engineDescription() const override { return "GStreamer"_s; }
    bool didPassCORSAccessCheck() const override;
    bool canSaveMediaData() const override;

    void purgeOldDownloadFiles(const String& downloadFilePrefixPath);
    void configureDownloadBuffer(GstElement*);
    static void downloadBufferFileCreatedCallback(MediaPlayerPrivateGStreamer*);

    void configureAudioDecoder(GstElement*);
    void configureVideoDecoder(GstElement*);
    void configureElement(GstElement*);
#if PLATFORM(BROADCOM) || USE(WESTEROS_SINK) || PLATFORM(AMLOGIC) || PLATFORM(REALTEK)
    void configureElementPlatformQuirks(GstElement*);
#endif

    void setPlaybinURL(const URL& urlString);

    void updateTracks(const GRefPtr<GstObject>& collectionOwner);
    void videoSinkCapsChanged(GstPad*);
    void updateVideoSizeAndOrientationFromCaps(const GstCaps*);
    bool hasFirstVideoSampleReachedSink() const;

#if ENABLE(ENCRYPTED_MEDIA)
    bool isCDMAttached() const { return m_cdmInstance; }
    void attemptToDecryptWithLocalInstance();
    void initializationDataEncountered(InitData&&);
    InitData parseInitDataFromProtectionMessage(GstMessage*);
    bool waitForCDMAttachment();
#endif

    void configureMediaStreamAudioTracks();
    void invalidateCachedPositionOnNextIteration() const;

    Atomic<bool> m_isPlayerShuttingDown;
    GRefPtr<GstElement> m_textSink;
    GUniquePtr<GstStructure> m_mediaLocations;
    int m_mediaLocationCurrentIndex { 0 };
    MediaTime m_timeOfOverlappingSeek;
    // Last playback rate sent through a GStreamer seek.
    float m_lastPlaybackRate { 1 };
    Timer m_fillTimer;
    MediaTime m_maxTimeLoaded;
    bool m_loadingStalled { false };
    MediaPlayer::Preload m_preload;
    bool m_isDelayingLoad { false };
    mutable MediaTime m_maxTimeLoadedAtLastDidLoadingProgress;
    bool m_hasVideo { false };
    bool m_hasAudio { false };
    Condition m_drawCondition;
    Lock m_drawLock;
    RunLoop::Timer m_drawTimer WTF_GUARDED_BY_LOCK(m_drawLock);
    RunLoop::Timer m_pausedTimerHandler;
#if USE(TEXTURE_MAPPER)
#if USE(NICOSIA)
    RefPtr<Nicosia::ContentLayer> m_nicosiaLayer;
#else
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif
#endif
    bool m_isBuffering { false };
    int m_bufferingPercentage { 0 };
    bool m_hasWebKitWebSrcSentEOS { false };
    mutable unsigned long long m_totalBytes { 0 };
    URL m_url;
    bool m_shouldPreservePitch { false };
    bool m_isLegacyPlaybin;
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_streamPrivate;
#endif

    bool m_isMuted { false };
    bool m_visible { false };

    // playbin3 only:
    bool m_waitingForStreamsSelectedEvent { true };
    AtomString m_currentAudioStreamId; // Currently playing.
    AtomString m_currentVideoStreamId;
    AtomString m_currentTextStreamId;
    AtomString m_wantedAudioStreamId; // Set in JavaScript.
    AtomString m_wantedVideoStreamId;
    AtomString m_wantedTextStreamId;
    AtomString m_requestedAudioStreamId; // Expected in the next STREAMS_SELECTED message.
    AtomString m_requestedVideoStreamId;
    AtomString m_requestedTextStreamId;

#if ENABLE(WEB_AUDIO)
    RefPtr<AudioSourceProviderGStreamer> m_audioSourceProvider;
#endif
    GRefPtr<GstElement> m_downloadBuffer;

    HashMap<AtomString, Ref<AudioTrackPrivateGStreamer>> m_audioTracks;
    HashMap<AtomString, Ref<VideoTrackPrivateGStreamer>> m_videoTracks;
    HashMap<AtomString, Ref<InbandTextTrackPrivateGStreamer>> m_textTracks;
    RefPtr<InbandMetadataTextTrackPrivateGStreamer> m_chaptersTrack;
#if USE(GSTREAMER_MPEGTS)
    HashMap<AtomString, RefPtr<InbandMetadataTextTrackPrivateGStreamer>> m_metadataTracks;
#endif
    virtual bool isMediaSource() const { return false; }

    uint64_t m_httpResponseTotalSize { 0 };
    uint64_t m_networkReadPosition { 0 };
    mutable uint64_t m_readPositionAtLastDidLoadingProgress { 0 };

    uint64_t m_totalVideoFrames { 0 };
    uint64_t m_droppedVideoFrames { 0 };
    uint64_t m_decodedVideoFrames { 0 };

    DataMutex<TaskAtMediaTimeScheduler> m_TaskAtMediaTimeSchedulerDataMutex;

private:
    std::optional<VideoFrameMetadata> videoFrameMetadata() final;
    uint64_t m_sampleCount { 0 };
    uint64_t m_lastVideoFrameMetadataSampleCount { 0 };
    mutable PlatformTimeRanges m_buffered;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    String m_errorMessage;

#if USE(TEXTURE_MAPPER_DMABUF)
    HashSet<GRefPtr<GstMemory>> m_dmabufMemory;
    RefPtr<GBMBufferSwapchain> m_swapchain;
#endif

    GRefPtr<GstStreamCollection> m_streamCollection;

    AbortableTaskQueue m_sinkTaskQueue;

    bool m_didTryToRecoverPlayingState { false };

    bool m_isVisibleInViewport { true };
    GstState m_invisiblePlayerState { GST_STATE_VOID_PENDING };

    // Specific to MediaStream playback.
    MediaTime m_startTime;
    MediaTime m_pausedTime;

    void setupCodecProbe(GstElement*);
    Lock m_codecsLock;
    HashMap<String, String> m_codecs WTF_GUARDED_BY_LOCK(m_codecsLock);

    bool isSeamlessSeekingEnabled() const { return m_seekFlags & (1 << GST_SEEK_FLAG_SEGMENT); }

    RefPtr<PlatformMediaResourceLoader> m_loader;
};

}
#endif // ENABLE(VIDEO) && USE(GSTREAMER)
