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

#include "config.h"

#if ENABLE(VIDEO)
#include "RenderMedia.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "MediaPlayer.h"
#include "RenderSlider.h"
#include "SystemTime.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;
    
static const double cTimeUpdateRepeatDelay = 0.2;
static const double cOpacityAnimationRepeatDelay = 0.05;
// FIXME get this from style
static const double cOpacityAnimationDuration = 0.5;

class RenderMediaControlShadowRoot : public RenderBlock {
public:
    RenderMediaControlShadowRoot(Element* e) : RenderBlock(e) { }
    void setParent(RenderObject* p) { RenderObject::setParent(p); }
};

class MediaControlShadowRootElement : public HTMLDivElement {
public:
    MediaControlShadowRootElement(Document* doc, HTMLMediaElement* mediaElement);
    
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_mediaElement; }
    
private:
    HTMLMediaElement* m_mediaElement;    
};
    
MediaControlShadowRootElement::MediaControlShadowRootElement(Document* doc, HTMLMediaElement* mediaElement) 
    : HTMLDivElement(doc)
    , m_mediaElement(mediaElement) 
{
    RenderStyle* rootStyle = new (mediaElement->renderer()->renderArena()) RenderStyle();
    rootStyle->setDisplay(BLOCK);
    rootStyle->setPosition(AbsolutePosition);
    RenderMediaControlShadowRoot* renderer = new (mediaElement->renderer()->renderArena()) RenderMediaControlShadowRoot(this);
    renderer->setParent(mediaElement->renderer());
    renderer->setStyle(rootStyle);
    setRenderer(renderer);
    setAttached();
    setInDocument(true);
}
    
// ----------------------------

class MediaControlInputElement : public HTMLInputElement {
public:
    MediaControlInputElement(Document*, RenderStyle::PseudoId, String type, HTMLMediaElement*);
    void attachToParent(PassRefPtr<Element>);
protected:
    HTMLMediaElement* m_mediaElement;   
};
    
MediaControlInputElement::MediaControlInputElement(Document* doc, RenderStyle::PseudoId pseudo, String type, HTMLMediaElement* mediaElement) 
: HTMLInputElement(doc)
, m_mediaElement(mediaElement)
{
    setInputType(type);
    RenderStyle* style = m_mediaElement->renderer()->getPseudoStyle(pseudo);
    RenderObject* renderer = createRenderer(m_mediaElement->renderer()->renderArena(), style);
    setRenderer(renderer);
    renderer->setStyle(style);
    renderer->updateFromElement();
    setAttached();
    setInDocument(true);
}

void MediaControlInputElement::attachToParent(PassRefPtr<Element> parent)
{
    parent->addChild(this);
    parent->renderer()->addChild(renderer());
}
    
// ----------------------------

class MediaControlPlayButtonElement : public MediaControlInputElement {
public:
    MediaControlPlayButtonElement(Document* doc, HTMLMediaElement* element)
        : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_PLAY_BUTTON, "button", element) { }
    bool inPausedState() const;
    virtual void defaultEventHandler(Event*);
    void update();
};

