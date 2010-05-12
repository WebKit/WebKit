/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef MediaPlayer_h
#define MediaPlayer_h

#if ENABLE(VIDEO)

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "MediaPlayerProxy.h"
#endif

#include "Document.h"
#include "IntRect.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayer.h"
#endif

#ifdef __OBJC__
@class QTMovie;
#else
class QTMovie;
#endif
class QTMovieGWorld;
class QTMovieVisualContext;

namespace WebCore {

// Structure that will hold every native
// types supported by the current media player.
// We have to do that has multiple media players
// backend can live at runtime.
struct PlatformMedia {
    enum {
        None,
        QTMovieType,
        QTMovieGWorldType,
        QTMovieVisualContextType
    } type;

    union {
        QTMovie* qtMovie;
        QTMovieGWorld* qtMovieGWorld;
        QTMovieVisualContext* qtMovieVisualContext;
    } media;
};

extern const PlatformMedia NoPlatformMedia;

class ContentType;
class FrameView;
class GraphicsContext;
class IntRect;
class IntSize;
class MediaPlayer;
class MediaPlayerPrivateInterface;
class String;
class TimeRanges;

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() { }

    // Get the document which the media player is owned by
    virtual Document* mediaPlayerOwningDocument() { return 0; }

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*) { }

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*) { }

    // the volume state has changed
    virtual void mediaPlayerVolumeChanged(MediaPlayer*) { }

    // the mute state has changed
    virtual void mediaPlayerMuteChanged(MediaPlayer*) { }

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged(MediaPlayer*) { }
    
    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged(MediaPlayer*) { }
    
    // the playback rate has changed
    virtual void mediaPlayerRateChanged(MediaPlayer*) { }

    // The MediaPlayer has found potentially problematic media content.
    // This is used internally to trigger swapping from a <video>
    // element to an <embed> in standalone documents
    virtual void mediaPlayerSawUnsupportedTracks(MediaPlayer*) { }

// Presentation-related methods
    // a new frame of video is available
    virtual void mediaPlayerRepaint(MediaPlayer*) { }

    // the movie size has changed
    virtual void mediaPlayerSizeChanged(MediaPlayer*) { }

#if USE(ACCELERATED_COMPOSITING)
    // whether the rendering system can accelerate the display of this MediaPlayer.
    virtual bool mediaPlayerRenderingCanBeAccelerated(MediaPlayer*) { return false; }

    // called when the media player's rendering mode changed, which indicates a change in the
    // availability of the platformLayer().
    virtual void mediaPlayerRenderingModeChanged(MediaPlayer*) { }
#endif
};

class MediaPlayer : public Noncopyable {
public:

    static PassOwnPtr<MediaPlayer> create(MediaPlayerClient* client)
    {
        return new MediaPlayer(client);
    }
    virtual ~MediaPlayer();

    // media engine support
    enum SupportsType { IsNotSupported, IsSupported, MayBeSupported };
    static MediaPlayer::SupportsType supportsType(ContentType contentType);
    static void getSupportedTypes(HashSet<String>&);
    static bool isAvailable();

    bool supportsFullscreen() const;
    bool supportsSave() const;
    PlatformMedia platformMedia() const;
#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer() const;
#endif

    IntSize naturalSize();
    bool hasVideo() const;
    bool hasAudio() const;
    
    void setFrameView(FrameView* frameView) { m_frameView = frameView; }
    FrameView* frameView() { return m_frameView; }
    bool inMediaDocument();
    
    IntSize size() const { return m_size; }
    void setSize(const IntSize& size);
    
    void load(const String& url, const ContentType& contentType);
    void cancelLoad();
    
    bool visible() const;
    void setVisible(bool);
    
    void prepareToPlay();
    void play();
    void pause();    
    
    bool paused() const;
    bool seeking() const;
    
    float duration() const;
    float currentTime() const;
    void seek(float time);

    float startTime() const;
    
    float rate() const;
    void setRate(float);

    bool preservesPitch() const;    
    void setPreservesPitch(bool);
    
    PassRefPtr<TimeRanges> buffered();
    float maxTimeSeekable();

    unsigned bytesLoaded();
    
    float volume() const;
    void setVolume(float);

    bool muted() const;
    void setMuted(bool);

    bool hasClosedCaptions() const;
    void setClosedCaptionsVisible(bool closedCaptionsVisible);

    bool autoplay() const;    
    void setAutoplay(bool);

    void paint(GraphicsContext*, const IntRect&);
    void paintCurrentFrameInContext(GraphicsContext*, const IntRect&);
    
    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    NetworkState networkState();

    enum ReadyState  { HaveNothing, HaveMetadata, HaveCurrentData, HaveFutureData, HaveEnoughData };
    ReadyState readyState();
    
    enum MovieLoadType { Unknown, Download, StoredStream, LiveStream };
    MovieLoadType movieLoadType() const;

    enum Preload { None, MetaData, Auto };
    Preload preload() const;
    void setPreload(Preload);

    void networkStateChanged();
    void readyStateChanged();
    void volumeChanged(float);
    void muteChanged(bool);
    void timeChanged();
    void sizeChanged();
    void rateChanged();
    void durationChanged();

    void repaint();

    MediaPlayerClient* mediaPlayerClient() const { return m_mediaPlayerClient; }

    bool hasAvailableVideoFrame() const;

    bool canLoadPoster() const;
    void setPoster(const String&);

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    void deliverNotification(MediaPlayerProxyNotificationType notification);
    void setMediaPlayerProxy(WebMediaPlayerProxy* proxy);
#endif

#if USE(ACCELERATED_COMPOSITING)
    // whether accelerated rendering is supported by the media engine for the current media.
    bool supportsAcceleratedRendering() const;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged();
#endif

    bool hasSingleSecurityOrigin() const;

private:
    MediaPlayer(MediaPlayerClient*);

    static void initializeMediaEngines();

    MediaPlayerClient* m_mediaPlayerClient;
    OwnPtr<MediaPlayerPrivateInterface*> m_private;
    void* m_currentMediaEngine;
    FrameView* m_frameView;
    IntSize m_size;
    Preload m_preload;
    bool m_visible;
    float m_rate;
    float m_volume;
    bool m_muted;
    bool m_preservesPitch;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    WebMediaPlayerProxy* m_playerProxy;    // not owned or used, passed to m_private
#endif
};

typedef MediaPlayerPrivateInterface* (*CreateMediaEnginePlayer)(MediaPlayer*);
typedef void (*MediaEngineSupportedTypes)(HashSet<String>& types);
typedef MediaPlayer::SupportsType (*MediaEngineSupportsType)(const String& type, const String& codecs);

typedef void (*MediaEngineRegistrar)(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType); 


}

#endif // ENABLE(VIDEO)

#endif
