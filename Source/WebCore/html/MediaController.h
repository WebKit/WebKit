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
    static Ref<MediaController> create(ScriptExecutionContext&);
    virtual ~MediaController();

    void addMediaElement(HTMLMediaElement*);
    void removeMediaElement(HTMLMediaElement*);
    bool containsMediaElement(HTMLMediaElement*) const;

    const String& mediaGroup() const { return m_mediaGroup; }
    
    PassRefPtr<TimeRanges> buffered() const override;
    PassRefPtr<TimeRanges> seekable() const override;
    PassRefPtr<TimeRanges> played() override;
    
    double duration() const override;
    double currentTime() const override;
    void setCurrentTime(double) override;
    
    bool paused() const override { return m_paused; }
    void play() override;
    void pause() override;
    void unpause();
    
    double defaultPlaybackRate() const override { return m_defaultPlaybackRate; }
    void setDefaultPlaybackRate(double) override;
    
    double playbackRate() const override;
    void setPlaybackRate(double) override;
    
    double volume() const override { return m_volume; }
    void setVolume(double, ExceptionCode&) override;
    
    bool muted() const override { return m_muted; }
    void setMuted(bool) override;
    
    ReadyState readyState() const override { return m_readyState; }

    enum PlaybackState { WAITING, PLAYING, ENDED };
    const AtomicString& playbackState() const;

    bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const override { return false; }
    bool isFullscreen() const override { return false; }
    void enterFullscreen() override { }

    bool hasAudio() const override;
    bool hasVideo() const override;
    bool hasClosedCaptions() const override;
    void setClosedCaptionsVisible(bool) override;
    bool closedCaptionsVisible() const override { return m_closedCaptionsVisible; }
    
    bool supportsScanning() const override;
    
    void beginScrubbing() override;
    void endScrubbing() override;
    void beginScanning(ScanDirection) override;
    void endScanning() override;
    
    bool canPlay() const override;
    
    bool isLiveStream() const override;
    
    bool hasCurrentSrc() const override;
    
    void returnToRealtime() override;

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
    void asyncEventTimerFired();
    void clearPositionTimerFired();
    bool hasEnded() const;
    void scheduleTimeupdateEvent();
    void startTimeupdateTimer();

    // EventTarget
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }
    EventTargetInterface eventTargetInterface() const override { return MediaControllerEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return &m_scriptExecutionContext; };

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
    Vector<Ref<Event>> m_pendingEvents;
    Timer m_asyncEventTimer;
    mutable Timer m_clearPositionTimer;
    String m_mediaGroup;
    bool m_closedCaptionsVisible;
    std::unique_ptr<Clock> m_clock;
    ScriptExecutionContext& m_scriptExecutionContext;
    Timer m_timeupdateTimer;
    double m_previousTimeupdateTime;
    bool m_resetCurrentTimeInNextPlay { false };
};

} // namespace WebCore

#endif
#endif
