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
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>
#include <wtf/Condition.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>
#include <wtf/threads/BinarySemaphore.h>

#if ENABLE(VIDEO_TRACK)
#include "TrackPrivateBaseGStreamer.h"
#include <wtf/text/AtomStringHash.h>
#endif
typedef struct _GstMpegtsSection GstMpegtsSection;

#if USE(GSTREAMER_GL)
#if USE(LIBEPOXY)
// Include the <epoxy/gl.h> header before <gst/gl/gl.h>.
#include <epoxy/gl.h>

// Workaround build issue with RPi userland GLESv2 headers and libepoxy <https://webkit.org/b/185639>
#if !GST_CHECK_VERSION(1, 14, 0)
#include <gst/gl/gstglconfig.h>
#if defined(GST_GL_HAVE_WINDOW_DISPMANX) && GST_GL_HAVE_WINDOW_DISPMANX
#define __gl2_h_
#undef GST_GL_HAVE_GLSYNC
#define GST_GL_HAVE_GLSYNC 1
#endif
#endif // !GST_CHECK_VERSION(1, 14, 0)
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

typedef struct _GstStreamVolume GstStreamVolume;
typedef struct _GstVideoInfo GstVideoInfo;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;

namespace WebCore {

class BitmapTextureGL;
class GLContext;
class GraphicsContext;
class GraphicsContext3D;
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
class MediaPlayerPrivateGStreamer : public MediaPlayerPrivateInterface, public CanMakeWeakPtr<MediaPlayerPrivateGStreamer, WeakPtrFactoryInitialization::Eager>
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
    static void initializeDebugCategory();

    MediaPlayerPrivateGStreamer(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamer();

    static bool isAvailable();
    static void registerMediaEngine(MediaEngineRegistrar);

    bool hasVideo() const override { return m_hasVideo; }
    bool hasAudio() const override { return m_hasAudio; }

    void load(const String &url) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const String& url, MediaSourcePrivateClient*) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void commitLoad();
    void cancelLoad() override;

    void prepareToPlay() override;
    void play() override;
    void pause() override;
    bool paused() const override;
    bool seeking() const override { return m_seeking; }
    void seek(const MediaTime&) override;

    MediaTime platformDuration() const;

    void setRate(float) override;
    double rate() const override;
    void setPreservesPitch(bool) override;
    void setPreload(MediaPlayer::Preload) override;

    FloatSize naturalSize() const final;

    void setVolume(float) override;
    float volume() const override;

#if USE(GSTREAMER_GL)
    bool ensureGstGLContext();
    GstContext* requestGLContext(const char* contextType);
#endif
    void setMuted(bool) override;
    bool muted() const;

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;

    void setVisible(bool) override { }
    void setSize(const IntSize&) override;
    void sizeChanged();

    // Prefer MediaTime based methods over float based.

    float duration() const override { return durationMediaTime().toFloat(); }
    double durationDouble() const override { return durationMediaTime().toDouble(); }
    MediaTime durationMediaTime() const override;
    float currentTime() const override { return currentMediaTime().toFloat(); }
    double currentTimeDouble() const override { return currentMediaTime().toDouble(); }
    MediaTime currentMediaTime() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    void seek(float time) override { seek(MediaTime::createWithFloat(time)); }
    void seekDouble(double time) override { seek(MediaTime::createWithDouble(time)); }

    float maxTimeSeekable() const override { return maxMediaTimeSeekable().toFloat(); }
    MediaTime maxMediaTimeSeekable() const override;
    double minTimeSeekable() const override { return minMediaTimeSeekable().toFloat(); }
    MediaTime minMediaTimeSeekable() const override { return MediaTime::zeroTime(); }

    bool didLoadingProgress() const override;
    unsigned long long totalBytes() const override;

    bool hasSingleSecurityOrigin() const override;
    Optional<bool> wouldTaintOrigin(const SecurityOrigin&) const override;

    void simulateAudioInterruption() override;

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() override;
#endif

    void paint(GraphicsContext&, const FloatRect&) override;

    bool supportsFullscreen() const override;

    MediaPlayer::MovieLoadType movieLoadType() const override;

    MediaPlayer* mediaPlayer() const { return m_player; }

