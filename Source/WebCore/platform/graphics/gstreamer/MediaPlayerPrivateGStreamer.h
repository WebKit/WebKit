/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2015, 2016 Igalia S.L
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
 * Copyright (C) 2015, 2016 Metrological Group B.V.
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
#include "MediaPlayerPrivateGStreamerBase.h"

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

#if ENABLE(VIDEO_TRACK)
#include "TrackPrivateBaseGStreamer.h"
#include <wtf/text/AtomicStringHash.h>
#endif

typedef struct _GstMpegtsSection GstMpegtsSection;

namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class AudioSourceProviderGStreamer;
#endif

class AudioTrackPrivateGStreamer;
class InbandMetadataTextTrackPrivateGStreamer;
class InbandTextTrackPrivateGStreamer;
class MediaPlayerRequestInstallMissingPluginsCallback;
class VideoTrackPrivateGStreamer;

#if ENABLE(MEDIA_SOURCE)
class MediaSourcePrivateClient;
#endif

class MediaPlayerPrivateGStreamer : public MediaPlayerPrivateGStreamerBase {
public:
    explicit MediaPlayerPrivateGStreamer(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamer();

    static void registerMediaEngine(MediaEngineRegistrar);
    static bool isAvailable();

    void handleMessage(GstMessage*);
    void handlePluginInstallerResult(GstInstallPluginsReturn);

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
    bool seeking() const override;

    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override;
    void seek(const MediaTime&) override;

    void setRate(float) override;
    double rate() const override;
    void setPreservesPitch(bool) override;

    void setPreload(MediaPlayer::Preload) override;
    void fillTimerFired();

    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    MediaTime maxMediaTimeSeekable() const override;
    bool didLoadingProgress() const override;
    unsigned long long totalBytes() const override;
    MediaTime maxTimeLoaded() const override;

    bool hasSingleSecurityOrigin() const override;
    std::optional<bool> wouldTaintOrigin(const SecurityOrigin&) const override;

    void loadStateChanged();
    void timeChanged();
    void didEnd();
    virtual void durationChanged();
    void loadingFailed(MediaPlayer::NetworkState);

    virtual void sourceSetup(GstElement*);

    GstElement* audioSink() const override;
    virtual void configurePlaySink() { }

    void simulateAudioInterruption() override;

    virtual bool changePipelineState(GstState);

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() override;
#endif

    bool isLiveStream() const override { return m_isStreaming; }

    void enableTrack(TrackPrivateBaseGStreamer::TrackType, unsigned index);

    bool handleSyncMessage(GstMessage*) override;

private:
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);


    GstElement* createAudioSink() override;

    MediaTime playbackPosition() const;

    virtual void updateStates();
    virtual void asyncStateChangeDone();

    void createGSTPlayBin(const gchar* playbinName, const String& pipelineName);

    bool loadNextLocation();
    void mediaLocationChanged(GstMessage*);

    virtual void setDownloadBuffering();
    void processBufferingStats(GstMessage*);
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
    void loadFull(const String& url, const gchar* playbinName, const String& pipelineName);

#if GST_CHECK_VERSION(1, 10, 0)
    void updateTracks();
    void clearTracks();
#endif

protected:
    void cacheDuration();

    bool m_buffering;
    int m_bufferingPercentage;
    mutable MediaTime m_cachedPosition;
    bool m_canFallBackToLastFinishedSeekPosition;
    bool m_changingRate;
    bool m_downloadFinished;
    bool m_errorOccured;
    mutable bool m_isEndReached;
    mutable bool m_isStreaming;
    mutable MediaTime m_durationAtEOS;
    bool m_paused;
    float m_playbackRate;
    GstState m_currentState;
    GstState m_oldState;
    GstState m_requestedState;
    bool m_resetPipeline;
    bool m_seeking;
    bool m_seekIsPending;
    MediaTime m_seekTime;
    GRefPtr<GstElement> m_source;
    bool m_volumeAndMuteInitialized;

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

private:

#if ENABLE(VIDEO_TRACK)
    GRefPtr<GstElement> m_textAppSink;
    GRefPtr<GstPad> m_textAppSinkPad;
#endif
    GstStructure* m_mediaLocations;
    int m_mediaLocationCurrentIndex;
    bool m_playbackRatePause;
    MediaTime m_timeOfOverlappingSeek;
    float m_lastPlaybackRate;
    Timer m_fillTimer;
    MediaTime m_maxTimeLoaded;
    bool m_loadingStalled { false };
    MediaPlayer::Preload m_preload;
    bool m_delayingLoad;
    mutable MediaTime m_maxTimeLoadedAtLastDidLoadingProgress;
    bool m_hasVideo;
    bool m_hasAudio;
    RunLoop::Timer<MediaPlayerPrivateGStreamer> m_readyTimerHandler;
    mutable unsigned long long m_totalBytes;
    URL m_url;
    bool m_preservesPitch;
    mutable std::optional<Seconds> m_lastQueryTime;
    bool m_isLegacyPlaybin;
#if GST_CHECK_VERSION(1, 10, 0)
    GRefPtr<GstStreamCollection> m_streamCollection;
    FloatSize naturalSize() const final;
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_streamPrivate;
#endif // ENABLE(MEDIA_STREAM)
#endif // GST_CHECK_VERSION(1, 10, 0)
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
    HashMap<AtomicString, RefPtr<AudioTrackPrivateGStreamer>> m_audioTracks;
    HashMap<AtomicString, RefPtr<InbandTextTrackPrivateGStreamer>> m_textTracks;
    HashMap<AtomicString, RefPtr<VideoTrackPrivateGStreamer>> m_videoTracks;
    RefPtr<InbandMetadataTextTrackPrivateGStreamer> m_chaptersTrack;
#if USE(GSTREAMER_MPEGTS)
    HashMap<AtomicString, RefPtr<InbandMetadataTextTrackPrivateGStreamer>> m_metadataTracks;
#endif
#endif
    virtual bool isMediaSource() const { return false; }

    std::optional<bool> m_hasTaintedOrigin { std::nullopt };
};
}

#endif // USE(GSTREAMER)
