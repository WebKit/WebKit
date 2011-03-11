/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef MediaPlayerPrivateAVFoundation_h
#define MediaPlayerPrivateAVFoundation_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "FloatSize.h"
#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include <wtf/RetainPtr.h>

namespace WebCore {

class MediaPlayerPrivateAVFoundation : public MediaPlayerPrivateInterface {
public:

    virtual void repaint();
    virtual void metadataLoaded();
    virtual void loadStateChanged();
    virtual void playabilityKnown();
    virtual void rateChanged();
    virtual void loadedTimeRangesChanged();
    virtual void seekableTimeRangesChanged();
    virtual void timeChanged(double);
    virtual void didEnd();

    class Notification {
    public:
        enum Type {
            None,
            ItemDidPlayToEndTime,
            ItemTracksChanged,
            ItemStatusChanged,
            ItemSeekableTimeRangesChanged,
            ItemLoadedTimeRangesChanged,
            ItemPresentationSizeChanged,
            ItemIsPlaybackLikelyToKeepUpChanged,
            ItemIsPlaybackBufferEmptyChanged,
            ItemIsPlaybackBufferFullChanged,
            AssetMetadataLoaded,
            AssetPlayabilityKnown,
            PlayerRateChanged,
            PlayerTimeChanged
        };
        
        Notification()
            : m_type(None)
            , m_time(0)
        {
        }

        Notification(Type type, double time)
            : m_type(type)
            , m_time(time)
        {
        }
        
        Type type() { return m_type; }
        bool isValid() { return m_type != None; }
        double time() { return m_time; }
        
    private:
        Type m_type;
        double m_time;
    };

    void scheduleMainThreadNotification(Notification::Type, double time = 0);
    void dispatchNotification();
    void clearMainThreadPendingFlag();

protected:
    MediaPlayerPrivateAVFoundation(MediaPlayer*);
    virtual ~MediaPlayerPrivateAVFoundation();

    // MediaPlayerPrivatePrivateInterface overrides.
    virtual void load(const String& url);
    virtual void cancelLoad() = 0;

    virtual void prepareToPlay();
    virtual PlatformMedia platformMedia() const = 0;

    virtual void play() = 0;
    virtual void pause() = 0;

    virtual IntSize naturalSize() const;
    virtual bool hasVideo() const { return m_cachedHasVideo; }
    virtual bool hasAudio() const { return m_cachedHasAudio; }
    virtual void setVisible(bool);
    virtual float duration() const;
    virtual float currentTime() const = 0;
    virtual void seek(float);
    virtual bool seeking() const;
    virtual void setRate(float);
    virtual bool paused() const;
    virtual void setVolume(float) = 0;
    virtual bool hasClosedCaptions() const { return m_cachedHasCaptions; }
    virtual void setClosedCaptionsVisible(bool) = 0;
    virtual MediaPlayer::NetworkState networkState() const { return m_networkState; }
    virtual MediaPlayer::ReadyState readyState() const { return m_readyState; }
    virtual float maxTimeSeekable() const;
    virtual PassRefPtr<TimeRanges> buffered() const;
    virtual unsigned bytesLoaded() const;
    virtual void setSize(const IntSize&);
    virtual void paint(GraphicsContext*, const IntRect&);
    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) = 0;
    virtual void setPreload(MediaPlayer::Preload);
    virtual bool hasAvailableVideoFrame() const;
#if USE(ACCELERATED_COMPOSITING)
    virtual PlatformLayer* platformLayer() const { return 0; }
    virtual bool supportsAcceleratedRendering() const = 0;
    virtual void acceleratedRenderingStateChanged();
#endif
    virtual bool hasSingleSecurityOrigin() const { return true; }
    virtual MediaPlayer::MovieLoadType movieLoadType() const;
    virtual void prepareForRendering();
    virtual float mediaTimeForTimeValue(float) const = 0;

    // FIXME: WebVideoFullscreenController assumes a QTKit/QuickTime media engine
    virtual bool supportsFullscreen() const { return false; }

    // Required interfaces for concrete derived classes.
    virtual void createAVPlayerForURL(const String& url) = 0;