    unsigned decodedFrameCount() const override;
    unsigned droppedFrameCount() const override;
    unsigned audioDecodedByteCount() const override;
    unsigned videoDecodedByteCount() const override;

    void acceleratedRenderingStateChanged() override;

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
    void cdmInstanceAttached(CDMInstance&) override;
    void cdmInstanceDetached(CDMInstance&) override;
    void handleProtectionEvent(GstEvent*);
    virtual void attemptToDecryptWithLocalInstance();
    void attemptToDecryptWithInstance(CDMInstance&) final;
    void initializationDataEncountered(InitData&&);
    void setWaitingForKey(bool);
    bool waitingForKey() const override;
#endif

    static bool supportsKeySystem(const String& keySystem, const String& mimeType);
    static MediaPlayer::SupportsType extendedSupportsType(const MediaEngineSupportParameters&, MediaPlayer::SupportsType);

#if USE(GSTREAMER_GL)
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum, GC3Dint, GC3Denum, GC3Denum, GC3Denum, bool, bool) override;
    NativeImagePtr nativeImageForCurrentTime() override;
#endif

    void setVideoSourceOrientation(ImageOrientation);
    GstElement* pipeline() const { return m_pipeline.get(); }
    void enableTrack(TrackPrivateBaseGStreamer::TrackType, unsigned index);

    // Append pipeline interface
    bool handleSyncMessage(GstMessage*);
    void handleMessage(GstMessage*);

protected:
    enum MainThreadNotification {
        VideoChanged = 1 << 0,
        VideoCapsChanged = 1 << 1,
        AudioChanged = 1 << 2,
        VolumeChanged = 1 << 3,
        MuteChanged = 1 << 4,
#if ENABLE(VIDEO_TRACK)
        TextChanged = 1 << 5,
#endif
        SizeChanged = 1 << 6,
        StreamCollectionChanged = 1 << 7
    };

    virtual bool isLiveStream() const { return m_isStreaming; }
    MediaTime maxTimeLoaded() const;

    virtual void durationChanged();
    virtual void sourceSetup(GstElement*);
    virtual void configurePlaySink() { }
    virtual bool changePipelineState(GstState);


#if USE(GSTREAMER_HOLEPUNCH)
    GstElement* createHolePunchVideoSink();
    void pushNextHolePunchBuffer();
    bool shouldIgnoreIntrinsicSize() final { return true; }
#endif

#if USE(GSTREAMER_GL)
    static GstFlowReturn newSampleCallback(GstElement*, MediaPlayerPrivateGStreamer*);
    static GstFlowReturn newPrerollCallback(GstElement*, MediaPlayerPrivateGStreamer*);
    void flushCurrentBuffer();
    GstElement* createGLAppSink();
    GstElement* createVideoSinkGL();
    GstGLContext* gstGLContext() const { return m_glContext.get(); }
    GstGLDisplay* gstGLDisplay() const { return m_glDisplay.get(); }
    void ensureGLVideoSinkContext();
#endif

#if USE(TEXTURE_MAPPER_GL)
    void pushTextureToCompositor();
#if USE(NICOSIA)
    void swapBuffersIfNeeded() override;
#else
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const override;
    void swapBuffersIfNeeded() override;
#endif
#endif

    GstElement* videoSink() const { return m_videoSink.get(); }

    void setStreamVolumeElement(GstStreamVolume*);

    void setPipeline(GstElement*);

    void triggerRepaint(GstSample*);
    void repaint();
    void cancelRepaint(bool destroying = false);

    static void repaintCallback(MediaPlayerPrivateGStreamer*, GstSample*);
    static void repaintCancelledCallback(MediaPlayerPrivateGStreamer*);

    void notifyPlayerOfVolumeChange();
    void notifyPlayerOfMute();

    static void volumeChangedCallback(MediaPlayerPrivateGStreamer*);
    static void muteChangedCallback(MediaPlayerPrivateGStreamer*);

    // FIXME: Where is this used?
    void handlePluginInstallerResult(GstInstallPluginsReturn);

    void readyTimerFired();

    void notifyPlayerOfVideo();
    void notifyPlayerOfVideoCaps();
    void notifyPlayerOfAudio();

#if ENABLE(VIDEO_TRACK)
    void notifyPlayerOfText();
    void newTextSample();