bool MediaControlPlayButtonElement::inPausedState() const
{
    return m_mediaElement->paused() || m_mediaElement->ended() || m_mediaElement->networkState() < HTMLMediaElement::LOADED_METADATA;
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == clickEvent) {
        ExceptionCode ec;
        if (inPausedState())
            m_mediaElement->play(ec);
        else 
            m_mediaElement->pause(ec);
        event->defaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::update()
{
    // FIXME: these are here just for temporary look
    static const UChar blackRightPointingTriangle = 0x25b6;
    const UChar twoBlackVerticalRectangles[] = { 0x25AE, 0x25AE };
    setValue(inPausedState() ? String(&blackRightPointingTriangle, 1) : String(&twoBlackVerticalRectangles[0], 2));
    renderer()->updateFromElement();
}

// ----------------------------

class MediaControlTimelineElement : public MediaControlInputElement {
public:
    MediaControlTimelineElement(Document* doc, HTMLMediaElement* element)
        : MediaControlInputElement(doc, RenderStyle::MEDIA_CONTROLS_TIMELINE, "range", element) { 
            setAttribute(precisionAttr, "float");
        }
    virtual void defaultEventHandler(Event*);
    void update(bool updateDuration = true);
};

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    float oldTime = (float)value().toDouble();
    HTMLInputElement::defaultEventHandler(event);
    float time = (float)value().toDouble();
    if (oldTime != time) {
        ExceptionCode ec;
        m_mediaElement->setCurrentTime(time, ec);
    }
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

RenderMedia::RenderMedia(HTMLMediaElement* video)
    : RenderReplaced(video)
    , m_controlsShadowRoot(0)
    , m_panel(0)
    , m_playButton(0)
    , m_timeline(0)
    , m_timeDisplay(0)
    , m_timeUpdateTimer(this, &RenderMedia::timeUpdateTimerFired)
    , m_opacityAnimationTimer(this, &RenderMedia::opacityAnimationTimerFired)
    , m_mouseOver(false)
    , m_opacityAnimationStartTime(0)
    , m_opacityAnimationFrom(0)
    , m_opacityAnimationTo(1.0f)
{
}

RenderMedia::RenderMedia(HTMLMediaElement* video, const IntSize& intrinsicSize)
    : RenderReplaced(video, intrinsicSize)
    , m_controlsShadowRoot(0)
    , m_panel(0)
    , m_playButton(0)
    , m_timeline(0)
    , m_timeDisplay(0)
    , m_timeUpdateTimer(this, &RenderMedia::timeUpdateTimerFired)
    , m_opacityAnimationTimer(this, &RenderMedia::opacityAnimationTimerFired)
    , m_mouseOver(false)
    , m_opacityAnimationStartTime(0)
    , m_opacityAnimationFrom(0)
    , m_opacityAnimationTo(1.0f)
{
}

RenderMedia::~RenderMedia()
{
    if (m_controlsShadowRoot && m_controlsShadowRoot->renderer()) {
        static_cast<RenderMediaControlShadowRoot*>(m_controlsShadowRoot->renderer())->setParent(0);
        m_controlsShadowRoot->detach();
    }
}
 
HTMLMediaElement* RenderMedia::mediaElement() const
{ 
    return static_cast<HTMLMediaElement*>(node()); 
}

MediaPlayer* RenderMedia::player() const
{
    return mediaElement()->player();
}

void RenderMedia::layout()
{
    IntSize oldSize = contentBox().size();

    RenderReplaced::layout();

    RenderObject* controlsRenderer = m_controlsShadowRoot ? m_controlsShadowRoot->renderer() : 0;
    if (!controlsRenderer)
        return;
    IntSize newSize = contentBox().size();
    if (newSize != oldSize || controlsRenderer->needsLayout()) {
        controlsRenderer->style()->setHeight(Length(newSize.height(), Fixed));
        controlsRenderer->style()->setWidth(Length(newSize.width(), Fixed));
        controlsRenderer->setNeedsLayout(true, false);
        controlsRenderer->layout();
        setChildNeedsLayout(false);
    }
}

RenderObject* RenderMedia::firstChild() const 
{ 
    return m_controlsShadowRoot ? m_controlsShadowRoot->renderer() : 0; 
}

RenderObject* RenderMedia::lastChild() const 
{ 
    return m_controlsShadowRoot ? m_controlsShadowRoot->renderer() : 0;
}
    
void RenderMedia::removeChild(RenderObject* child)
{
    ASSERT(m_controlsShadowRoot);
    ASSERT(child == m_controlsShadowRoot->renderer());
    child->removeLayers(enclosingLayer());
}
    
void RenderMedia::createControlsShadowRoot()
{
    ASSERT(!m_controlsShadowRoot);
    m_controlsShadowRoot = new MediaControlShadowRootElement(document(), mediaElement());
}

void RenderMedia::createPanel()
{
    ASSERT(!m_panel);
    RenderStyle* style = getPseudoStyle(RenderStyle::MEDIA_CONTROLS_PANEL);
    m_panel = new HTMLDivElement(document());
    RenderObject* renderer = m_panel->createRenderer(renderArena(), style);
    m_panel->setRenderer(renderer);
    renderer->setStyle(style);
    m_panel->setAttached();
    m_panel->setInDocument(true);
    m_controlsShadowRoot->addChild(m_panel);
    m_controlsShadowRoot->renderer()->addChild(renderer);
}

void RenderMedia::createPlayButton()
{
    ASSERT(!m_playButton);
    m_playButton = new MediaControlPlayButtonElement(document(), mediaElement());
    m_playButton->attachToParent(m_panel);
}

void RenderMedia::createTimeline()
{
    ASSERT(!m_timeline);
    m_timeline = new MediaControlTimelineElement(document(), mediaElement());
    m_timeline->attachToParent(m_panel);
}
  
void RenderMedia::createTimeDisplay()
{
    ASSERT(!m_timeDisplay);
    RenderStyle* style = getPseudoStyle(RenderStyle::MEDIA_CONTROLS_TIME_DISPLAY);
    m_timeDisplay = new HTMLDivElement(document());
    RenderObject* renderer = m_timeDisplay->createRenderer(renderArena(), style);
    m_timeDisplay->setRenderer(renderer);
    renderer->setStyle(style);
    m_timeDisplay->setAttached();
    m_timeDisplay->setInDocument(true);
    m_panel->addChild(m_timeDisplay);
    m_panel->renderer()->addChild(renderer);
}
    
void RenderMedia::updateFromElement()
{
    updateControls();
}
            
void RenderMedia::updateControls()
{
    HTMLMediaElement* media = mediaElement();
    if (!media->controls()) {
        if (m_controlsShadowRoot) {
            m_controlsShadowRoot->detach();
            m_panel = 0;
            m_playButton = 0;
            m_timeline = 0;
            m_timeDisplay = 0;
            m_controlsShadowRoot = 0;
        }
        return;
    }
    
    if (!m_controlsShadowRoot) {
        createControlsShadowRoot();
        createPanel();
        createPlayButton();
        createTimeline();
        createTimeDisplay();
    }
    
    if (media->paused() || media->ended() || media->networkState() < HTMLMediaElement::LOADED_METADATA)
        m_timeUpdateTimer.stop();
    else
        m_timeUpdateTimer.startRepeating(cTimeUpdateRepeatDelay);
    
    if (m_playButton)
        m_playButton->update();
    if (m_timeline)
        m_timeline->update();
    updateTimeDisplay();
    updateControlVisibility();
}

void RenderMedia::timeUpdateTimerFired(Timer<RenderMedia>*)
{
    if (m_timeline)
        m_timeline->update(false);
    updateTimeDisplay();
}
    
String RenderMedia::formatTime(float time)
{
    if (!isfinite(time))
        time = 0;
    int seconds = (int)time; 
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    return String::format("%02d:%02d:%02d", hours, minutes, seconds);
}

void RenderMedia::updateTimeDisplay()
{
    if (!m_timeDisplay)
        return;
    String timeString = formatTime(mediaElement()->currentTime());
    ExceptionCode ec;
    m_timeDisplay->setInnerText(timeString, ec);
}
    
void RenderMedia::updateControlVisibility() 
{
    if (!m_panel)
        return;
    // do fading manually, css animations don't work well with shadow trees
    HTMLMediaElement* media = mediaElement();
    bool visible = m_mouseOver || media->paused() || media->ended() || media->networkState() < HTMLMediaElement::LOADED_METADATA;
    if (visible == (m_opacityAnimationTo > 0))
        return;
    if (visible) {
        m_opacityAnimationFrom = m_panel->renderer()->style()->opacity();
        m_opacityAnimationTo = 1.0f;
    } else {
        m_opacityAnimationFrom = m_panel->renderer()->style()->opacity();
        m_opacityAnimationTo = 0;
    }
    m_opacityAnimationStartTime = currentTime();
    m_opacityAnimationTimer.startRepeating(cOpacityAnimationRepeatDelay);
}
    
void RenderMedia::changeOpacity(HTMLElement* e, float opacity) 
{
    if (!e || !e->renderer() || !e->renderer()->style())
        return;
    RenderStyle* s = new (renderArena()) RenderStyle(*e->renderer()->style());
    s->setOpacity(opacity);
    // z-index can't be auto if opacity is used
    s->setZIndex(0);
    e->renderer()->setStyle(s);
}
    
void RenderMedia::opacityAnimationTimerFired(Timer<RenderMedia>*)
{
    double time = currentTime() - m_opacityAnimationStartTime;
    if (time >= cOpacityAnimationDuration) {
        time = cOpacityAnimationDuration;
        m_opacityAnimationTimer.stop();
    }
    float opacity = narrowPrecisionToFloat(m_opacityAnimationFrom + (m_opacityAnimationTo - m_opacityAnimationFrom) * time / cOpacityAnimationDuration);
    changeOpacity(m_panel.get(), opacity);
}
    
void RenderMedia::forwardEvent(Event* event)
{
    if (event->isMouseEvent() && m_controlsShadowRoot) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        IntPoint point(mouseEvent->pageX(), mouseEvent->pageY());
        if (m_playButton && m_playButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_playButton->defaultEventHandler(event);
        if (m_timeline && m_timeline->renderer()->absoluteBoundingBoxRect().contains(point))
            m_timeline->defaultEventHandler(event);
        
        if (event->type() == mouseoverEvent) {
            m_mouseOver = true;
            updateControlVisibility();
        }
        if (event->type() == mouseoutEvent) {
            // FIXME: moving over scrollbar thumb generates mouseout for the ancestor media element for some reason
            m_mouseOver = absoluteBoundingBoxRect().contains(point);
            updateControlVisibility();
        }
    }
}

} // namespace WebCore

#endif
