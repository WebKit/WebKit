/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#ifndef MediaController_h
#define MediaController_h

#if ENABLE(VIDEO)

#include "Event.h"
#include "EventTarget.h"
#include "MediaControllerInterface.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Clock;
class HTMLMediaElement;
class Event;
class ScriptExecutionContext;

class MediaController FINAL : public RefCounted<MediaController>, public MediaControllerInterface, public EventTargetWithInlineData {
public:
    static PassRefPtr<MediaController> create(ScriptExecutionContext*);
    virtual ~MediaController();

    void addMediaElement(HTMLMediaElement*);
    void removeMediaElement(HTMLMediaElement*);
    bool containsMediaElement(HTMLMediaElement*) const;

    const String& mediaGroup() const { return m_mediaGroup; }
    
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> seekable() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> played() OVERRIDE;
    
    virtual double duration() const OVERRIDE;
    virtual double currentTime() const OVERRIDE;
    virtual void setCurrentTime(double, ExceptionCode&) OVERRIDE;
    
    virtual bool paused() const OVERRIDE { return m_paused; }
    virtual void play() OVERRIDE;
    virtual void pause() OVERRIDE;
    void unpause();
    
    virtual double defaultPlaybackRate() const OVERRIDE { return m_defaultPlaybackRate; }
    virtual void setDefaultPlaybackRate(double) OVERRIDE;
    
    virtual double playbackRate() const OVERRIDE;
    virtual void setPlaybackRate(double) OVERRIDE;
    
    virtual double volume() const OVERRIDE { return m_volume; }
    virtual void setVolume(double, ExceptionCode&) OVERRIDE;
    
    virtual bool muted() const OVERRIDE { return m_muted; }
    virtual void setMuted(bool) OVERRIDE;
    
    virtual ReadyState readyState() const OVERRIDE { return m_readyState; }

    enum PlaybackState { WAITING, PLAYING, ENDED };
    const AtomicString& playbackState() const;

    virtual bool supportsFullscreen() const OVERRIDE { return false; }
    virtual bool isFullscreen() const OVERRIDE { return false; }
    virtual void enterFullscreen() OVERRIDE { }

    virtual bool hasAudio() const OVERRIDE;
    virtual bool hasVideo() const OVERRIDE;
    virtual bool hasClosedCaptions() const OVERRIDE;
    virtual void setClosedCaptionsVisible(bool) OVERRIDE;
    virtual bool closedCaptionsVisible() const OVERRIDE { return m_closedCaptionsVisible; }
    
    virtual bool supportsScanning() const OVERRIDE;
    
    virtual void beginScrubbing() OVERRIDE;
    virtual void endScrubbing() OVERRIDE;
    
    virtual bool canPlay() const OVERRIDE;
    
    virtual bool isLiveStream() const OVERRIDE;
    
    virtual bool hasCurrentSrc() const OVERRIDE;
    
    virtual void returnToRealtime() OVERRIDE;

    bool isBlocked() const;

    // EventTarget
    using RefCounted<MediaController>::ref;
    using RefCounted<MediaController>::deref;

private:
    explicit MediaController(ScriptExecutionContext*);
    void reportControllerState();
    void updateReadyState();
    void updatePlaybackState();
    void updateMediaElements();
    void bringElementUpToSpeed(HTMLMediaElement*);
    void scheduleEvent(const AtomicString& eventName);
    void asyncEventTimerFired(Timer<MediaController>*);
    void clearPositionTimerFired(Timer<MediaController>*);
    bool hasEnded() const;
    void scheduleTimeupdateEvent();
    void timeupdateTimerFired(Timer<MediaController>*);
    void startTimeupdateTimer();

    // EventTarget
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
    virtual EventTargetInterface eventTargetInterface() const OVERRIDE { return MediaControllerEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE { return m_scriptExecutionContext; };

    friend class HTMLMediaElement;
    friend class MediaControllerEventListener;
    Vector<HTMLMediaElement*> m_mediaElements;
    bool m_paused;
    double m_defaultPlaybackRate;
    double m_volume;
    mutable double m_position;
    bool m_muted;
    ReadyState m_readyState;
    PlaybackState m_playbackState;
    Vector<RefPtr<Event> > m_pendingEvents;
    Timer<MediaController> m_asyncEventTimer;
    mutable Timer<MediaController> m_clearPositionTimer;
    String m_mediaGroup;
    bool m_closedCaptionsVisible;
    PassRefPtr<Clock> m_clock;
    ScriptExecutionContext* m_scriptExecutionContext;
    Timer<MediaController> m_timeupdateTimer;
    double m_previousTimeupdateTime;
};

} // namespace WebCore

#endif
#endif
