/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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
#include "Event.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "MediaPlayer.h"
#include "RenderSlider.h"
#include "SystemTime.h"
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

using namespace EventNames;

static const double cTimeUpdateRepeatDelay = 0.2;
static const double cOpacityAnimationRepeatDelay = 0.05;
// FIXME get this from style
static const double cOpacityAnimationDuration = 0.1;

RenderMedia::RenderMedia(HTMLMediaElement* video)
    : RenderReplaced(video)
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
        controlsRenderer->setPos(borderLeft() + paddingLeft(), borderTop() + paddingTop());
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
    if (renderer) {
        m_panel->setRenderer(renderer);
        renderer->setStyle(style);
        m_panel->setAttached();
        m_panel->setInDocument(true);
        m_controlsShadowRoot->addChild(m_panel);
        m_controlsShadowRoot->renderer()->addChild(renderer);
    }
}

void RenderMedia::createMuteButton()
{
    ASSERT(!m_muteButton);
    m_muteButton = new MediaControlMuteButtonElement(document(), mediaElement());
    m_muteButton->attachToParent(m_panel.get());
}

void RenderMedia::createPlayButton()
{
    ASSERT(!m_playButton);
    m_playButton = new MediaControlPlayButtonElement(document(), mediaElement());
    m_playButton->attachToParent(m_panel.get());
}

void RenderMedia::createSeekBackButton()
{
    ASSERT(!m_seekBackButton);
    m_seekBackButton = new MediaControlSeekButtonElement(document(), mediaElement(), false);
    m_seekBackButton->attachToParent(m_panel.get());
}

void RenderMedia::createSeekForwardButton()
{
    ASSERT(!m_seekForwardButton);
    m_seekForwardButton = new MediaControlSeekButtonElement(document(), mediaElement(), true);
    m_seekForwardButton->attachToParent(m_panel.get());
}

void RenderMedia::createTimeline()
{
    ASSERT(!m_timeline);
    m_timeline = new MediaControlTimelineElement(document(), mediaElement());
    m_timeline->attachToParent(m_panel.get());
}
  
void RenderMedia::createTimeDisplay()
{
    ASSERT(!m_timeDisplay);
    RenderStyle* style = getPseudoStyle(RenderStyle::MEDIA_CONTROLS_TIME_DISPLAY);
    m_timeDisplay = new HTMLDivElement(document());
    RenderObject* renderer = m_timeDisplay->createRenderer(renderArena(), style);
    if (renderer) {
        m_timeDisplay->setRenderer(renderer);
        renderer->setStyle(style);
        m_timeDisplay->setAttached();
        m_timeDisplay->setInDocument(true);
        m_panel->addChild(m_timeDisplay);
        m_panel->renderer()->addChild(renderer);
    }
}

void RenderMedia::createFullscreenButton()
{
    ASSERT(!m_fullscreenButton);
    m_fullscreenButton = new MediaControlFullscreenButtonElement(document(), mediaElement());
    m_fullscreenButton->attachToParent(m_panel.get());
}
    
void RenderMedia::updateFromElement()
{
    updateControls();
}
            
void RenderMedia::updateControls()
{
    HTMLMediaElement* media = mediaElement();
    if (!media->controls() || media->inPageCache()) {
        if (m_controlsShadowRoot) {
            m_controlsShadowRoot->detach();
            m_panel = 0;
            m_muteButton = 0;
            m_playButton = 0;
            m_timeline = 0;
            m_seekBackButton = 0;
            m_seekForwardButton = 0;
            m_timeDisplay = 0;
            m_fullscreenButton = 0;
            m_controlsShadowRoot = 0;
        }
        m_opacityAnimationTo = 1.0f;
        m_opacityAnimationTimer.stop();
        m_timeUpdateTimer.stop();
        return;
    }
    
    if (!m_controlsShadowRoot) {
        createControlsShadowRoot();
        createPanel();
        createMuteButton();
        createPlayButton();
        createTimeline();
        createSeekBackButton();
        createSeekForwardButton();
        createTimeDisplay();
        createFullscreenButton();
    }
    
    if (media->paused() || media->ended() || media->networkState() < HTMLMediaElement::LOADED_METADATA)
        m_timeUpdateTimer.stop();
    else
        m_timeUpdateTimer.startRepeating(cTimeUpdateRepeatDelay);
    
    if (m_muteButton)
        m_muteButton->update();
    if (m_playButton)
        m_playButton->update();
    if (m_timeline)
        m_timeline->update();
    if (m_seekBackButton)
        m_seekBackButton->update();
    if (m_seekForwardButton)
        m_seekForwardButton->update();
    if (m_fullscreenButton)
        m_fullscreenButton->update();
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
    if (!m_panel || !m_panel->renderer())
        return;
    // do fading manually, css animations don't work well with shadow trees
    HTMLMediaElement* media = mediaElement();
    // Don't fade for audio controls.
    if (!media->isVideo())
        return;
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
        if (m_muteButton && m_muteButton->renderer() && m_muteButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_muteButton->defaultEventHandler(event);
        if (m_playButton && m_playButton->renderer() && m_playButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_playButton->defaultEventHandler(event);
        if (m_seekBackButton && m_seekBackButton->renderer() && m_seekBackButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_seekBackButton->defaultEventHandler(event);
        if (m_seekForwardButton && m_seekForwardButton->renderer() && m_seekForwardButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_seekForwardButton->defaultEventHandler(event);
        if (m_timeline && m_timeline->renderer() && m_timeline->renderer()->absoluteBoundingBoxRect().contains(point))
            m_timeline->defaultEventHandler(event);
        if (m_fullscreenButton && m_fullscreenButton->renderer() && m_fullscreenButton->renderer()->absoluteBoundingBoxRect().contains(point))
            m_fullscreenButton->defaultEventHandler(event);
        
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
