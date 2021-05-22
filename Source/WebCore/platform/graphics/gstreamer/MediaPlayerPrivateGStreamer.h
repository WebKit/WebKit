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

#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include "ImageOrientation.h"
#include "Logging.h"
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include "TrackPrivateBaseGStreamer.h"
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>
#include <wtf/Atomics.h>
#include <wtf/Condition.h>
#include <wtf/Forward.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>

typedef struct _GstMpegtsSection GstMpegtsSection;

#if USE(GSTREAMER_GL)
#if USE(LIBEPOXY)
// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>
#endif // USE(LIBEPOXY)

#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>
#undef GST_USE_UNSTABLE_API
#endif

#if USE(TEXTURE_MAPPER_GL)
#include "TextureMapperGL.h"
#if USE(NICOSIA)
#include "NicosiaContentLayerTextureMapperImpl.h"
#else
#include "TextureMapperPlatformLayerProxyProvider.h"
#endif
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "CDMProxy.h"
#endif

typedef struct _GstStreamVolume GstStreamVolume;
typedef struct _GstVideoInfo GstVideoInfo;

#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
struct wpe_video_plane_display_dmabuf_source;
#endif

namespace WebCore {

class BitmapTextureGL;
class GLContext;
class GraphicsContext;
class GraphicsContextGL;
class IntSize;
class IntRect;
class VideoTextureCopierGStreamer;

#if USE(TEXTURE_MAPPER_GL)
class TextureMapperPlatformLayerProxy;
#endif

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class AudioSourceProviderGStreamer;
#endif

class AudioTrackPrivateGStreamer;
class InbandMetadataTextTrackPrivateGStreamer;
class InbandTextTrackPrivateGStreamer;
class MediaPlayerRequestInstallMissingPluginsCallback;
class VideoTrackPrivateGStreamer;

void registerWebKitGStreamerElements();

// Use eager initialization for the WeakPtrFactory since we call makeWeakPtr() from another thread.
class MediaPlayerPrivateGStreamer : public MediaPlayerPrivateInterface
    , public CanMakeWeakPtr<MediaPlayerPrivateGStreamer, WeakPtrFactoryInitialization::Eager>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
    , public Nicosia::ContentLayerTextureMapperImpl::Client
#else
    , public PlatformLayer
#endif
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateGStreamer(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamer();

    static void registerMediaEngine(MediaEngineRegistrar);
    static MediaPlayer::SupportsType extendedSupportsType(const MediaEngineSupportParameters&, MediaPlayer::SupportsType);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    bool hasVideo() const final { return m_hasVideo; }
    bool hasAudio() const final { return m_hasAudio; }
    void load(const String &url) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient*) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() final;
    void prepareToPlay() final;
    void play() override;
    void pause() override;
    bool paused() const final;
    bool seeking() const override { return m_isSeeking; }
    void seek(const MediaTime&) override;
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
    void setVisible(bool visible) final { m_visible = visible; }
    void setSize(const IntSize&) final;
    // Prefer MediaTime based methods over float based.
    float duration() const final { return durationMediaTime().toFloat(); }
    double durationDouble() const final { return durationMediaTime().toDouble(); }
    MediaTime durationMediaTime() const override;
    float currentTime() const final { return currentMediaTime().toFloat(); }
    double currentTimeDouble() const final { return currentMediaTime().toDouble(); }
    MediaTime currentMediaTime() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    void seek(float time) final { seek(MediaTime::createWithFloat(time)); }
    void seekDouble(double time) final { seek(MediaTime::createWithDouble(time)); }
    float maxTimeSeekable() const final { return maxMediaTimeSeekable().toFloat(); }
    MediaTime maxMediaTimeSeekable() const override;
    double minTimeSeekable() const final { return minMediaTimeSeekable().toFloat(); }
    MediaTime minMediaTimeSeekable() const final { return MediaTime::zeroTime(); }
    bool didLoadingProgress() const final;
    unsigned long long totalBytes() const final;
    bool hasSingleSecurityOrigin() const final;
    Optional<bool> wouldTaintOrigin(const SecurityOrigin&) const final;
    void simulateAudioInterruption() final;
#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() final;
#endif
    void paint(GraphicsContext&, const FloatRect&) final;
    bool supportsFullscreen() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final;

    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;
    void acceleratedRenderingStateChanged() final;

#if USE(TEXTURE_MAPPER_GL)
    PlatformLayer* platformLayer() const override;
#if PLATFORM(WIN_CAIRO)
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

#if USE(GSTREAMER_GL)
    bool copyVideoTextureToPlatformTexture(GraphicsContextGL*, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool) override;
    RefPtr<NativeImage> nativeImageForCurrentTime() override;
#endif

