/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef MediaPlayerPrivateQTKit_h
#define MediaPlayerPrivateQTKit_h

#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include "FloatSize.h"
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
#import <QTKit/QTTime.h>
#else
class QTTime;
#endif

OBJC_CLASS NSDictionary;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS QTMovie;
OBJC_CLASS QTMovieLayer;
OBJC_CLASS QTVideoRendererWebKitOnly;
OBJC_CLASS WebCoreMovieObserver;

namespace WebCore {
    
class MediaPlayerPrivateQTKit : public MediaPlayerPrivateInterface {
public:
    explicit MediaPlayerPrivateQTKit(MediaPlayer*);
    ~MediaPlayerPrivateQTKit();
    WEBCORE_EXPORT static void registerMediaEngine(MediaEngineRegistrar);

    void repaint();
    void loadStateChanged();
    void loadedRangesChanged();
    void rateChanged();
    void sizeChanged();
    void timeChanged();
    void didEnd();
    void layerHostChanged(PlatformLayer* rootLayer);

private:
    // engine support
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    static void getSitesInMediaCache(Vector<String>&);
    static void clearMediaCache();
    static void clearMediaCacheForSite(const String&);
    static bool isAvailable();

    PlatformMedia platformMedia() const override;
    PlatformLayer* platformLayer() const override;

    FloatSize naturalSize() const override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    bool supportsFullscreen() const override;
    virtual bool supportsScanning() const override { return true; }
    
    void load(const String& url) override;
#if ENABLE(MEDIA_SOURCE)
    virtual void load(const String&, MediaSourcePrivateClient*) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override { }
#endif
    void cancelLoad() override;
    void loadInternal(const String& url);
    void resumeLoad();
    
    void play() override;
    void pause() override;
    void prepareToPlay() override;
    
    bool paused() const override;
    bool seeking() const override;
    
    virtual MediaTime durationMediaTime() const override;
    virtual MediaTime currentMediaTime() const override;
    virtual void seek(const MediaTime&) override;
    
    void setRate(float) override;
    virtual double rate() const override;
    void setVolume(float) override;
    void setPreservesPitch(bool) override;

    bool hasClosedCaptions() const override;
    void setClosedCaptionsVisible(bool) override;

    void setPreload(MediaPlayer::Preload) override;

    MediaPlayer::NetworkState networkState() const override { return m_networkState; }
    MediaPlayer::ReadyState readyState() const override { return m_readyState; }
    
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    MediaTime maxMediaTimeSeekable() const override;
    bool didLoadingProgress() const override;
    unsigned long long totalBytes() const override;
    
    void setVisible(bool) override;
    void setSize(const IntSize&) override;
    
    virtual bool hasAvailableVideoFrame() const override;

    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    virtual void prepareForRendering() override;

    bool supportsAcceleratedRendering() const override;
    void acceleratedRenderingStateChanged() override;

    bool hasSingleSecurityOrigin() const override;
    MediaPlayer::MovieLoadType movieLoadType() const override;

    virtual bool canSaveMediaData() const override;

    void createQTMovie(const String& url);
    void createQTMovie(NSURL *, NSDictionary *movieAttributes);

    enum MediaRenderingMode { MediaRenderingNone, MediaRenderingSoftwareRenderer, MediaRenderingMovieLayer };
    MediaRenderingMode currentRenderingMode() const;
    MediaRenderingMode preferredRenderingMode() const;
    
    void setUpVideoRendering();
    void tearDownVideoRendering();
    bool hasSetUpVideoRendering() const;
    
    enum QTVideoRendererMode { QTVideoRendererModeDefault, QTVideoRendererModeListensForNewImages };
    void createQTVideoRenderer(QTVideoRendererMode rendererMode);
    void destroyQTVideoRenderer();
    
    void createQTMovieLayer();
    void destroyQTMovieLayer();

    void updateStates();
    void doSeek();
    void cancelSeek();
    void seekTimerFired();
    MediaTime maxMediaTimeLoaded() const;
    void disableUnsupportedTracks();
    
    void sawUnsupportedTracks();
    void cacheMovieScale();
    bool metaDataAvailable() const { return m_qtMovie && m_readyState >= MediaPlayer::HaveMetadata; }

    bool isReadyForVideoSetup() const;

    virtual double maximumDurationToCacheMediaTime() const override { return 5; }

    virtual void setPrivateBrowsingMode(bool) override;
    
    NSMutableDictionary* commonMovieAttributes();

    virtual String engineDescription() const override { return "QTKit"; }
    virtual long platformErrorCode() const override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual bool isCurrentPlaybackTargetWireless() const override;
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    virtual void setShouldPlayToPlaybackTarget(bool) override;
    bool wirelessVideoPlaybackDisabled() const override { return false; }
#endif

    MediaPlayer* m_player;
    RetainPtr<QTMovie> m_qtMovie;
    RetainPtr<QTVideoRendererWebKitOnly> m_qtVideoRenderer;
    RetainPtr<WebCoreMovieObserver> m_objcObserver;
    String m_movieURL;
    MediaTime m_seekTo;
    Timer m_seekTimer;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    IntRect m_rect;
    FloatSize m_scaleFactor;
    unsigned m_enabledTrackCount;
    unsigned m_totalTrackCount;
    MediaTime m_reportedDuration;
    MediaTime m_cachedDuration;
    MediaTime m_timeToRestore;
    RetainPtr<QTMovieLayer> m_qtVideoLayer;
    MediaPlayer::Preload m_preload;
    bool m_startedPlaying;
    bool m_isStreaming;
    bool m_visible;
    bool m_hasUnsupportedTracks;
    bool m_videoFrameHasDrawn;
    bool m_isAllowedToRender;
    bool m_privateBrowsing;
    mutable MediaTime m_maxTimeLoadedAtLastDidLoadingProgress;
    mutable FloatSize m_cachedNaturalSize;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_shouldPlayToTarget { false };
#endif
};

}

#endif
#endif
