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

class MediaControlShadowRootElement : public HTMLDivElement {
public:
    MediaControlShadowRootElement(Document*, HTMLMediaElement*);
    
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_mediaElement; }
    
private:
    HTMLMediaElement* m_mediaElement;    
};

// ----------------------------

class MediaControlInputElement : public HTMLInputElement {
public:
    MediaControlInputElement(Document*, RenderStyle::PseudoId, String type, HTMLMediaElement*);
    void attachToParent(Element*);
    void update();
protected:
    HTMLMediaElement* m_mediaElement;   
};

// ----------------------------

class MediaControlMuteButtonElement : public MediaControlInputElement {
public:
    MediaControlMuteButtonElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
};

// ----------------------------

class MediaControlPlayButtonElement : public MediaControlInputElement {
public:
    MediaControlPlayButtonElement(Document*, HTMLMediaElement*);
    virtual void defaultEventHandler(Event*);
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
