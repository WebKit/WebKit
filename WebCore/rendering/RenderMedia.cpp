/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include "FloatConversion.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

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
    , m_previousVisible(VISIBLE)
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
}

void RenderMedia::destroy()
{
    if (m_controlsShadowRoot && m_controlsShadowRoot->renderer()) {

        // detach the panel before removing the shadow renderer to prevent a crash in m_controlsShadowRoot->detach() 
        //  when display: style changes
        m_panel->detach();

        removeChild(m_controlsShadowRoot->renderer());
        m_controlsShadowRoot->detach();
        m_controlsShadowRoot = 0;
    }
    RenderReplaced::destroy();
}

HTMLMediaElement* RenderMedia::mediaElement() const
{ 
    return static_cast<HTMLMediaElement*>(node()); 
}

MediaPlayer* RenderMedia::player() const
{
    return mediaElement()->player();
}

void RenderMedia::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);

    if (m_controlsShadowRoot) {
        if (m_panel->renderer())
            m_panel->renderer()->setStyle(getCachedPseudoStyle(MEDIA_CONTROLS_PANEL));

        if (m_timelineContainer->renderer())
            m_timelineContainer->renderer()->setStyle(getCachedPseudoStyle(MEDIA_CONTROLS_TIMELINE_CONTAINER));
        
        m_muteButton->updateStyle();
        m_playButton->updateStyle();
        m_seekBackButton->updateStyle();
        m_seekForwardButton->updateStyle();
        m_timeline->updateStyle();
        m_fullscreenButton->updateStyle();
        m_currentTimeDisplay->updateStyle();
        m_timeRemainingDisplay->updateStyle();
    }
}

void RenderMedia::layout()
{
    IntSize oldSize = contentBoxRect().size();

    RenderReplaced::layout();

    RenderBox* controlsRenderer = m_controlsShadowRoot ? m_controlsShadowRoot->renderBox() : 0;
    if (!controlsRenderer)
        return;
    IntSize newSize = contentBoxRect().size();
    if (newSize != oldSize || controlsRenderer->needsLayout()) {
        controlsRenderer->setLocation(borderLeft() + paddingLeft(), borderTop() + paddingTop());
        controlsRenderer->style()->setHeight(Length(newSize.height(), Fixed));
        controlsRenderer->style()->setWidth(Length(newSize.width(), Fixed));
        controlsRenderer->setNeedsLayout(true, false);
        controlsRenderer->layout();
        setChildNeedsLayout(false);
    }
}

void RenderMedia::createControlsShadowRoot()
{
    ASSERT(!m_controlsShadowRoot);
    m_controlsShadowRoot = new MediaControlShadowRootElement(document(), mediaElement());
    addChild(m_controlsShadowRoot->renderer());
}