    void updateEnabledVideoTrack();
    void updateEnabledAudioTrack();
    void playbin3SendSelectStreamsIfAppropriate();

    // Append pipeline interface
    // FIXME: Use the client interface pattern, AppendPipeline does not need the full interface to this class just for this function.
    bool handleNeedContextMessage(GstMessage*);

    void handleStreamCollectionMessage(GstMessage*);
    void handleMessage(GstMessage*);

    void triggerRepaint(GstSample*);
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

#if USE(GSTREAMER_GL)
    GstElement* createVideoSinkGL();
#endif

#if USE(TEXTURE_MAPPER_GL)
    void pushTextureToCompositor();
#if USE(NICOSIA)
    void swapBuffersIfNeeded() final;
#else
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const final;
    void swapBuffersIfNeeded() final;
#endif
#endif

    GstElement* videoSink() const { return m_videoSink.get(); }

    void setStreamVolumeElement(GstStreamVolume*);

    GstElement* pipeline() const { return m_pipeline.get(); }

    void repaint();
    void cancelRepaint(bool destroying = false);

    static void repaintCallback(MediaPlayerPrivateGStreamer*, GstSample*);
    static void repaintCancelledCallback(MediaPlayerPrivateGStreamer*);

    void notifyPlayerOfVolumeChange();
    void notifyPlayerOfMute();

    static void volumeChangedCallback(MediaPlayerPrivateGStreamer*);
    static void muteChangedCallback(MediaPlayerPrivateGStreamer*);

    void readyTimerFired();

    void notifyPlayerOfVideo();
    void notifyPlayerOfAudio();
    void notifyPlayerOfText();

    void ensureAudioSourceProvider();
    void setAudioStreamProperties(GObject*);

    virtual bool doSeek(const MediaTime& position, float rate, GstSeekFlags);
    void invalidateCachedPosition() { m_lastQueryTime.reset(); }

    static void setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer*, GObject*);

    static void sourceSetupCallback(MediaPlayerPrivateGStreamer*, GstElement*);
    static void videoChangedCallback(MediaPlayerPrivateGStreamer*);
    static void audioChangedCallback(MediaPlayerPrivateGStreamer*);
    static void textChangedCallback(MediaPlayerPrivateGStreamer*);

    void timeChanged();
    void loadingFailed(MediaPlayer::NetworkState, MediaPlayer::ReadyState = MediaPlayer::ReadyState::HaveNothing, bool forceNotifications = false);
    void loadStateChanged();

#if USE(TEXTURE_MAPPER_GL)
    void updateTextureMapperFlags();
#endif

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    MediaPlayer* m_player;
    String m_referrer;
    mutable MediaTime m_cachedPosition;
    mutable MediaTime m_cachedDuration;
    bool m_canFallBackToLastFinishedSeekPosition { false };
    bool m_isChangingRate { false };
    bool m_didDownloadFinish { false };
    bool m_didErrorOccur { false };
    mutable bool m_isEndReached { false };
    mutable bool m_isLiveStream { false };
    bool m_isPaused { true };
    float m_playbackRate { 1 };
    GstState m_currentState { GST_STATE_NULL };
    GstState m_oldState { GST_STATE_NULL };
    GstState m_requestedState { GST_STATE_VOID_PENDING };
    bool m_shouldResetPipeline { false };
    bool m_isSeeking { false };
    bool m_isSeekPending { false };
    MediaTime m_seekTime;
    GRefPtr<GstElement> m_source { nullptr };
    bool m_areVolumeAndMuteInitialized { false };

#if USE(TEXTURE_MAPPER_GL)
    TextureMapperGL::Flags m_textureMapperFlags;
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

    bool m_isBeingDestroyed { false };

#if USE(GSTREAMER_GL)
    std::unique_ptr<VideoTextureCopierGStreamer> m_videoTextureCopier;
    GRefPtr<GstGLColorConvert> m_colorConvert;
    GRefPtr<GstCaps> m_colorConvertInputCaps;
    GRefPtr<GstCaps> m_colorConvertOutputCaps;
#endif

    ImageOrientation m_videoSourceOrientation;

#if ENABLE(ENCRYPTED_MEDIA)
    Lock m_cdmAttachmentMutex;
    Condition m_cdmAttachmentCondition;
    RefPtr<CDMInstanceProxy> m_cdmInstance;

    Lock m_protectionMutex; // Guards access to m_handledProtectionEvents.
    HashSet<uint32_t> m_handledProtectionEvents;

    bool m_isWaitingForKey { false };
#endif

    Optional<GstVideoDecoderPlatform> m_videoDecoderPlatform;

