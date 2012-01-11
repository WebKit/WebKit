/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "MediaControlRootElement.h"

#include "Chrome.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderTheme.h"
#include "Text.h"

#if ENABLE(VIDEO_TRACK)
#include "TextTrackCue.h"
#endif

using namespace std;

namespace WebCore {

static const double timeWithoutMouseMovementBeforeHidingControls = 3;

MediaControlRootElement::MediaControlRootElement(Document* document)
    : MediaControls(document)
    , m_mediaController(0)
    , m_rewindButton(0)
    , m_playButton(0)
    , m_returnToRealTimeButton(0)
    , m_statusDisplay(0)
    , m_currentTimeDisplay(0)
    , m_timeline(0)
    , m_timeRemainingDisplay(0)
    , m_timelineContainer(0)
    , m_seekBackButton(0)
    , m_seekForwardButton(0)
    , m_toggleClosedCaptionsButton(0)
    , m_panelMuteButton(0)
    , m_volumeSlider(0)
    , m_volumeSliderMuteButton(0)
    , m_volumeSliderContainer(0)
    , m_fullScreenButton(0)
    , m_fullScreenMinVolumeButton(0)
    , m_fullScreenVolumeSlider(0)
    , m_fullScreenMaxVolumeButton(0)
    , m_panel(0)
#if ENABLE(VIDEO_TRACK)
    , m_textDisplayContainer(0)
    , m_textTrackDisplay(0)
#endif
    , m_hideFullscreenControlsTimer(this, &MediaControlRootElement::hideFullscreenControlsTimerFired)
    , m_isMouseOverControls(false)
{
}

PassRefPtr<MediaControls> MediaControls::create(Document* document)
{
    return MediaControlRootElement::create(document);
}

PassRefPtr<MediaControlRootElement> MediaControlRootElement::create(Document* document)
{
    if (!document->page())
        return 0;

    RefPtr<MediaControlRootElement> controls = adoptRef(new MediaControlRootElement(document));

    RefPtr<MediaControlPanelElement> panel = MediaControlPanelElement::create(document);

    ExceptionCode ec;

    RefPtr<MediaControlRewindButtonElement> rewindButton = MediaControlRewindButtonElement::create(document);
    controls->m_rewindButton = rewindButton.get();
    panel->appendChild(rewindButton.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlPlayButtonElement> playButton = MediaControlPlayButtonElement::create(document);
    controls->m_playButton = playButton.get();
    panel->appendChild(playButton.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlReturnToRealtimeButtonElement> returnToRealtimeButton = MediaControlReturnToRealtimeButtonElement::create(document);
    controls->m_returnToRealTimeButton = returnToRealtimeButton.get();
    panel->appendChild(returnToRealtimeButton.release(), ec, true);
    if (ec)
        return 0;

    if (document->page()->theme()->usesMediaControlStatusDisplay()) {
        RefPtr<MediaControlStatusDisplayElement> statusDisplay = MediaControlStatusDisplayElement::create(document);
        controls->m_statusDisplay = statusDisplay.get();
        panel->appendChild(statusDisplay.release(), ec, true);
        if (ec)
            return 0;
    }

    RefPtr<MediaControlTimelineContainerElement> timelineContainer = MediaControlTimelineContainerElement::create(document);

    RefPtr<MediaControlCurrentTimeDisplayElement> currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(document);
    controls->m_currentTimeDisplay = currentTimeDisplay.get();
    timelineContainer->appendChild(currentTimeDisplay.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlTimelineElement> timeline = MediaControlTimelineElement::create(document, controls.get());
    controls->m_timeline = timeline.get();
    timelineContainer->appendChild(timeline.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlTimeRemainingDisplayElement> timeRemainingDisplay = MediaControlTimeRemainingDisplayElement::create(document);
    controls->m_timeRemainingDisplay = timeRemainingDisplay.get();
    timelineContainer->appendChild(timeRemainingDisplay.release(), ec, true);
    if (ec)
        return 0;

    controls->m_timelineContainer = timelineContainer.get();
    panel->appendChild(timelineContainer.release(), ec, true);
    if (ec)
        return 0;

    // FIXME: Only create when needed <http://webkit.org/b/57163>
    RefPtr<MediaControlSeekBackButtonElement> seekBackButton = MediaControlSeekBackButtonElement::create(document);
    controls->m_seekBackButton = seekBackButton.get();
    panel->appendChild(seekBackButton.release(), ec, true);
    if (ec)
        return 0;

    // FIXME: Only create when needed <http://webkit.org/b/57163>
    RefPtr<MediaControlSeekForwardButtonElement> seekForwardButton = MediaControlSeekForwardButtonElement::create(document);
    controls->m_seekForwardButton = seekForwardButton.get();
    panel->appendChild(seekForwardButton.release(), ec, true);
    if (ec)
        return 0;

    if (document->page()->theme()->supportsClosedCaptioning()) {
        RefPtr<MediaControlToggleClosedCaptionsButtonElement> toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(document);
        controls->m_toggleClosedCaptionsButton = toggleClosedCaptionsButton.get();
        panel->appendChild(toggleClosedCaptionsButton.release(), ec, true);
        if (ec)
            return 0;
    }

    // FIXME: Only create when needed <http://webkit.org/b/57163>
    RefPtr<MediaControlFullscreenButtonElement> fullScreenButton = MediaControlFullscreenButtonElement::create(document, controls.get());
    controls->m_fullScreenButton = fullScreenButton.get();
    panel->appendChild(fullScreenButton.release(), ec, true);

    RefPtr<MediaControlPanelMuteButtonElement> panelMuteButton = MediaControlPanelMuteButtonElement::create(document, controls.get());
    controls->m_panelMuteButton = panelMuteButton.get();
    panel->appendChild(panelMuteButton.release(), ec, true);
    if (ec)
        return 0;

    if (document->page()->theme()->usesMediaControlVolumeSlider()) {
        RefPtr<MediaControlVolumeSliderContainerElement> volumeSliderContainer = MediaControlVolumeSliderContainerElement::create(document);

        RefPtr<MediaControlVolumeSliderElement> slider = MediaControlVolumeSliderElement::create(document);
        controls->m_volumeSlider = slider.get();
        volumeSliderContainer->appendChild(slider.release(), ec, true);
        if (ec)
            return 0;

        RefPtr<MediaControlVolumeSliderMuteButtonElement> volumeSliderMuteButton = MediaControlVolumeSliderMuteButtonElement::create(document);
        controls->m_volumeSliderMuteButton = volumeSliderMuteButton.get();
        volumeSliderContainer->appendChild(volumeSliderMuteButton.release(), ec, true);
        if (ec)
            return 0;

        controls->m_volumeSliderContainer = volumeSliderContainer.get();
        panel->appendChild(volumeSliderContainer.release(), ec, true);
        if (ec)
            return 0;
    }

    // FIXME: Only create when needed <http://webkit.org/b/57163>
    RefPtr<MediaControlFullscreenVolumeMinButtonElement> fullScreenMinVolumeButton = MediaControlFullscreenVolumeMinButtonElement::create(document);
    controls->m_fullScreenMinVolumeButton = fullScreenMinVolumeButton.get();
    panel->appendChild(fullScreenMinVolumeButton.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlFullscreenVolumeSliderElement> fullScreenVolumeSlider = MediaControlFullscreenVolumeSliderElement::create(document);
    controls->m_fullScreenVolumeSlider = fullScreenVolumeSlider.get();
    panel->appendChild(fullScreenVolumeSlider.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlFullscreenVolumeMaxButtonElement> fullScreenMaxVolumeButton = MediaControlFullscreenVolumeMaxButtonElement::create(document);
    controls->m_fullScreenMaxVolumeButton = fullScreenMaxVolumeButton.get();
    panel->appendChild(fullScreenMaxVolumeButton.release(), ec, true);
    if (ec)
        return 0;

    controls->m_panel = panel.get();
    controls->appendChild(panel.release(), ec, true);
    if (ec)
        return 0;

    return controls.release();
}

void MediaControlRootElement::setMediaController(MediaControllerInterface* controller)
{
    if (m_mediaController == controller)
        return;
    m_mediaController = controller;

    if (m_rewindButton)
        m_rewindButton->setMediaController(controller);
    if (m_playButton)
        m_playButton->setMediaController(controller);
    if (m_returnToRealTimeButton)
        m_returnToRealTimeButton->setMediaController(controller);
    if (m_statusDisplay)
        m_statusDisplay->setMediaController(controller);
    if (m_currentTimeDisplay)
        m_currentTimeDisplay->setMediaController(controller);
    if (m_timeline)
        m_timeline->setMediaController(controller);
    if (m_timeRemainingDisplay)
        m_timeRemainingDisplay->setMediaController(controller);
    if (m_timelineContainer)
        m_timelineContainer->setMediaController(controller);
    if (m_seekBackButton)
        m_seekBackButton->setMediaController(controller);
    if (m_seekForwardButton)
        m_seekForwardButton->setMediaController(controller);
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->setMediaController(controller);
    if (m_panelMuteButton)
        m_panelMuteButton->setMediaController(controller);
    if (m_volumeSlider)
        m_volumeSlider->setMediaController(controller);
    if (m_volumeSliderMuteButton)
        m_volumeSliderMuteButton->setMediaController(controller);
    if (m_volumeSliderContainer)
        m_volumeSliderContainer->setMediaController(controller);
    if (m_fullScreenButton)
        m_fullScreenButton->setMediaController(controller);
    if (m_fullScreenMinVolumeButton)
        m_fullScreenMinVolumeButton->setMediaController(controller);
    if (m_fullScreenVolumeSlider)
        m_fullScreenVolumeSlider->setMediaController(controller);
    if (m_fullScreenMaxVolumeButton)
        m_fullScreenMaxVolumeButton->setMediaController(controller);
    if (m_panel)
        m_panel->setMediaController(controller);
#if ENABLE(VIDEO_TRACK)
    if (m_textDisplayContainer)
        m_textDisplayContainer->setMediaController(controller);
    if (m_textTrackDisplay)
        m_textTrackDisplay->setMediaController(controller);
#endif
    reset();
}

void MediaControlRootElement::show()
{
    m_panel->show();
}

void MediaControlRootElement::hide()
{
    m_panel->hide();
}

void MediaControlRootElement::makeOpaque()
{
    m_panel->makeOpaque();
}

void MediaControlRootElement::makeTransparent()
{
    m_panel->makeTransparent();
}

void MediaControlRootElement::reset()
{
    Page* page = document()->page();
    if (!page)
        return;

    updateStatusDisplay();

    if (m_mediaController->supportsFullscreen())
        m_fullScreenButton->show();
    else
        m_fullScreenButton->hide();

    float duration = m_mediaController->duration();
    if (isfinite(duration) || page->theme()->hasOwnDisabledStateHandlingFor(MediaSliderPart)) {
        m_timeline->setDuration(duration);
        m_timelineContainer->show();
        m_timeline->setPosition(m_mediaController->currentTime());
        updateTimeDisplay();
    } else
        m_timelineContainer->hide();

    if (m_mediaController->hasAudio() || page->theme()->hasOwnDisabledStateHandlingFor(MediaMuteButtonPart))
        m_panelMuteButton->show();
    else
        m_panelMuteButton->hide();

    if (m_volumeSlider)
        m_volumeSlider->setVolume(m_mediaController->volume());

    if (m_toggleClosedCaptionsButton) {
        if (m_mediaController->hasClosedCaptions())
            m_toggleClosedCaptionsButton->show();
        else
            m_toggleClosedCaptionsButton->hide();
    }

    m_playButton->updateDisplayType();

#if ENABLE(FULLSCREEN_API)
    if (document()->webkitIsFullScreen() && document()->webkitCurrentFullScreenElement() == toParentMediaElement(this)) {
        if (m_mediaController->isLiveStream()) {
            m_seekBackButton->hide();
            m_seekForwardButton->hide();
            m_rewindButton->show();
            m_returnToRealTimeButton->show();
        } else {
            m_seekBackButton->show();
            m_seekForwardButton->show();
            m_rewindButton->hide();
            m_returnToRealTimeButton->hide();
        }
    } else
#endif
    if (!m_mediaController->isLiveStream()) {
        m_returnToRealTimeButton->hide();
        m_rewindButton->show();
    } else {
        m_returnToRealTimeButton->show();
        m_rewindButton->hide();
    }

    makeOpaque();
}

void MediaControlRootElement::playbackStarted()
{
    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();

    if (m_mediaController->isFullscreen())
        startHideFullscreenControlsTimer();
}

void MediaControlRootElement::playbackProgressed()
{
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();
    
    if (!m_isMouseOverControls && m_mediaController->hasVideo())
        makeTransparent();
}

void MediaControlRootElement::playbackStopped()
{
    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();
    makeOpaque();
    
    stopHideFullscreenControlsTimer();
}

void MediaControlRootElement::updateTimeDisplay()
{
    float now = m_mediaController->currentTime();
    float duration = m_mediaController->duration();

    Page* page = document()->page();
    if (!page)
        return;

    // Allow the theme to format the time.
    ExceptionCode ec;
    m_currentTimeDisplay->setInnerText(page->theme()->formatMediaControlsCurrentTime(now, duration), ec);
    m_currentTimeDisplay->setCurrentValue(now);
    m_timeRemainingDisplay->setInnerText(page->theme()->formatMediaControlsRemainingTime(now, duration), ec);
    m_timeRemainingDisplay->setCurrentValue(now - duration);
}

void MediaControlRootElement::reportedError()
{
    Page* page = document()->page();
    if (!page)
        return;

    if (!page->theme()->hasOwnDisabledStateHandlingFor(MediaSliderPart))
        m_timelineContainer->hide();

    if (!page->theme()->hasOwnDisabledStateHandlingFor(MediaMuteButtonPart))
        m_panelMuteButton->hide();

     m_fullScreenButton->hide();

    if (m_volumeSliderContainer)
        m_volumeSliderContainer->hide();
    if (m_toggleClosedCaptionsButton && !page->theme()->hasOwnDisabledStateHandlingFor(MediaToggleClosedCaptionsButtonPart))
        m_toggleClosedCaptionsButton->hide();
}

void MediaControlRootElement::updateStatusDisplay()
{
    if (m_statusDisplay)
        m_statusDisplay->update();
}

void MediaControlRootElement::loadedMetadata()
{
    if (m_statusDisplay && m_mediaController->isLiveStream())
        m_statusDisplay->hide();

    reset();
}

void MediaControlRootElement::changedClosedCaptionsVisibility()
{
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControlRootElement::changedMute()
{
    m_panelMuteButton->changedMute();
    if (m_volumeSliderMuteButton)
        m_volumeSliderMuteButton->changedMute();
}

void MediaControlRootElement::changedVolume()
{
    if (m_volumeSlider)
        m_volumeSlider->setVolume(m_mediaController->volume());
}

void MediaControlRootElement::enteredFullscreen()
{
    if (m_mediaController->isLiveStream()) {
        m_seekBackButton->hide();
        m_seekForwardButton->hide();
        m_rewindButton->show();
        m_returnToRealTimeButton->show();
    } else {
        m_seekBackButton->show();
        m_seekForwardButton->show();
        m_rewindButton->hide();
        m_returnToRealTimeButton->hide();
    }

    m_panel->setCanBeDragged(true);

    if (Page* page = document()->page())
        page->chrome()->setCursorHiddenUntilMouseMoves(true);

    startHideFullscreenControlsTimer();
}

void MediaControlRootElement::exitedFullscreen()
{
    // "show" actually means removal of display:none style, so we are just clearing styles
    // when exiting fullscreen.
    // FIXME: Clarify naming of show/hide <http://webkit.org/b/58157>
    m_rewindButton->show();
    m_seekBackButton->show();
    m_seekForwardButton->show();
    m_returnToRealTimeButton->show();

    m_panel->setCanBeDragged(false);

    // We will keep using the panel, but we want it to go back to the standard position.
    // This will matter right away because we use the panel even when not fullscreen.
    // And if we reenter fullscreen we also want the panel in the standard position.
    m_panel->resetPosition();

    stopHideFullscreenControlsTimer();    
}

void MediaControlRootElement::showVolumeSlider()
{
    if (!m_mediaController->hasAudio())
        return;

    if (m_volumeSliderContainer)
        m_volumeSliderContainer->show();
}

bool MediaControlRootElement::shouldHideControls()
{
    return !m_panel->hovered();
}

bool MediaControlRootElement::containsRelatedTarget(Event* event)
{
    if (!event->isMouseEvent())
        return false;
    EventTarget* relatedTarget = static_cast<MouseEvent*>(event)->relatedTarget();
    if (!relatedTarget)
        return false;
    return contains(relatedTarget->toNode());
}

void MediaControlRootElement::defaultEventHandler(Event* event)
{
    MediaControls::defaultEventHandler(event);

    if (event->type() == eventNames().mouseoverEvent) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = true;
            if (!m_mediaController->canPlay()) {
                makeOpaque();
                if (shouldHideControls())
                    startHideFullscreenControlsTimer();
            }
        }
    } else if (event->type() == eventNames().mouseoutEvent) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = false;
            stopHideFullscreenControlsTimer();
        }
    } else if (event->type() == eventNames().mousemoveEvent) {
        if (m_mediaController->isFullscreen()) {
            // When we get a mouse move in fullscreen mode, show the media controls, and start a timer
            // that will hide the media controls after a 3 seconds without a mouse move.
            makeOpaque();
            if (shouldHideControls())
                startHideFullscreenControlsTimer();
        }
    }
}

void MediaControlRootElement::startHideFullscreenControlsTimer()
{
    if (!m_mediaController->isFullscreen())
        return;
    
    m_hideFullscreenControlsTimer.startOneShot(timeWithoutMouseMovementBeforeHidingControls);
}

void MediaControlRootElement::hideFullscreenControlsTimerFired(Timer<MediaControlRootElement>*)
{
    if (m_mediaController->paused())
        return;
    
    if (!m_mediaController->isFullscreen())
        return;
    
    if (!shouldHideControls())
        return;

    if (Page* page = document()->page())
        page->chrome()->setCursorHiddenUntilMouseMoves(true);

    makeTransparent();
}

void MediaControlRootElement::stopHideFullscreenControlsTimer()
{
    m_hideFullscreenControlsTimer.stop();
}

#if ENABLE(VIDEO_TRACK)
void MediaControlRootElement::createTextTrackDisplay()
{
    if (m_textDisplayContainer)
        return;

    RefPtr<MediaControlTextTrackContainerElement> textDisplayContainer = MediaControlTextTrackContainerElement::create(document());
    m_textDisplayContainer = textDisplayContainer.get();

    RefPtr<MediaControlTextTrackDisplayElement> textDisplay = MediaControlTextTrackDisplayElement::create(document());
    m_textDisplayContainer->hide();
    m_textTrackDisplay = textDisplay.get();

    ExceptionCode ec;
    textDisplayContainer->appendChild(textDisplay.release(), ec, true);
    if (ec)
        return;

    // Insert it before the first controller element so it always displays behind the controls.
    insertBefore(textDisplayContainer.release(), m_panel, ec, true);
}

void MediaControlRootElement::showTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->show();
}

void MediaControlRootElement::hideTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->hide();
}

void MediaControlRootElement::updateTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();

    CueList activeCues = toParentMediaElement(m_textDisplayContainer)->currentlyActiveCues();
    m_textTrackDisplay->removeChildren();
    bool nothingToDisplay = true;
    for (size_t i = 0; i < activeCues.size(); ++i) {
        TextTrackCue* cue = activeCues[i].data();
        ASSERT(cue->isActive());
        if (!cue->track() || cue->track()->mode() != TextTrack::SHOWING)
            continue;

        String cueText = cue->text();
        if (!cueText.isEmpty()) {
            if (!nothingToDisplay)
                m_textTrackDisplay->appendChild(document()->createElement(HTMLNames::brTag, false), ASSERT_NO_EXCEPTION);
            m_textTrackDisplay->appendChild(document()->createTextNode(cueText), ASSERT_NO_EXCEPTION);
            nothingToDisplay = false;
        }
    }

    if (!nothingToDisplay)
        m_textDisplayContainer->show();
    else
        m_textDisplayContainer->hide();
}
#endif

const AtomicString& MediaControlRootElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls"));
    return id;
}

void MediaControlRootElement::bufferingProgressed()
{
    // We only need to update buffering progress when paused, during normal
    // playback playbackProgressed() will take care of it.
    if (m_mediaController->paused())
        m_timeline->setPosition(m_mediaController->currentTime());
}

}

#endif
