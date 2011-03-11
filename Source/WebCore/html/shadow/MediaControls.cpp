/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "MediaControls.h"

#include "EventNames.h"
#include "FloatConversion.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>


using namespace std;

namespace WebCore {

using namespace HTMLNames;

static const double cTimeUpdateRepeatDelay = 0.2;
static const double cOpacityAnimationRepeatDelay = 0.05;

MediaControls::MediaControls(HTMLMediaElement* mediaElement)
    : m_mediaElement(mediaElement)
    , m_timeUpdateTimer(this, &MediaControls::timeUpdateTimerFired)
    , m_opacityAnimationTimer(this, &MediaControls::opacityAnimationTimerFired)
    , m_opacityAnimationStartTime(0)
    , m_opacityAnimationDuration(0)
    , m_opacityAnimationFrom(0)
    , m_opacityAnimationTo(1.0f)
    , m_mouseOver(false)
{
}

void MediaControls::reset()
{
    update();
}

void MediaControls::changedMute()
{
    update();
}

void MediaControls::changedVolume()
{
    update();
}

void MediaControls::changedClosedCaptionsVisibility()
{
    update();
}

void MediaControls::updateStyle()
{
    if (!m_controlsShadowRoot)
        return;

    if (m_panel)
        m_panel->updateStyle();
    if (m_muteButton)
        m_muteButton->updateStyle();
    if (m_playButton)
        m_playButton->updateStyle();
    if (m_seekBackButton)
        m_seekBackButton->updateStyle();
    if (m_seekForwardButton)
        m_seekForwardButton->updateStyle();
    if (m_rewindButton)
        m_rewindButton->updateStyle();
    if (m_returnToRealtimeButton)
        m_returnToRealtimeButton->updateStyle();
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->updateStyle();
    if (m_statusDisplay)
        m_statusDisplay->updateStyle();
    if (m_timelineContainer)
        m_timelineContainer->updateStyle();
    if (m_timeline)
        m_timeline->updateStyle();
    if (m_fullscreenButton)
        m_fullscreenButton->updateStyle();
    if (m_currentTimeDisplay)
        m_currentTimeDisplay->updateStyle();
    if (m_timeRemainingDisplay)
        m_timeRemainingDisplay->updateStyle();
    if (m_volumeSliderContainer)
        m_volumeSliderContainer->updateStyle();
    if (m_volumeSliderMuteButton)
        m_volumeSliderMuteButton->updateStyle();
    if (m_volumeSlider)
        m_volumeSlider->updateStyle();
}

void MediaControls::destroy()
{
    ASSERT(m_mediaElement->renderer());

    if (m_controlsShadowRoot && m_controlsShadowRoot->renderer()) {

        // detach the panel before removing the shadow renderer to prevent a crash in m_controlsShadowRoot->detach() 
        //  when display: style changes
        m_panel->detach();

        m_mediaElement->renderer()->removeChild(m_controlsShadowRoot->renderer());
        m_controlsShadowRoot->detach();
        m_controlsShadowRoot = 0;
    }
}

void MediaControls::update()
{
    HTMLMediaElement* media = m_mediaElement;
    if (!media->controls() || !media->inActiveDocument()) {
        if (m_controlsShadowRoot) {
            m_controlsShadowRoot->detach();
            m_panel = 0;
            m_muteButton = 0;
            m_playButton = 0;
            m_statusDisplay = 0;
            m_timelineContainer = 0;
            m_timeline = 0;
            m_seekBackButton = 0;
            m_seekForwardButton = 0;
            m_rewindButton = 0;
            m_returnToRealtimeButton = 0;
            m_currentTimeDisplay = 0;
            m_timeRemainingDisplay = 0;
            m_fullscreenButton = 0;
            m_volumeSliderContainer = 0;
            m_volumeSlider = 0;
            m_volumeSliderMuteButton = 0;
            m_controlsShadowRoot = 0;
            m_toggleClosedCaptionsButton = 0;
        }
        m_opacityAnimationTo = 1.0f;
        m_opacityAnimationTimer.stop();
        m_timeUpdateTimer.stop();
        return;
    }

    if (!m_controlsShadowRoot) {
        createControlsShadowRoot();
        createPanel();
        if (m_panel) {
            createRewindButton();
            createPlayButton();
            createReturnToRealtimeButton();
            createStatusDisplay();
            createTimelineContainer();
            if (m_timelineContainer) {
                createCurrentTimeDisplay();
                createTimeline();
                createTimeRemainingDisplay();
            }
            createSeekBackButton();
            createSeekForwardButton();
            createToggleClosedCaptionsButton();
            createFullscreenButton();
            createMuteButton();
            createVolumeSliderContainer();
            if (m_volumeSliderContainer) {
                createVolumeSlider();
                createVolumeSliderMuteButton();
            }
            m_panel->attach();
        }
    }

    if (media->canPlay()) {
        if (m_timeUpdateTimer.isActive())
            m_timeUpdateTimer.stop();
    } else if (media->renderer()->style()->visibility() == VISIBLE && m_timeline && m_timeline->renderer() && m_timeline->renderer()->style()->display() != NONE) {
        m_timeUpdateTimer.startRepeating(cTimeUpdateRepeatDelay);
    }

    if (m_panel) {
        // update() might alter the opacity of the element, especially if we are in the middle
        // of an animation. This is the only element concerned as we animate only this element.
        float opacityBeforeChangingStyle = m_panel->renderer() ? m_panel->renderer()->style()->opacity() : 0;
        m_panel->update();
        changeOpacity(m_panel.get(), opacityBeforeChangingStyle);
    }
    if (m_muteButton)
        m_muteButton->update();
    if (m_playButton)
        m_playButton->update();
    if (m_timelineContainer)
        m_timelineContainer->update();
    if (m_volumeSliderContainer)
        m_volumeSliderContainer->update();
    if (m_timeline)
        m_timeline->update();
    if (m_currentTimeDisplay)
        m_currentTimeDisplay->update();
    if (m_timeRemainingDisplay)
        m_timeRemainingDisplay->update();
    if (m_seekBackButton)
        m_seekBackButton->update();
    if (m_seekForwardButton)
        m_seekForwardButton->update();
    if (m_rewindButton)
        m_rewindButton->update();
    if (m_returnToRealtimeButton)
        m_returnToRealtimeButton->update();
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->update();
    if (m_statusDisplay)
        m_statusDisplay->update();
    if (m_fullscreenButton)
        m_fullscreenButton->update();
    if (m_volumeSlider)
        m_volumeSlider->update();
    if (m_volumeSliderMuteButton)
        m_volumeSliderMuteButton->update();

    updateTimeDisplay();
    updateControlVisibility();
}

void MediaControls::createControlsShadowRoot()
{
    ASSERT(!m_controlsShadowRoot);
    m_controlsShadowRoot = MediaControlShadowRootElement::create(m_mediaElement);
    m_mediaElement->renderer()->addChild(m_controlsShadowRoot->renderer());
}

void MediaControls::createPanel()
{
    ASSERT(!m_panel);
    m_panel = MediaControlPanelElement::create(m_mediaElement);
    m_panel->attachToParent(m_controlsShadowRoot.get());
}

void MediaControls::createMuteButton()
{
    ASSERT(!m_muteButton);
    m_muteButton = MediaControlPanelMuteButtonElement::create(m_mediaElement);
    m_muteButton->attachToParent(m_panel.get());
}

void MediaControls::createPlayButton()
{
    ASSERT(!m_playButton);
    m_playButton = MediaControlPlayButtonElement::create(m_mediaElement);
    m_playButton->attachToParent(m_panel.get());
}

void MediaControls::createSeekBackButton()
{
    ASSERT(!m_seekBackButton);
    m_seekBackButton = MediaControlSeekBackButtonElement::create(m_mediaElement);
    m_seekBackButton->attachToParent(m_panel.get());
}

void MediaControls::createSeekForwardButton()
{
    ASSERT(!m_seekForwardButton);
    m_seekForwardButton = MediaControlSeekForwardButtonElement::create(m_mediaElement);
    m_seekForwardButton->attachToParent(m_panel.get());
}

void MediaControls::createRewindButton()
{
    ASSERT(!m_rewindButton);
    m_rewindButton = MediaControlRewindButtonElement::create(m_mediaElement);
    m_rewindButton->attachToParent(m_panel.get());
}

void MediaControls::createReturnToRealtimeButton()
{
    ASSERT(!m_returnToRealtimeButton);
    m_returnToRealtimeButton = MediaControlReturnToRealtimeButtonElement::create(m_mediaElement);
    m_returnToRealtimeButton->attachToParent(m_panel.get());
}

void MediaControls::createToggleClosedCaptionsButton()
{
    ASSERT(!m_toggleClosedCaptionsButton);
    m_toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(m_mediaElement);
    m_toggleClosedCaptionsButton->attachToParent(m_panel.get());
}

void MediaControls::createStatusDisplay()
{
    ASSERT(!m_statusDisplay);
    m_statusDisplay = MediaControlStatusDisplayElement::create(m_mediaElement);
    m_statusDisplay->attachToParent(m_panel.get());
}

void MediaControls::createTimelineContainer()
{
    ASSERT(!m_timelineContainer);
    m_timelineContainer = MediaControlTimelineContainerElement::create(m_mediaElement);
    m_timelineContainer->attachToParent(m_panel.get());
}

void MediaControls::createTimeline()
{
    ASSERT(!m_timeline);
    m_timeline = MediaControlTimelineElement::create(m_mediaElement);
    m_timeline->setAttribute(precisionAttr, "float");
    m_timeline->attachToParent(m_timelineContainer.get());
}

void MediaControls::createVolumeSliderContainer()
{
    ASSERT(!m_volumeSliderContainer);
    m_volumeSliderContainer = MediaControlVolumeSliderContainerElement::create(m_mediaElement);
    m_volumeSliderContainer->attachToParent(m_panel.get());
}

void MediaControls::createVolumeSlider()
{
    ASSERT(!m_volumeSlider);
    m_volumeSlider = MediaControlVolumeSliderElement::create(m_mediaElement);
    m_volumeSlider->setAttribute(precisionAttr, "float");
    m_volumeSlider->setAttribute(maxAttr, "1");
    m_volumeSlider->setAttribute(valueAttr, String::number(m_mediaElement->volume()));
    m_volumeSlider->attachToParent(m_volumeSliderContainer.get());
}

void MediaControls::createVolumeSliderMuteButton()
{
    ASSERT(!m_volumeSliderMuteButton);
    m_volumeSliderMuteButton = MediaControlVolumeSliderMuteButtonElement::create(m_mediaElement);
    m_volumeSliderMuteButton->attachToParent(m_volumeSliderContainer.get());
}

void MediaControls::createCurrentTimeDisplay()
{
    ASSERT(!m_currentTimeDisplay);
    m_currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(m_mediaElement);
    m_currentTimeDisplay->attachToParent(m_timelineContainer.get());
}

void MediaControls::createTimeRemainingDisplay()
{
    ASSERT(!m_timeRemainingDisplay);
    m_timeRemainingDisplay = MediaControlTimeRemainingDisplayElement::create(m_mediaElement);
    m_timeRemainingDisplay->attachToParent(m_timelineContainer.get());
}

void MediaControls::createFullscreenButton()
{
    ASSERT(!m_fullscreenButton);
    m_fullscreenButton = MediaControlFullscreenButtonElement::create(m_mediaElement);
    m_fullscreenButton->attachToParent(m_panel.get());
}

void MediaControls::timeUpdateTimerFired(Timer<MediaControls>*)
{
    if (m_timeline)
        m_timeline->update(false);
    updateTimeDisplay();
}

void MediaControls::updateTimeDisplay()
{
    ASSERT(m_mediaElement->renderer());

    if (!m_currentTimeDisplay || !m_currentTimeDisplay->renderer() || m_currentTimeDisplay->renderer()->style()->display() == NONE || m_mediaElement->renderer()->style()->visibility() != VISIBLE)
        return;

    float now = m_mediaElement->currentTime();
    float duration = m_mediaElement->duration();

    // Allow the theme to format the time
    ExceptionCode ec;
    m_currentTimeDisplay->setInnerText(m_mediaElement->renderer()->theme()->formatMediaControlsCurrentTime(now, duration), ec);
    m_currentTimeDisplay->setCurrentValue(now);
    m_timeRemainingDisplay->setInnerText(m_mediaElement->renderer()->theme()->formatMediaControlsRemainingTime(now, duration), ec);
    m_timeRemainingDisplay->setCurrentValue(now - duration);
}

RenderBox* MediaControls::renderBox()
{
    return m_controlsShadowRoot ? m_controlsShadowRoot->renderBox() : 0;
}

void MediaControls::updateControlVisibility()
{
    if (!m_panel || !m_panel->renderer())
        return;

    // Don't fade for audio controls.
    HTMLMediaElement* media = m_mediaElement;
    if (!media->hasVideo())
        return;

    ASSERT(media->renderer());

    // Don't fade if the media element is not visible
    if (media->renderer()->style()->visibility() != VISIBLE)
        return;
    
    bool shouldHideController = !m_mouseOver && !media->canPlay();

    // Do fading manually, css animations don't work with shadow trees

    float animateFrom = m_panel->renderer()->style()->opacity();
    float animateTo = shouldHideController ? 0.0f : 1.0f;

    if (animateFrom == animateTo)
        return;

    if (m_opacityAnimationTimer.isActive()) {
        if (m_opacityAnimationTo == animateTo)
            return;
        m_opacityAnimationTimer.stop();
    }

    if (animateFrom < animateTo)
        m_opacityAnimationDuration = m_panel->renderer()->theme()->mediaControlsFadeInDuration();
    else
        m_opacityAnimationDuration = m_panel->renderer()->theme()->mediaControlsFadeOutDuration();

    m_opacityAnimationFrom = animateFrom;
    m_opacityAnimationTo = animateTo;

    m_opacityAnimationStartTime = currentTime();
    m_opacityAnimationTimer.startRepeating(cOpacityAnimationRepeatDelay);
}

void MediaControls::changeOpacity(HTMLElement* e, float opacity)
{
    if (!e || !e->renderer() || !e->renderer()->style())
        return;
    RefPtr<RenderStyle> s = RenderStyle::clone(e->renderer()->style());
    s->setOpacity(opacity);
    // z-index can't be auto if opacity is used
    s->setZIndex(0);
    e->renderer()->setStyle(s.release());
}

void MediaControls::opacityAnimationTimerFired(Timer<MediaControls>*)
{
    double time = currentTime() - m_opacityAnimationStartTime;
    if (time >= m_opacityAnimationDuration) {
        time = m_opacityAnimationDuration;
        m_opacityAnimationTimer.stop();
    }
    float opacity = narrowPrecisionToFloat(m_opacityAnimationFrom + (m_opacityAnimationTo - m_opacityAnimationFrom) * time / m_opacityAnimationDuration);
    changeOpacity(m_panel.get(), opacity);
}

void MediaControls::updateVolumeSliderContainer(bool visible)
{
    if (!m_mediaElement->hasAudio() || !m_volumeSliderContainer || !m_volumeSlider)
        return;

    if (visible && !m_volumeSliderContainer->isVisible()) {
        if (!m_muteButton || !m_muteButton->renderer() || !m_muteButton->renderBox())
            return;

        RefPtr<RenderStyle> s = m_volumeSliderContainer->styleForElement();
        int height = s->height().isPercent() ? 0 : s->height().value();
        int width = s->width().isPercent() ? 0 : s->width().value();
        IntPoint offset = m_mediaElement->document()->page()->theme()->volumeSliderOffsetFromMuteButton(m_muteButton->renderBox(), IntSize(width, height));
        int x = offset.x() + m_muteButton->renderBox()->offsetLeft();
        int y = offset.y() + m_muteButton->renderBox()->offsetTop();

        m_volumeSliderContainer->setPosition(x, y);
        m_volumeSliderContainer->setVisible(true);
        m_volumeSliderContainer->update();
        m_volumeSlider->update();
    } else if (!visible && m_volumeSliderContainer->isVisible()) {
        m_volumeSliderContainer->setVisible(false);
        m_volumeSliderContainer->updateStyle();
    }
}

void MediaControls::forwardEvent(Event* event)
{
    ASSERT(m_mediaElement->renderer());

    if (event->isMouseEvent() && m_controlsShadowRoot) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        IntPoint point(mouseEvent->absoluteLocation());

        bool defaultHandled = false;
        if (m_volumeSliderMuteButton && m_volumeSliderMuteButton->hitTest(point)) {
            m_volumeSliderMuteButton->defaultEventHandler(event);
            defaultHandled = event->defaultHandled();
        }

        bool showVolumeSlider = false;
        if (!defaultHandled && m_muteButton && m_muteButton->hitTest(point)) {
            m_muteButton->defaultEventHandler(event);
            if (event->type() != eventNames().mouseoutEvent)
                showVolumeSlider = true;
        }

        if (m_volumeSliderContainer && m_volumeSliderContainer->hitTest(point))
            showVolumeSlider = true;

        if (m_volumeSlider && m_volumeSlider->hitTest(point)) {
            m_volumeSlider->defaultEventHandler(event);
            showVolumeSlider = true;
        }

        updateVolumeSliderContainer(showVolumeSlider);

        if (m_playButton && m_playButton->hitTest(point))
            m_playButton->defaultEventHandler(event);

        if (m_seekBackButton && m_seekBackButton->hitTest(point))
            m_seekBackButton->defaultEventHandler(event);

        if (m_seekForwardButton && m_seekForwardButton->hitTest(point))
            m_seekForwardButton->defaultEventHandler(event);

        if (m_rewindButton && m_rewindButton->hitTest(point))
            m_rewindButton->defaultEventHandler(event);

        if (m_returnToRealtimeButton && m_returnToRealtimeButton->hitTest(point))
            m_returnToRealtimeButton->defaultEventHandler(event);

       if (m_toggleClosedCaptionsButton && m_toggleClosedCaptionsButton->hitTest(point))
            m_toggleClosedCaptionsButton->defaultEventHandler(event);

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
            m_mouseOver = mouseOverRenderer && mouseOverRenderer->isDescendantOf(m_mediaElement->renderer());
            updateControlVisibility();
        }
    }
}

// We want the timeline slider to be at least 100 pixels wide.
static const int minWidthToDisplayTimeDisplays = 16 + 16 + 45 + 100 + 45 + 16 + 1;

void MediaControls::updateTimeDisplayVisibility()
{
    ASSERT(m_mediaElement->renderer());

    if (!m_currentTimeDisplay && !m_timeRemainingDisplay)
        return;

    int width = m_mediaElement->renderBox()->width();
    bool shouldShowTimeDisplays = width >= minWidthToDisplayTimeDisplays * m_mediaElement->renderer()->style()->effectiveZoom();

    m_currentTimeDisplay->setVisible(shouldShowTimeDisplays);
    m_timeRemainingDisplay->setVisible(shouldShowTimeDisplays);
}

}

#endif
