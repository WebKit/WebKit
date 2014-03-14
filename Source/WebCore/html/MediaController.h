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

class MediaController final : public RefCounted<MediaController>, public MediaControllerInterface, public EventTargetWithInlineData {
public:
    static PassRefPtr<MediaController> create(ScriptExecutionContext&);
    virtual ~MediaController();

    void addMediaElement(HTMLMediaElement*);
    void removeMediaElement(HTMLMediaElement*);
    bool containsMediaElement(HTMLMediaElement*) const;

    const String& mediaGroup() const { return m_mediaGroup; }
    
    virtual PassRefPtr<TimeRanges> buffered() const override;
    virtual PassRefPtr<TimeRanges> seekable() const override;
    virtual PassRefPtr<TimeRanges> played() override;
    
    virtual double duration() const override;
    virtual double currentTime() const override;
    virtual void setCurrentTime(double) override;
    
    virtual bool paused() const override { return m_paused; }
    virtual void play() override;
    virtual void pause() override;
    void unpause();
    
    virtual double defaultPlaybackRate() const override { return m_defaultPlaybackRate; }
    virtual void setDefaultPlaybackRate(double) override;
    
    virtual double playbackRate() const override;
    virtual void setPlaybackRate(double) override;
    
    virtual double volume() const override { return m_volume; }
    virtual void setVolume(double, ExceptionCode&) override;
    
    virtual bool muted() const override { return m_muted; }
    virtual void setMuted(bool) override;
    
    virtual ReadyState readyState() const override { return m_readyState; }

    enum PlaybackState { WAITING, PLAYING, ENDED };
    const AtomicString& playbackState() const;

    virtual bool supportsFullscreen() const override { return false; }
    virtual bool isFullscreen() const override { return false; }
    virtual void enterFullscreen() override { }

    virtual bool hasAudio() const override;
    virtual bool hasVideo() const override;
    virtual bool hasClosedCaptions() const override;
    virtual void setClosedCaptionsVisible(bool) override;
    virtual bool closedCaptionsVisible() const override { return m_closedCaptionsVisible; }
    
    virtual bool supportsScanning() const override;
    
    virtual void beginScrubbing() override;
    virtual void endScrubbing() override;
    virtual void beginScanning(ScanDirection) override;
    virtual void endScanning() override;
    
    virtual bool canPlay() const override;
    
    virtual bool isLiveStream() const override;
    
    virtual bool hasCurrentSrc() const override;
    
    virtual void returnToRealtime() override;

    bool isBlocked() const;

    // EventTarget
    using RefCounted<MediaController>::ref;
    using RefCounted<MediaController>::deref;

private:
    explicit MediaController(ScriptExecutionContext&);
    void reportControllerState();
    void updateReadyState();
    void updatePlaybackState();
    void updateMediaElements();
    void bringElementUpToSpeed(HTMLMediaElement*);
    void scheduleEvent(const AtomicString& eventName);
    void asyncEventTimerFired(Timer<MediaController>&);
    void clearPositionTimerFired(Timer<MediaController>&);
    bool hasEnded() const;
    void scheduleTimeupdateEvent();
    void timeupdateTimerFired(Timer<MediaController>&);
    void startTimeupdateTimer();

    // EventTarget
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }
    virtual EventTargetInterface eventTargetInterface() const override { return MediaControllerEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return &m_scriptExecutionContext; };

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
    Vector<RefPtr<Event>> m_pendingEvents;
    Timer<MediaController> m_asyncEventTimer;
    mutable Timer<MediaController> m_clearPositionTimer;
    String m_mediaGroup;
    bool m_closedCaptionsVisible;
    std::unique_ptr<Clock> m_clock;
    ScriptExecutionContext& m_scriptExecutionContext;
    Timer<MediaController> m_timeupdateTimer;
    double m_previousTimeupdateTime;
};

} // namespace WebCore

#endif
#endif