    enum ItemStatus {
        MediaPlayerAVPlayerItemStatusUnknown,
        MediaPlayerAVPlayerItemStatusFailed,
        MediaPlayerAVPlayerItemStatusReadyToPlay,
        MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty,
        MediaPlayerAVPlayerItemStatusPlaybackBufferFull,
        MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp,
    };
    virtual ItemStatus playerItemStatus() const = 0;

    enum AVAssetStatus {
        MediaPlayerAVAssetStatusUnknown,
        MediaPlayerAVAssetStatusLoading,
        MediaPlayerAVAssetStatusFailed,
        MediaPlayerAVAssetStatusCancelled,
        MediaPlayerAVAssetStatusLoaded,
        MediaPlayerAVAssetStatusPlayable,
    };
    virtual AVAssetStatus assetStatus() const = 0;

    virtual void checkPlayability() = 0;
    virtual float rate() const = 0;
    virtual void seekToTime(float time) = 0;
    virtual unsigned totalBytes() const = 0;
    virtual PassRefPtr<TimeRanges> platformBufferedTimeRanges() const = 0;
    virtual float platformMaxTimeSeekable() const = 0;
    virtual float platformMaxTimeLoaded() const = 0;
    virtual float platformDuration() const = 0;

    virtual void beginLoadingMetadata() = 0;
    virtual void tracksChanged() = 0;
    virtual void sizeChanged() = 0;

    virtual void createContextVideoRenderer() = 0;
    virtual void destroyContextVideoRenderer() = 0;

    virtual void createVideoLayer() = 0;
    virtual void destroyVideoLayer() = 0;
    virtual bool videoLayerIsReadyToDisplay() const = 0;

    virtual bool hasContextRenderer() const = 0;
    virtual bool hasLayerRenderer() const = 0;

protected:
    void resumeLoad();
    void updateStates();

    void setHasVideo(bool b) { m_cachedHasVideo = b; };
    void setHasAudio(bool b) { m_cachedHasAudio = b; }
    void setHasClosedCaptions(bool b) { m_cachedHasCaptions = b; }
    void setDelayCallbacks(bool);
    void setIgnoreLoadStateChanges(bool delay) { m_ignoreLoadStateChanges = delay; }
    void setNaturalSize(IntSize);

    enum MediaRenderingMode { MediaRenderingNone, MediaRenderingToContext, MediaRenderingToLayer };
    MediaRenderingMode currentRenderingMode() const;
    MediaRenderingMode preferredRenderingMode() const;

    bool metaDataAvailable() const { return m_readyState >= MediaPlayer::HaveMetadata; }
    float requestedRate() const { return m_requestedRate; }
    float maxTimeLoaded() const;
    bool isReadyForVideoSetup() const;
    virtual void setUpVideoRendering();
    virtual void tearDownVideoRendering();
    bool hasSetUpVideoRendering() const;

    static void mainThreadCallback(void*);

private:

    MediaPlayer* m_player;

    Vector<Notification> m_queuedNotifications;
    Mutex m_queueMutex;
    bool m_mainThreadCallPending;

    mutable RefPtr<TimeRanges> m_cachedLoadedTimeRanges;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;

    String m_assetURL;
    MediaPlayer::Preload m_preload;
    FloatSize m_scaleFactor;

    IntSize m_cachedNaturalSize;
    mutable float m_cachedMaxTimeLoaded;
    mutable float m_cachedMaxTimeSeekable;
    mutable float m_cachedDuration;
    float m_reportedDuration;

    float m_seekTo;
    float m_requestedRate;
    int m_delayCallbacks;
    bool m_havePreparedToPlay;
    bool m_assetIsPlayable;
    bool m_visible;
    bool m_videoFrameHasDrawn;
    bool m_loadingMetadata;
    bool m_delayingLoad;
    bool m_isAllowedToRender;
    bool m_cachedHasAudio;
    bool m_cachedHasVideo;
    bool m_cachedHasCaptions;
    bool m_ignoreLoadStateChanges;
};

}

#endif
#endif
