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

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaControlElements.h"

#include "Event.h"
#include "EventNames.h"
#include "EventHandler.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "RenderSlider.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// FIXME: These constants may need to be tweaked to better match the seeking in the QT plugin
static const float cSeekRepeatDelay = 0.1f;
static const float cStepTime = 0.07f;
static const float cSeekTime = 0.2f;

MediaControlShadowRootElement::MediaControlShadowRootElement(Document* doc, HTMLMediaElement* mediaElement) 
    : HTMLDivElement(doc)
    , m_mediaElement(mediaElement) 
{
    RenderStyle* rootStyle = new (mediaElement->renderer()->renderArena()) RenderStyle();
    rootStyle->inheritFrom(mediaElement->renderer()->style());
    rootStyle->setDisplay(BLOCK);
    rootStyle->setPosition(RelativePosition);
    RenderMediaControlShadowRoot* renderer = new (mediaElement->renderer()->renderArena()) RenderMediaControlShadowRoot(this);
    renderer->setParent(mediaElement->renderer());
    renderer->setStyle(rootStyle);
    setRenderer(renderer);
    setAttached();
    setInDocument(true);
}

// ----------------------------

MediaControlInputElement::MediaControlInputElement(Document* doc, RenderStyle::PseudoId pseudo, String type, HTMLMediaElement* mediaElement) 
    : HTMLInputElement(doc)
    , m_mediaElement(mediaElement)
{
    setInputType(type);
    RenderStyle* style = m_mediaElement->renderer()->getPseudoStyle(pseudo);
    RenderObject* renderer = createRenderer(m_mediaElement->renderer()->renderArena(), style);
    if (renderer) {
        setRenderer(renderer);
        renderer->setStyle(style);
    }
    setAttached();
    setInDocument(true);
}

void MediaControlInputElement::attachToParent(Element* parent)
{
    parent->addChild(this);
    parent->renderer()->addChild(renderer());
}

void MediaControlInputElement::update()
{
    if (renderer())
        renderer()->updateFromElement();
}

// ----------------------------

MediaControlMuteButtonElement::MediaControlMuteButtonElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_MUTE_BUTTON, "button", element)
{
}

void MediaControlMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == clickEvent) {
        m_mediaElement->setMuted(!m_mediaElement->muted());
        event->defaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlPlayButtonElement::MediaControlPlayButtonElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_PLAY_BUTTON, "button", element)
{
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == clickEvent) {
        ExceptionCode ec;
        if (m_mediaElement->canPlay())
            m_mediaElement->play(ec);
        else 
            m_mediaElement->pause(ec);
        event->defaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlSeekButtonElement::MediaControlSeekButtonElement(Document* doc, HTMLMediaElement* element, bool forward)
    : MediaControlInputElement(doc, forward ? RenderStyle::MEDIA_CONTROLS_SEEK_FORWARD_BUTTON : RenderStyle::MEDIA_CONTROLS_SEEK_BACK_BUTTON, "button", element)
    , m_forward(forward)
    , m_seeking(false)
    , m_capturing(false)
    , m_seekTimer(this, &MediaControlSeekButtonElement::seekTimerFired)
{
}

void MediaControlSeekButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == mousedownEvent) {
        if (Frame* frame = document()->frame()) {
            m_capturing = true;
            frame->eventHandler()->setCapturingMouseEventsNode(this);
        }
        ExceptionCode ec;
        m_mediaElement->pause(ec);
        m_seekTimer.startRepeating(cSeekRepeatDelay);
        event->defaultHandled();
    } else if (event->type() == mouseupEvent) {
        if (m_capturing)
            if (Frame* frame = document()->frame()) {
                m_capturing = false;
                frame->eventHandler()->setCapturingMouseEventsNode(0);
            }
        ExceptionCode ec;
        if (m_seeking || m_seekTimer.isActive()) {
            if (!m_seeking) {
                float stepTime = m_forward ? cStepTime : -cStepTime;
                m_mediaElement->setCurrentTime(m_mediaElement->currentTime() + stepTime, ec);
            }
            m_seekTimer.stop();
            m_seeking = false;
            event->defaultHandled();
        }
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlSeekButtonElement::seekTimerFired(Timer<MediaControlSeekButtonElement>*)
{
    ExceptionCode ec;
    m_seeking = true;
    float seekTime = m_forward ? cSeekTime : -cSeekTime;
    m_mediaElement->setCurrentTime(m_mediaElement->currentTime() + seekTime, ec);
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_TIMELINE, "range", element)
{ 
    setAttribute(precisionAttr, "float");
}

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    RenderSlider* slider = static_cast<RenderSlider*>(renderer());
    bool oldInDragMode = slider && slider->inDragMode();
    float oldTime = narrowPrecisionToFloat(value().toDouble());
    bool oldEnded = m_mediaElement->ended();

    HTMLInputElement::defaultEventHandler(event);

    float time = narrowPrecisionToFloat(value().toDouble());
    if (oldTime != time || event->type() == inputEvent) {
        ExceptionCode ec;
        m_mediaElement->setCurrentTime(time, ec);
    }
    // Media element stays in non-paused state when it reaches end. If the slider is now dragged
    // to some other position the playback resumes which does not match usual media player UIs.
    // Get the expected behavior by pausing explicitly in this case.
    if (oldEnded && !m_mediaElement->ended() && !m_mediaElement->paused()) {
        ExceptionCode ec;
        m_mediaElement->pause(ec);
    }
    // Pause playback during drag, but do it without using DOM API which would generate events 
    bool inDragMode = slider && slider->inDragMode();
    if (inDragMode != oldInDragMode)
        m_mediaElement->setPausedInternal(inDragMode);
}

void MediaControlTimelineElement::update(bool updateDuration) 
{
    if (updateDuration) {
        float dur = m_mediaElement->duration();
        setAttribute(maxAttr, String::number(isfinite(dur) ? dur : 0));
    }
    setValue(String::number(m_mediaElement->currentTime()));
}

// ----------------------------

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_FULLSCREEN_BUTTON, "button", element)
{
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == clickEvent) {
        event->defaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

} //namespace WebCore
#endif // enable(video)
