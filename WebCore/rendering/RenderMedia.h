/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef RenderMedia_h
#define RenderMedia_h

#if ENABLE(VIDEO)

#include "RenderReplaced.h"
#include "Timer.h"

namespace WebCore {
    
class HTMLInputElement;
class HTMLMediaElement;
class MediaControlPlayButtonElement;
class MediaControlTimelineElement;
class MediaPlayer;

class RenderMedia : public RenderReplaced {
public:
    RenderMedia(HTMLMediaElement*);
    RenderMedia(HTMLMediaElement*, const IntSize& intrinsicSize);
    virtual ~RenderMedia();
    
    virtual RenderObject* firstChild() const;
    virtual RenderObject* lastChild() const;
    virtual void removeChild(RenderObject*);
    
    virtual void layout();

    virtual const char* renderName() const { return "RenderMedia"; }
    virtual bool isMedia() const { return true; }
    
    HTMLMediaElement* mediaElement() const;
    MediaPlayer* player() const;

    static String formatTime(float time);

    void updateFromElement();
    void updatePlayer();
    void updateControls();
    
    void forwardEvent(Event*);
    
private:
    void createControlsShadowRoot();
    void createPanel();
    void createPlayButton();
    void createTimeline();
    void createTimeDisplay();
    
    void timeUpdateTimerFired(Timer<RenderMedia>*);
    void updateTimeDisplay();
    
    void updateControlVisibility();
    void changeOpacity(HTMLElement*, float opacity);
    void opacityAnimationTimerFired(Timer<RenderMedia>*);

    RefPtr<HTMLElement> m_controlsShadowRoot;
    RefPtr<HTMLElement> m_panel;
    RefPtr<MediaControlPlayButtonElement> m_playButton;
    RefPtr<MediaControlTimelineElement> m_timeline;
    RefPtr<HTMLElement> m_timeDisplay;
    
    Timer<RenderMedia> m_timeUpdateTimer;
    Timer<RenderMedia> m_opacityAnimationTimer;
    bool m_mouseOver;
    double m_opacityAnimationStartTime;
    float m_opacityAnimationFrom;
    float m_opacityAnimationTo;
};

} // namespace WebCore

#endif
#endif // RenderMedia_h