#endif

    void ensureAudioSourceProvider();
    void setAudioStreamProperties(GObject*);

    static void setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer*, GObject*);

    static void sourceSetupCallback(MediaPlayerPrivateGStreamer*, GstElement*);
    static void videoChangedCallback(MediaPlayerPrivateGStreamer*);
    static void videoSinkCapsChangedCallback(MediaPlayerPrivateGStreamer*);
    static void audioChangedCallback(MediaPlayerPrivateGStreamer*);
#if ENABLE(VIDEO_TRACK)
    static void textChangedCallback(MediaPlayerPrivateGStreamer*);
    static GstFlowReturn newTextSampleCallback(MediaPlayerPrivateGStreamer*);
#endif

    void timeChanged();
    void loadingFailed(MediaPlayer::NetworkState, MediaPlayer::ReadyState = MediaPlayer::HaveNothing, bool forceNotifications = false);
    void loadStateChanged();

#if USE(TEXTURE_MAPPER_GL)
    void updateTextureMapperFlags();
#endif

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    MediaPlayer* m_player;
    mutable MediaTime m_cachedPosition;
    mutable MediaTime m_cachedDuration;
    bool m_canFallBackToLastFinishedSeekPosition { false };
    bool m_changingRate { false };
    bool m_downloadFinished { false };
    bool m_errorOccured { false };
    mutable bool m_isEndReached { false };
    mutable bool m_isStreaming { false };
    bool m_paused { true };
    float m_playbackRate { 1 };
    GstState m_currentState;
    GstState m_oldState;
    GstState m_requestedState { GST_STATE_VOID_PENDING };
    bool m_resetPipeline { false };
    bool m_seeking { false };
    bool m_seekIsPending { false };
    MediaTime m_seekTime;
    GRefPtr<GstElement> m_source { nullptr };
    bool m_volumeAndMuteInitialized { false };

#if USE(TEXTURE_MAPPER_GL)
    TextureMapperGL::Flags m_textureMapperFlags;
#endif

    GRefPtr<GstStreamVolume> m_volumeElement;
    GRefPtr<GstElement> m_videoSink;
    GRefPtr<GstElement> m_pipeline;
    IntSize m_size;

    MediaPlayer::ReadyState m_readyState { MediaPlayer::HaveNothing };
    mutable MediaPlayer::NetworkState m_networkState { MediaPlayer::Empty };

    mutable Lock m_sampleMutex;
    GRefPtr<GstSample> m_sample;

    mutable FloatSize m_videoSize;
    bool m_usingFallbackVideoSink { false };
    bool m_renderingCanBeAccelerated { false };

    bool m_destroying { false };

#if USE(GSTREAMER_GL)
    GRefPtr<GstGLContext> m_glContext;
    GRefPtr<GstGLDisplay> m_glDisplay;
    GRefPtr<GstContext> m_glDisplayElementContext;
    GRefPtr<GstContext> m_glAppElementContext;
    std::unique_ptr<VideoTextureCopierGStreamer> m_videoTextureCopier;

    GRefPtr<GstGLColorConvert> m_colorConvert;
    GRefPtr<GstCaps> m_colorConvertInputCaps;
    GRefPtr<GstCaps> m_colorConvertOutputCaps;
#endif

    ImageOrientation m_videoSourceOrientation;

#if ENABLE(ENCRYPTED_MEDIA)
    BinarySemaphore m_cdmAttachmentSemaphore;
    RefPtr<const CDMInstance> m_cdmInstance;

    Lock m_protectionMutex; // Guards access to m_handledProtectionEvents.
    HashSet<uint32_t> m_handledProtectionEvents;

    bool m_waitingForKey { false };
#endif

    Optional<GstVideoDecoderPlatform> m_videoDecoderPlatform;

private:
    void fillTimerFired();

    void didEnd();


    GstElement* createVideoSink();
    GstElement* createAudioSink();
    GstElement* audioSink() const;

    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    void syncOnClock(bool sync);

    MediaTime playbackPosition() const;

    virtual void updateStates();
    virtual void asyncStateChangeDone();

    void createGSTPlayBin(const URL&, const String& pipelineName);

    bool loadNextLocation();
    void mediaLocationChanged(GstMessage*);

    virtual void updateDownloadBufferingFlag();
    void processBufferingStats(GstMessage*);
    void updateBufferingStatus(GstBufferingMode, double percentage);
    void updateMaxTimeLoaded(double percentage);

