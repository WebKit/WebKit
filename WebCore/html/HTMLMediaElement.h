/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "VoidCallback.h"

namespace WebCore {

class MediaError;
class TimeRanges;
    
class HTMLMediaElement : public HTMLElement, public MediaPlayerClient {
public:
    HTMLMediaElement(const QualifiedName&, Document*);
    virtual ~HTMLMediaElement();

    bool checkDTD(const Node* newChild);
    
    void attributeChanged(Attribute*, bool preserveDecls);

    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void attach();
    
    MediaPlayer* player() const { return m_player.get(); }
    
    virtual bool isVideo() const { return false; }
    
    void scheduleLoad();
    
    virtual void defaultEventHandler(Event*);
    
    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);
    
    bool inPageCache() const { return m_inPageCache; }
    
// DOM API
// error state
    PassRefPtr<MediaError> error() const;

// network state
    String src() const;
    void setSrc(const String&);
    String currentSrc() const;
    
    enum NetworkState { EMPTY, LOADING, LOADED_METADATA, LOADED_FIRST_FRAME, LOADED };
    NetworkState networkState() const;
    float bufferingRate();
    PassRefPtr<TimeRanges> buffered() const;
    void load(ExceptionCode&);

// ready state
    enum ReadyState { DATA_UNAVAILABLE, CAN_SHOW_CURRENT_FRAME, CAN_PLAY, CAN_PLAY_THROUGH };
    ReadyState readyState() const;
    bool seeking() const;

// playback state
    float currentTime() const;
    void setCurrentTime(float, ExceptionCode&);
    float duration() const;
    bool paused() const;
    float defaultPlaybackRate() const;
    void setDefaultPlaybackRate(float, ExceptionCode&);
    float playbackRate() const;
    void setPlaybackRate(float, ExceptionCode&);
    PassRefPtr<TimeRanges> played() const;
    PassRefPtr<TimeRanges> seekable() const;
    bool ended() const;
    bool autoplay() const;    
    void setAutoplay(bool b);
    void play(ExceptionCode&);
    void pause(ExceptionCode&);
    
// looping
    float start() const;
    void setStart(float time);
    float end() const;
    void setEnd(float time);
    float loopStart() const;
    void setLoopStart(float time);
    float loopEnd() const;
    void setLoopEnd(float time);
    unsigned playCount() const;
    void setPlayCount(unsigned, ExceptionCode&);
    unsigned currentLoop() const;
    void setCurrentLoop(unsigned);

// controls
    bool controls() const;
    void setControls(bool);
    float volume() const;
    void setVolume(float, ExceptionCode&);
    bool muted() const;
    void setMuted(bool);

    bool canPlay() const;

protected:
    float getTimeOffsetAttribute(const QualifiedName&, float valueOnError) const;
    void setTimeOffsetAttribute(const QualifiedName&, float value);
    
    virtual void willSaveToCache();
    virtual void didRestoreFromCache();
    
    void initAndDispatchProgressEvent(const AtomicString& eventName);
    void dispatchEventAsync(const AtomicString& eventName);
    
    void setReadyState(ReadyState);
    
private: // MediaPlayerObserver
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*);
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*);
    virtual void mediaPlayerTimeChanged(MediaPlayer*);
    virtual void mediaPlayerRepaint(MediaPlayer*);

private:
    void loadTimerFired(Timer<HTMLMediaElement>*);
    void asyncEventTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void seek(float time, ExceptionCode& ec);
    void checkIfSeekNeeded();
    
    String pickMedia();
    void updateVolume();
    void updatePlayState();
    float effectiveStart() const;
    float effectiveEnd() const;
    float effectiveLoopStart() const;
    float effectiveLoopEnd() const;
    bool activelyPlaying() const;
    bool endedPlayback() const;
    
protected:
    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_asyncEventTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Vector<AtomicString> m_asyncEventsToDispatch;
    
    float m_defaultPlaybackRate;
    NetworkState m_networkState;
    ReadyState m_readyState;
    String m_currentSrc;
    
    RefPtr<MediaError> m_error;
    
    bool m_begun;
    bool m_loadedFirstFrame;
    bool m_autoplaying;
    
    unsigned m_currentLoop;
    float m_volume;
    bool m_muted;
    
    bool m_paused;
    bool m_seeking;
    
    float m_currentTimeDuringSeek;
    
    unsigned m_previousProgress;
    double m_previousProgressTime;
    bool m_sentStalledEvent;
    
    float m_bufferingRate;
    
    unsigned m_loadNestingLevel;
    unsigned m_terminateLoadBelowNestingLevel;
    
    bool m_pausedInternal;
    bool m_inPageCache;

    OwnPtr<MediaPlayer> m_player;
};

} //namespace

#endif
#endif
