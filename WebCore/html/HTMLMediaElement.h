/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#if ENABLE(VIDEO)

#include "HTMLElement.h"
#include "MediaPlayer.h"
#include "Timer.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "MediaPlayerProxy.h"
#endif

namespace WebCore {

class Event;
class HTMLSourceElement;
class MediaError;
class KURL;
class TimeRanges;

class HTMLMediaElement : public HTMLElement, public MediaPlayerClient {
public:
    HTMLMediaElement(const QualifiedName&, Document*);
    virtual ~HTMLMediaElement();

    bool checkDTD(const Node* newChild);
    
    void attributeChanged(Attribute*, bool preserveDecls);
    void parseMappedAttribute(MappedAttribute *);

    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void willRemove();
    virtual void removedFromDocument();
    virtual void attach();
    virtual void recalcStyle(StyleChange);
    
    MediaPlayer* player() const { return m_player.get(); }
    
    virtual bool isVideo() const { return false; }
    virtual bool hasVideo() const { return false; }
    virtual bool hasAudio() const;

    void rewind(float timeDelta);
    void returnToRealtime();

    // Eventually overloaded in HTMLVideoElement
    virtual bool supportsFullscreen() const { return false; };
    virtual bool supportsSave() const;
    
    PlatformMedia platformMedia() const;

    void scheduleLoad();
    
    virtual void defaultEventHandler(Event*);
    
    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);
    
    MediaPlayer::MovieLoadType movieLoadType() const;
    
    bool inActiveDocument() const { return m_inActiveDocument; }
    
// DOM API
// error state
    PassRefPtr<MediaError> error() const;

// network state
    KURL src() const;
    void setSrc(const String&);
    String currentSrc() const;

    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_LOADED, NETWORK_NO_SOURCE };
    NetworkState networkState() const;
    bool autobuffer() const;    
    void setAutobuffer(bool);

    PassRefPtr<TimeRanges> buffered() const;
    void load(ExceptionCode&);
    String canPlayType(const String& mimeType) const;

// ready state
    enum ReadyState { HAVE_NOTHING, HAVE_METADATA, HAVE_CURRENT_DATA, HAVE_FUTURE_DATA, HAVE_ENOUGH_DATA };
    ReadyState readyState() const;
    bool seeking() const;

// playback state
    float currentTime() const;
    void setCurrentTime(float, ExceptionCode&);
    float startTime() const;
    float duration() const;
    bool paused() const;
    float defaultPlaybackRate() const;
    void setDefaultPlaybackRate(float);
    float playbackRate() const;
    void setPlaybackRate(float);
    bool webkitPreservesPitch() const;
    void setWebkitPreservesPitch(bool);
    PassRefPtr<TimeRanges> played();
    PassRefPtr<TimeRanges> seekable() const;
    bool ended() const;
    bool autoplay() const;    
    void setAutoplay(bool b);
    bool loop() const;    
    void setLoop(bool b);
    void play();
    void pause();
    
// controls
    bool controls() const;
    void setControls(bool);
    float volume() const;
    void setVolume(float, ExceptionCode&);
    bool muted() const;
    void setMuted(bool);
    void togglePlayState();
    void beginScrubbing();
    void endScrubbing();

    const IntRect screenRect();

    bool canPlay() const;

    float percentLoaded() const;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    void setNeedWidgetUpdate(bool needWidgetUpdate) { m_needWidgetUpdate = needWidgetUpdate; }
    void deliverNotification(MediaPlayerProxyNotificationType notification);
    void setMediaPlayerProxy(WebMediaPlayerProxy* proxy);
    String initialURL();
    virtual void finishParsingChildren();
#endif

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }
    
    void enterFullscreen();
    void exitFullscreen();

protected:
    float getTimeOffsetAttribute(const QualifiedName&, float valueOnError) const;
    void setTimeOffsetAttribute(const QualifiedName&, float value);
    
    virtual void documentWillBecomeInactive();
    virtual void documentDidBecomeActive();
    virtual void mediaVolumeDidChange();
    
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);
    
