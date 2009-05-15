/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "RenderMedia.h"
#include "RenderSlider.h"
#include "RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: These constants may need to be tweaked to better match the seeking in the QT plugin
static const float cSeekRepeatDelay = 0.1f;
static const float cStepTime = 0.07f;
static const float cSeekTime = 0.2f;

MediaControlShadowRootElement::MediaControlShadowRootElement(Document* doc, HTMLMediaElement* mediaElement) 
    : HTMLDivElement(divTag, doc)
    , m_mediaElement(mediaElement) 
{
    RefPtr<RenderStyle> rootStyle = RenderStyle::create();
    rootStyle->inheritFrom(mediaElement->renderer()->style());
    rootStyle->setDisplay(BLOCK);
    rootStyle->setPosition(RelativePosition);
    RenderMediaControlShadowRoot* renderer = new (mediaElement->renderer()->renderArena()) RenderMediaControlShadowRoot(this);
    renderer->setStyle(rootStyle.release());
    setRenderer(renderer);
    setAttached();
    setInDocument(true);
}

void MediaControlShadowRootElement::updateStyle()
{
    if (renderer()) {
        RenderStyle* timelineContainerStyle = m_mediaElement->renderer()->getCachedPseudoStyle(MEDIA_CONTROLS_TIMELINE_CONTAINER);
        renderer()->setStyle(timelineContainerStyle);
    }
}

// ----------------------------

MediaTextDisplayElement::MediaTextDisplayElement(Document* doc, PseudoId pseudo, HTMLMediaElement* mediaElement) 
    : HTMLDivElement(divTag, doc)
    , m_mediaElement(mediaElement)
    , m_pseudoStyleId(pseudo)
{
    RenderStyle* style = m_mediaElement->renderer()->getCachedPseudoStyle(m_pseudoStyleId);
    RenderObject* renderer = createRenderer(m_mediaElement->renderer()->renderArena(), style);
    if (renderer) {
        setRenderer(renderer);
        renderer->setStyle(style);
    }
    setAttached();
    setInDocument(true);
}

void MediaTextDisplayElement::attachToParent(Element* parent)
{
    parent->addChild(this);
    if (renderer())
        parent->renderer()->addChild(renderer());
}

void MediaTextDisplayElement::update()
{
    if (renderer())
        renderer()->updateFromElement();
}

void MediaTextDisplayElement::updateStyle()
{
    if (renderer() && m_mediaElement->renderer()) {
        RenderStyle* style = m_mediaElement->renderer()->getCachedPseudoStyle(m_pseudoStyleId);
        renderer()->setStyle(style);
    }
}

MediaTimeDisplayElement::MediaTimeDisplayElement(Document* doc, HTMLMediaElement* element, bool currentTime)
    : MediaTextDisplayElement(doc, currentTime ? MEDIA_CONTROLS_CURRENT_TIME_DISPLAY : MEDIA_CONTROLS_TIME_REMAINING_DISPLAY, element)
{
}

// ----------------------------

MediaControlInputElement::MediaControlInputElement(Document* doc, PseudoId pseudo, const String& type, HTMLMediaElement* mediaElement, MediaControlElementType displayType) 
    : HTMLInputElement(inputTag, doc)
    , m_mediaElement(mediaElement)
    , m_pseudoStyleId(pseudo)
    , m_displayType(displayType)
{
    setInputType(type);
    RenderStyle* style = m_mediaElement->renderer()->getCachedPseudoStyle(m_pseudoStyleId);
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
    updateDisplayType();
    if (renderer())
        renderer()->updateFromElement();
}

void MediaControlInputElement::updateStyle()
{
    if (renderer() && m_mediaElement->renderer()) {
        RenderStyle* style = m_mediaElement->renderer()->getCachedPseudoStyle(m_pseudoStyleId);
        renderer()->setStyle(style);
    }
}

bool MediaControlInputElement::hitTest(const IntPoint& absPoint)
{
    if (renderer() && renderer()->style()->hasAppearance())
        return theme()->hitTestMediaControlPart(renderer(), absPoint);

    return false;
}

void MediaControlInputElement::setDisplayType(MediaControlElementType displayType)
{
    if (displayType == m_displayType)
        return;

    m_displayType = displayType;
    if (RenderObject* o = renderer())
        o->repaint();
}

// ----------------------------

MediaControlMuteButtonElement::MediaControlMuteButtonElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, MEDIA_CONTROLS_MUTE_BUTTON, "button", element, element->muted() ? MediaUnMuteButton : MediaMuteButton)
{
}

void MediaControlMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        m_mediaElement->setMuted(!m_mediaElement->muted());
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlMuteButtonElement::updateDisplayType()
{
    setDisplayType(m_mediaElement->muted() ? MediaUnMuteButton : MediaMuteButton);
}

// ----------------------------

MediaControlPlayButtonElement::MediaControlPlayButtonElement(Document* doc, HTMLMediaElement* element)
    : MediaControlInputElement(doc, MEDIA_CONTROLS_PLAY_BUTTON, "button", element, element->canPlay() ? MediaPlayButton : MediaPauseButton)
{
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        m_mediaElement->togglePlayState();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::updateDisplayType()
{
    setDisplayType(m_mediaElement->canPlay() ? MediaPlayButton : MediaPauseButton);
}

// ----------------------------

MediaControlSeekButtonElement::MediaControlSeekButtonElement(Document* doc, HTMLMediaElement* element, bool forward)
    : MediaControlInputElement(doc, forward ? MEDIA_CONTROLS_SEEK_FORWARD_BUTTON : MEDIA_CONTROLS_SEEK_BACK_BUTTON,
                               "button", element, forward ? MediaSeekForwardButton : MediaSeekBackButton)
    , m_forward(forward)
    , m_seeking(false)
    , m_capturing(false)
    , m_seekTimer(this, &MediaControlSeekButtonElement::seekTimerFired)
{
}

void MediaControlSeekButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().mousedownEvent) {
        if (Frame* frame = document()->frame()) {
            m_capturing = true;
            frame->eventHandler()->setCapturingMouseEventsNode(this);
        }
        m_mediaElement->pause();
        m_seekTimer.startRepeating(cSeekRepeatDelay);
        event->setDefaultHandled();
    } else if (event->type() == eventNames().mouseupEvent) {
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
            event->setDefaultHandled();
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

MediaControlTimelineElement::MediaControlTimelineElement(Document* document, HTMLMediaElement* element)
    : MediaControlInputElement(document, MEDIA_CONTROLS_TIMELINE, "range", element, MediaTimelineContainer)
{ 
}

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().mousedownEvent)
        m_mediaElement->beginScrubbing();

    HTMLInputElement::defaultEventHandler(event);

     if (event->type() == eventNames().mouseoverEvent || event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mousemoveEvent ) {
        return;
    }

    float time = narrowPrecisionToFloat(value().toDouble());
    if (time != m_mediaElement->currentTime()) {
        ExceptionCode ec;
        m_mediaElement->setCurrentTime(time, ec);
    }

    RenderSlider* slider = static_cast<RenderSlider*>(renderer());
    if (slider && slider->inDragMode())
        static_cast<RenderMedia*>(m_mediaElement->renderer())->updateTimeDisplay();

    if (event->type() == eventNames().mouseupEvent)
        m_mediaElement->endScrubbing();
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
    : MediaControlInputElement(doc, MEDIA_CONTROLS_FULLSCREEN_BUTTON, "button", element, MediaFullscreenButton)
{
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

} //namespace WebCore
#endif // enable(video)
