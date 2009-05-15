/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlElements_h
#define MediaControlElements_h

#if ENABLE(VIDEO)

#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "RenderBlock.h"

// These are the shadow elements used in RenderMedia

namespace WebCore {

class Event;
class Frame;

enum MediaControlElementType {
    MediaFullscreenButton, MediaMuteButton, MediaPlayButton,
    MediaSeekBackButton, MediaSeekForwardButton, MediaSlider, MediaSliderThumb,
    MediaUnMuteButton, MediaPauseButton, MediaTimelineContainer, MediaCurrentTimeDisplay, 
    MediaTimeRemainingDisplay, MediaControlsPanel
};

class MediaControlShadowRootElement : public HTMLDivElement {
public:
    MediaControlShadowRootElement(Document*, HTMLMediaElement*);
    
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_mediaElement; }

    void updateStyle();
    
private:
    HTMLMediaElement* m_mediaElement;    
};

 // ----------------------------
 
class MediaTextDisplayElement : public HTMLDivElement
{
public:
    MediaTextDisplayElement(Document*, PseudoId, HTMLMediaElement*);
    void attachToParent(Element*);
    void update();
    void updateStyle();
protected:
    HTMLMediaElement* m_mediaElement;   
    PseudoId m_pseudoStyleId;
};

// ----------------------------

class MediaTimeDisplayElement : public MediaTextDisplayElement {
public:
    MediaTimeDisplayElement(Document*, HTMLMediaElement*, bool currentTime);
};

// ----------------------------

class MediaControlInputElement : public HTMLInputElement {
public:
    MediaControlInputElement(Document*, PseudoId, const String& type, HTMLMediaElement*, MediaControlElementType);
    void attachToParent(Element*);
    void update();
    void updateStyle();
    bool hitTest(const IntPoint& absPoint);

protected:
    virtual void updateDisplayType() { }
    void setDisplayType(MediaControlElementType);

    HTMLMediaElement* m_mediaElement;   
    PseudoId m_pseudoStyleId;
    MediaControlElementType m_displayType;  // some elements can show multiple types (e.g. play/pause)
};

// ----------------------------

class MediaControlMuteButtonElement : public MediaControlInputElement {
public:
    MediaControlMuteButtonElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
    virtual void updateDisplayType();
};

// ----------------------------

class MediaControlPlayButtonElement : public MediaControlInputElement {
public:
    MediaControlPlayButtonElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
    virtual void updateDisplayType();
};

// ----------------------------

class MediaControlSeekButtonElement : public MediaControlInputElement {
public:
    MediaControlSeekButtonElement(Document*, HTMLMediaElement*, bool forward);
    virtual void defaultEventHandler(Event*);
    void seekTimerFired(Timer<MediaControlSeekButtonElement>*);

private:
    bool m_forward;
    bool m_seeking;
    bool m_capturing;
    Timer<MediaControlSeekButtonElement> m_seekTimer;
};

// ----------------------------

class MediaControlTimelineElement : public MediaControlInputElement {
public:
    MediaControlTimelineElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
    void update(bool updateDuration = true);
};

// ----------------------------

class MediaControlFullscreenButtonElement : public MediaControlInputElement {
public:
    MediaControlFullscreenButtonElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
};

// ----------------------------

class RenderMediaControlShadowRoot : public RenderBlock {
public:
    RenderMediaControlShadowRoot(Element* e) : RenderBlock(e) { }
    void setParent(RenderObject* p) { RenderObject::setParent(p); }
};

// ----------------------------

} //namespace WebCore
#endif // enable(video)
#endif // MediaControlElements_h