private:
    bool isPlayerShuttingDown() const { return m_isPlayerShuttingDown.load(); }
    MediaTime maxTimeLoaded() const;
    void setVideoSourceOrientation(ImageOrientation);
    MediaTime platformDuration() const;
    bool isMuted() const;
    void commitLoad();
    void fillTimerFired();
    void didEnd();

    GstElement* createVideoSink();
    GstElement* createAudioSink();
    GstElement* audioSink() const;

    friend class MediaPlayerFactoryGStreamer;
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void syncOnClock(bool sync);

    MediaTime playbackPosition() const;

    virtual void updateStates();
    virtual void asyncStateChangeDone();

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

    void purgeInvalidAudioTracks(Vector<String> validTrackIds);
    void purgeInvalidVideoTracks(Vector<String> validTrackIds);
    void purgeInvalidTextTracks(Vector<String> validTrackIds);

    String engineDescription() const override { return "GStreamer"; }
    bool didPassCORSAccessCheck() const override;
    bool canSaveMediaData() const override;

    void purgeOldDownloadFiles(const char*);
    static void uriDecodeBinElementAddedCallback(GstBin*, GstElement*, MediaPlayerPrivateGStreamer*);
    static void downloadBufferFileCreatedCallback(MediaPlayerPrivateGStreamer*);

    void setPlaybinURL(const URL& urlString);

    void updateTracks(const GRefPtr<GstStreamCollection>&);
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

    Atomic<bool> m_isPlayerShuttingDown;
    GRefPtr<GstElement> m_textSink;
    GstStructure* m_mediaLocations { nullptr };
    int m_mediaLocationCurrentIndex { 0 };
    bool m_isPlaybackRatePaused { false };
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
    Lock m_drawMutex;
    RunLoop::Timer<MediaPlayerPrivateGStreamer> m_drawTimer;
    RunLoop::Timer<MediaPlayerPrivateGStreamer> m_readyTimerHandler;
#if USE(TEXTURE_MAPPER_GL)
#if USE(NICOSIA)
    Ref<Nicosia::ContentLayer> m_nicosiaLayer;
#else
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif
#endif
    bool m_isBuffering { false };
    int m_bufferingPercentage { 0 };
    mutable unsigned long long m_totalBytes { 0 };
    URL m_url;
    bool m_shouldPreservePitch { false };
    mutable Optional<Seconds> m_lastQueryTime;
    bool m_isLegacyPlaybin;
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_streamPrivate;
#endif

    bool m_visible { false };

    // playbin3 only:
    bool m_waitingForStreamsSelectedEvent { true };
    AtomString m_currentAudioStreamId; // Currently playing.
    AtomString m_currentVideoStreamId;
    AtomString m_wantedAudioStreamId; // Set in JavaScript.
    AtomString m_wantedVideoStreamId;
    AtomString m_requestedAudioStreamId; // Expected in the next STREAMS_SELECTED message.
    AtomString m_requestedVideoStreamId;

#if ENABLE(WEB_AUDIO)
    std::unique_ptr<AudioSourceProviderGStreamer> m_audioSourceProvider;
#endif
    GRefPtr<GstElement> m_downloadBuffer;
    Vector<RefPtr<MediaPlayerRequestInstallMissingPluginsCallback>> m_missingPluginCallbacks;
    HashMap<AtomString, RefPtr<AudioTrackPrivateGStreamer>> m_audioTracks;
    HashMap<AtomString, RefPtr<InbandTextTrackPrivateGStreamer>> m_textTracks;
    HashMap<AtomString, RefPtr<VideoTrackPrivateGStreamer>> m_videoTracks;
    RefPtr<InbandMetadataTextTrackPrivateGStreamer> m_chaptersTrack;
#if USE(GSTREAMER_MPEGTS)
    HashMap<AtomString, RefPtr<InbandMetadataTextTrackPrivateGStreamer>> m_metadataTracks;
#endif
    virtual bool isMediaSource() const { return false; }

    uint64_t m_httpResponseTotalSize { 0 };
    uint64_t m_networkReadPosition { 0 };
    mutable uint64_t m_readPositionAtLastDidLoadingProgress { 0 };

    HashSet<RefPtr<WebCore::SecurityOrigin>> m_origins;
    Optional<bool> m_hasTaintedOrigin { WTF::nullopt };

    GRefPtr<GstElement> m_fpsSink { nullptr };
    uint64_t m_totalVideoFrames { 0 };
    uint64_t m_droppedVideoFrames { 0 };

    // This is set to true if no videoflip element has been added to the pipeline.
    bool m_shouldHandleOrientationTags { false };

private:
#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
    GUniquePtr<struct wpe_video_plane_display_dmabuf_source> m_wpeVideoPlaneDisplayDmaBuf;
#endif
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}
#endif // ENABLE(VIDEO) && USE(GSTREAMER)