private: // MediaPlayerClient
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*);
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*);
    virtual void mediaPlayerTimeChanged(MediaPlayer*);
    virtual void mediaPlayerVolumeChanged(MediaPlayer*);
    virtual void mediaPlayerDurationChanged(MediaPlayer*);
    virtual void mediaPlayerRateChanged(MediaPlayer*);
    virtual void mediaPlayerSawUnsupportedTracks(MediaPlayer*);
    virtual void mediaPlayerRepaint(MediaPlayer*);
    virtual void mediaPlayerSizeChanged(MediaPlayer*);
#if USE(ACCELERATED_COMPOSITING)
    virtual bool mediaPlayerRenderingCanBeAccelerated(MediaPlayer*);
    virtual GraphicsLayer* mediaPlayerGraphicsLayer(MediaPlayer*);
#endif

private:
    void loadTimerFired(Timer<HTMLMediaElement>*);
    void asyncEventTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>*);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(float time, ExceptionCode&);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(float start, float end);
    
    void scheduleTimeupdateEvent(bool periodicEvent);
    void scheduleProgressEvent(const AtomicString& eventName);
    void scheduleEvent(const AtomicString& eventName);
    void enqueueEvent(RefPtr<Event> event);
    
    // loading
    void selectMediaResource();
    void loadResource(const KURL&, ContentType&);
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    bool havePotentialSourceChild();
    void noneSupported();
    void mediaEngineError(PassRefPtr<MediaError> err);
    void cancelPendingEventsAndCallbacks();

    enum InvalidSourceAction { DoNothing, Complain };
    bool isSafeToLoadURL(const KURL&, InvalidSourceAction);
    KURL selectNextSourceChild(ContentType*, InvalidSourceAction);

    // These "internal" functions do not check user gesture restrictions.
    void loadInternal();
    void playInternal();
    void pauseInternal();

    void prepareForLoad();
    
    bool processingUserGesture() const;
    bool processingMediaPlayerCallback() const { return m_processingMediaPlayerCallback > 0; }
    void beginProcessingMediaPlayerCallback() { ++m_processingMediaPlayerCallback; }
    void endProcessingMediaPlayerCallback() { ASSERT(m_processingMediaPlayerCallback); --m_processingMediaPlayerCallback; }

    void updateVolume();
    void updatePlayState();
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;

    float minTimeSeekable() const;
    float maxTimeSeekable() const;

    // Restrictions to change default behaviors. This is a effectively a compile time choice at the moment
    //  because there are no accessor methods.
    enum BehaviorRestrictions {
        NoRestrictions = 0,
        RequireUserGestureForLoadRestriction = 1 << 0,
        RequireUserGestureForRateChangeRestriction = 1 << 1,
    };

protected:
    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_asyncEventTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    Vector<RefPtr<Event> > m_pendingEvents;
    RefPtr<TimeRanges> m_playedTimeRanges;
    
    float m_playbackRate;
    float m_defaultPlaybackRate;
    bool m_webkitPreservesPitch;
    NetworkState m_networkState;
    ReadyState m_readyState;
    String m_currentSrc;
    
    RefPtr<MediaError> m_error;

    float m_volume;
    float m_lastSeekTime;
    
    unsigned m_previousProgress;
    double m_previousProgressTime;

    // the last time a timeupdate event was sent (wall clock)
    double m_lastTimeUpdateEventWallTime;

    // the last time a timeupdate event was sent in movie time
    float m_lastTimeUpdateEventMovieTime;
    
    // loading state
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    HTMLSourceElement *m_currentSourceNode;
    
    OwnPtr<MediaPlayer> m_player;

    BehaviorRestrictions m_restrictions;

    bool m_playing;

    // counter incremented while processing a callback from the media player, so we can avoid
    //  calling the media engine recursively
    int m_processingMediaPlayerCallback;

    bool m_processingLoad : 1;
    bool m_delayingTheLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_inActiveDocument : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_pausedInternal : 1;

    // Not all media engines provide enough information about a file to be able to
    // support progress events so setting m_sendProgressEvents disables them 
    bool m_sendProgressEvents : 1;

    bool m_isFullscreen : 1;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    bool m_needWidgetUpdate : 1;
#endif
};

} //namespace

#endif
#endif