#if ENABLE(VIDEO_TRACK)
#if USE(GSTREAMER_MPEGTS)
    void processMpegTsSection(GstMpegtsSection*);
#endif

    void processTableOfContents(GstMessage*);
    void processTableOfContentsEntry(GstTocEntry*);

    void purgeInvalidAudioTracks(Vector<String> validTrackIds);
    void purgeInvalidVideoTracks(Vector<String> validTrackIds);
    void purgeInvalidTextTracks(Vector<String> validTrackIds);
#endif
    virtual bool doSeek(const MediaTime& position, float rate, GstSeekFlags seekType);
    virtual void updatePlaybackRate();

    String engineDescription() const override { return "GStreamer"; }
    bool didPassCORSAccessCheck() const override;
    bool canSaveMediaData() const override;

    void purgeOldDownloadFiles(const char*);
    static void uriDecodeBinElementAddedCallback(GstBin*, GstElement*, MediaPlayerPrivateGStreamer*);
    static void downloadBufferFileCreatedCallback(MediaPlayerPrivateGStreamer*);

    void setPlaybinURL(const URL& urlString);
    void loadFull(const String& url, const String& pipelineName);

    void updateTracks();
    void clearTracks();

#if ENABLE(VIDEO_TRACK)
    GRefPtr<GstElement> m_textAppSink;
    GRefPtr<GstPad> m_textAppSinkPad;
#endif
    GstStructure* m_mediaLocations { nullptr };
    int m_mediaLocationCurrentIndex { 0 };
    bool m_playbackRatePause { false };
    MediaTime m_timeOfOverlappingSeek;
    float m_lastPlaybackRate { 1 };
    Timer m_fillTimer;
    MediaTime m_maxTimeLoaded;
    bool m_loadingStalled { false };
    MediaPlayer::Preload m_preload;
    bool m_delayingLoad { false };
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
    bool m_buffering { false };
    int m_bufferingPercentage { 0 };
    mutable unsigned long long m_totalBytes { 0 };
    URL m_url;
    bool m_preservesPitch { false };
    mutable Optional<Seconds> m_lastQueryTime;
    bool m_isLegacyPlaybin;
    GRefPtr<GstStreamCollection> m_streamCollection;
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_streamPrivate;
#endif
    String m_currentAudioStreamId;
    String m_currentVideoStreamId;
    String m_currentTextStreamId;
#if ENABLE(WEB_AUDIO)
    std::unique_ptr<AudioSourceProviderGStreamer> m_audioSourceProvider;
#endif
    GRefPtr<GstElement> m_autoAudioSink;
    GRefPtr<GstElement> m_downloadBuffer;
    Vector<RefPtr<MediaPlayerRequestInstallMissingPluginsCallback>> m_missingPluginCallbacks;
#if ENABLE(VIDEO_TRACK)
    HashMap<AtomString, RefPtr<AudioTrackPrivateGStreamer>> m_audioTracks;
    HashMap<AtomString, RefPtr<InbandTextTrackPrivateGStreamer>> m_textTracks;
    HashMap<AtomString, RefPtr<VideoTrackPrivateGStreamer>> m_videoTracks;
    RefPtr<InbandMetadataTextTrackPrivateGStreamer> m_chaptersTrack;
#if USE(GSTREAMER_MPEGTS)
    HashMap<AtomString, RefPtr<InbandMetadataTextTrackPrivateGStreamer>> m_metadataTracks;
#endif
#endif // ENABLE(VIDEO_TRACK)
    virtual bool isMediaSource() const { return false; }

    uint64_t m_httpResponseTotalSize { 0 };
    uint64_t m_networkReadPosition { 0 };
    mutable uint64_t m_readPositionAtLastDidLoadingProgress { 0 };

    HashSet<RefPtr<WebCore::SecurityOrigin>> m_origins;
    Optional<bool> m_hasTaintedOrigin { WTF::nullopt };

    GRefPtr<GstElement> m_fpsSink { nullptr };
};

}
#endif // ENABLE(VIDEO) && USE(GSTREAMER)