void RenderMedia::createPanel()
{
    ASSERT(!m_panel);
    RenderStyle* style = getCachedPseudoStyle(MEDIA_CONTROLS_PANEL);
    m_panel = new HTMLDivElement(HTMLNames::divTag, document());
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

void RenderMedia::createTimelineContainer()
{
    ASSERT(!m_timelineContainer);
    RenderStyle* style = getCachedPseudoStyle(MEDIA_CONTROLS_TIMELINE_CONTAINER);
    m_timelineContainer = new HTMLDivElement(HTMLNames::divTag, document());
    RenderObject* renderer = m_timelineContainer->createRenderer(renderArena(), style);
    if (renderer) {
        m_timelineContainer->setRenderer(renderer);
        renderer->setStyle(style);
        m_timelineContainer->setAttached();
        m_timelineContainer->setInDocument(true);
        m_panel->addChild(m_timelineContainer);
        m_panel->renderer()->addChild(renderer);
    }
}

void RenderMedia::createTimeline()
{
    ASSERT(!m_timeline);
    m_timeline = new MediaControlTimelineElement(document(), mediaElement());
    m_timeline->setAttribute(precisionAttr, "float");
    m_timeline->attachToParent(m_timelineContainer.get());
}
  
void RenderMedia::createCurrentTimeDisplay()
{
    ASSERT(!m_currentTimeDisplay);
    m_currentTimeDisplay = new MediaTimeDisplayElement(document(), mediaElement(), true);
    m_currentTimeDisplay->attachToParent(m_timelineContainer.get());
}

void RenderMedia::createTimeRemainingDisplay()
{
    ASSERT(!m_timeRemainingDisplay);
    m_timeRemainingDisplay = new MediaTimeDisplayElement(document(), mediaElement(), false);
    m_timeRemainingDisplay->attachToParent(m_timelineContainer.get());
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
    if (!media->controls() || !media->inActiveDocument()) {
        if (m_controlsShadowRoot) {
            m_controlsShadowRoot->detach();
            m_panel = 0;
            m_muteButton = 0;
            m_playButton = 0;
            m_timelineContainer = 0;
            m_timeline = 0;
            m_seekBackButton = 0;
            m_seekForwardButton = 0;
            m_currentTimeDisplay = 0;
            m_timeRemainingDisplay = 0;
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
        createTimelineContainer();
        createTimeline();
        createSeekBackButton();
        createSeekForwardButton();
        createCurrentTimeDisplay();
        createTimeRemainingDisplay();
        createFullscreenButton();
    }

    if (media->canPlay()) {
        if (m_timeUpdateTimer.isActive())
            m_timeUpdateTimer.stop();
    } else if (style()->visibility() == VISIBLE && m_timeline && m_timeline->renderer() && m_timeline->renderer()->style()->display() != NONE ) {
        m_timeUpdateTimer.startRepeating(cTimeUpdateRepeatDelay);
    }

    m_previousVisible = style()->visibility();
    
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
    int seconds = (int)fabsf(time); 
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    if (hours) {
        if (hours > 9)
            return String::format("%s%02d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
        else
            return String::format("%s%01d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
    }
    else
        return String::format("%s%02d:%02d", (time < 0 ? "-" : ""), minutes, seconds);
}

void RenderMedia::updateTimeDisplay()
{
    if (!m_currentTimeDisplay || !m_currentTimeDisplay->renderer() || m_currentTimeDisplay->renderer()->style()->display() == NONE || style()->visibility() != VISIBLE)
        return;
    float now = mediaElement()->currentTime();
    float duration = mediaElement()->duration();

    String timeString = formatTime(now);
    ExceptionCode ec;
    m_currentTimeDisplay->setInnerText(timeString, ec);
    
    timeString = formatTime(now - duration);
    m_timeRemainingDisplay->setInnerText(timeString, ec);
}

void RenderMedia::updateControlVisibility() 
{
    if (!m_panel || !m_panel->renderer())
        return;

    // Don't fade for audio controls.
    HTMLMediaElement* media = mediaElement();
    if (!media->hasVideo())
        return;

    // do fading manually, css animations don't work well with shadow trees
    bool visible = style()->visibility() == VISIBLE && (m_mouseOver || media->canPlay());
    if (visible == (m_opacityAnimationTo > 0))
        return;

    if (style()->visibility() != m_previousVisible) {
        // don't fade gradually if it the element has just changed visibility
        m_previousVisible = style()->visibility();
        m_opacityAnimationTo = m_previousVisible == VISIBLE ? 1.0f : 0;
        changeOpacity(m_panel.get(), m_opacityAnimationTo);
        return;
    }

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
    RefPtr<RenderStyle> s = RenderStyle::clone(e->renderer()->style());
    s->setOpacity(opacity);
    // z-index can't be auto if opacity is used
    s->setZIndex(0);
    e->renderer()->setStyle(s.release());
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
        IntPoint point(mouseEvent->absoluteLocation());
        if (m_muteButton && m_muteButton->hitTest(point))
            m_muteButton->defaultEventHandler(event);

        if (m_playButton && m_playButton->hitTest(point))
            m_playButton->defaultEventHandler(event);

        if (m_seekBackButton && m_seekBackButton->hitTest(point))
            m_seekBackButton->defaultEventHandler(event);

        if (m_seekForwardButton && m_seekForwardButton->hitTest(point))
            m_seekForwardButton->defaultEventHandler(event);

        if (m_timeline && m_timeline->hitTest(point))
            m_timeline->defaultEventHandler(event);

        if (m_fullscreenButton && m_fullscreenButton->hitTest(point))
            m_fullscreenButton->defaultEventHandler(event);
        
        if (event->type() == eventNames().mouseoverEvent) {
            m_mouseOver = true;
            updateControlVisibility();
        }
        if (event->type() == eventNames().mouseoutEvent) {
            // When the scrollbar thumb captures mouse events, we should treat the mouse as still being over our renderer if the new target is a descendant
            Node* mouseOverNode = mouseEvent->relatedTarget() ? mouseEvent->relatedTarget()->toNode() : 0;
            RenderObject* mouseOverRenderer = mouseOverNode ? mouseOverNode->renderer() : 0;
            m_mouseOver = mouseOverRenderer && mouseOverRenderer->isDescendantOf(this);
            updateControlVisibility();
        }
    }
}

int RenderMedia::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderReplaced::lowestPosition(includeOverflowInterior, includeSelf);
    if (!m_controlsShadowRoot || !m_controlsShadowRoot->renderer())
        return bottom;
    
    return max(bottom,  m_controlsShadowRoot->renderBox()->y() + m_controlsShadowRoot->renderBox()->lowestPosition(includeOverflowInterior, includeSelf));
}

int RenderMedia::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int right = RenderReplaced::rightmostPosition(includeOverflowInterior, includeSelf);
    if (!m_controlsShadowRoot || !m_controlsShadowRoot->renderer())
        return right;
    
    return max(right, m_controlsShadowRoot->renderBox()->x() + m_controlsShadowRoot->renderBox()->rightmostPosition(includeOverflowInterior, includeSelf));
}

int RenderMedia::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int left = RenderReplaced::leftmostPosition(includeOverflowInterior, includeSelf);
    if (!m_controlsShadowRoot || !m_controlsShadowRoot->renderer())
        return left;
    
    return min(left, m_controlsShadowRoot->renderBox()->x() +  m_controlsShadowRoot->renderBox()->leftmostPosition(includeOverflowInterior, includeSelf));
}

} // namespace WebCore

#endif
