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
#include "MediaControlRootElementChromium.h"

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

MediaControlRootElementChromium::MediaControlRootElementChromium(Document* document)
    : MediaControls(document)
    , m_mediaController(0)
    , m_playButton(0)
    , m_currentTimeDisplay(0)
    , m_timeline(0)
    , m_timelineContainer(0)
    , m_panelMuteButton(0)
    , m_volumeSlider(0)
    , m_volumeSliderContainer(0)
    , m_panel(0)
#if ENABLE(VIDEO_TRACK)
    , m_textDisplayContainer(0)
    , m_textTrackDisplay(0)
#endif
    , m_opaque(true)
    , m_isMouseOverControls(false)
{
}

PassRefPtr<MediaControls> MediaControls::create(Document* document)
{
    return MediaControlRootElementChromium::create(document);
}

PassRefPtr<MediaControlRootElementChromium> MediaControlRootElementChromium::create(Document* document)
{
    if (!document->page())
        return 0;

    RefPtr<MediaControlRootElementChromium> controls = adoptRef(new MediaControlRootElementChromium(document));

    RefPtr<MediaControlPanelElement> panel = MediaControlPanelElement::create(document);

    ExceptionCode ec;

    RefPtr<MediaControlPlayButtonElement> playButton = MediaControlPlayButtonElement::create(document);
    controls->m_playButton = playButton.get();
    panel->appendChild(playButton.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlTimelineContainerElement> timelineContainer = MediaControlTimelineContainerElement::create(document);

    RefPtr<MediaControlTimelineElement> timeline = MediaControlTimelineElement::create(document, controls.get());
    controls->m_timeline = timeline.get();
    timelineContainer->appendChild(timeline.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlCurrentTimeDisplayElement> currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(document);
    controls->m_currentTimeDisplay = currentTimeDisplay.get();
    timelineContainer->appendChild(currentTimeDisplay.release(), ec, true);
    if (ec)
        return 0;

    controls->m_timelineContainer = timelineContainer.get();
    panel->appendChild(timelineContainer.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlPanelMuteButtonElement> panelMuteButton = MediaControlPanelMuteButtonElement::create(document, controls.get());
    controls->m_panelMuteButton = panelMuteButton.get();
    panel->appendChild(panelMuteButton.release(), ec, true);
    if (ec)
        return 0;

    RefPtr<MediaControlVolumeSliderContainerElement> volumeSliderContainer = MediaControlVolumeSliderContainerElement::create(document);

    RefPtr<MediaControlVolumeSliderElement> slider = MediaControlVolumeSliderElement::create(document);
    controls->m_volumeSlider = slider.get();
    volumeSliderContainer->appendChild(slider.release(), ec, true);
    if (ec)
        return 0;

    controls->m_volumeSliderContainer = volumeSliderContainer.get();
    panel->appendChild(volumeSliderContainer.release(), ec, true);
    if (ec)
        return 0;

    controls->m_panel = panel.get();
    controls->appendChild(panel.release(), ec, true);
    if (ec)
        return 0;

    return controls.release();
}

void MediaControlRootElementChromium::setMediaController(MediaControllerInterface* controller)
{
    if (m_mediaController == controller)
        return;
    m_mediaController = controller;

    if (m_playButton)
        m_playButton->setMediaController(controller);
    if (m_currentTimeDisplay)
        m_currentTimeDisplay->setMediaController(controller);
    if (m_timeline)
        m_timeline->setMediaController(controller);
    if (m_timelineContainer)
        m_timelineContainer->setMediaController(controller);
    if (m_panelMuteButton)
        m_panelMuteButton->setMediaController(controller);
    if (m_volumeSlider)
        m_volumeSlider->setMediaController(controller);
    if (m_volumeSliderContainer)
        m_volumeSliderContainer->setMediaController(controller);
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

void MediaControlRootElementChromium::show()
{
    m_panel->show();
}

void MediaControlRootElementChromium::hide()
{
    m_panel->hide();
}

void MediaControlRootElementChromium::makeOpaque()
{
    m_panel->makeOpaque();
}

void MediaControlRootElementChromium::makeTransparent()
{
    m_panel->makeTransparent();
}

void MediaControlRootElementChromium::reset()
{
    Page* page = document()->page();
    if (!page)
        return;

    updateStatusDisplay();

    float duration = m_mediaController->duration();
    m_timeline->setDuration(duration);
    m_timelineContainer->show();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();

    m_panelMuteButton->show();

    if (m_volumeSlider)
        m_volumeSlider->setVolume(m_mediaController->volume());

    makeOpaque();
}

void MediaControlRootElementChromium::playbackStarted()
{
    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();
}

void MediaControlRootElementChromium::playbackProgressed()
{
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();

    if (!m_isMouseOverControls && m_mediaController->hasVideo())
        makeTransparent();
}

void MediaControlRootElementChromium::playbackStopped()
{
    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateTimeDisplay();
    makeOpaque();
}

void MediaControlRootElementChromium::updateTimeDisplay()
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
}

void MediaControlRootElementChromium::reportedError()
{
    Page* page = document()->page();
    if (!page)
        return;

    m_timelineContainer->hide();
    m_panelMuteButton->hide();
    m_volumeSliderContainer->hide();
}

void MediaControlRootElementChromium::updateStatusDisplay()
{
}

bool MediaControlRootElementChromium::shouldHideControls()
{
    return !m_panel->hovered();
}

void MediaControlRootElementChromium::loadedMetadata()
{
    reset();
}

bool MediaControlRootElementChromium::containsRelatedTarget(Event* event)
{
    if (!event->isMouseEvent())
        return false;
    EventTarget* relatedTarget = static_cast<MouseEvent*>(event)->relatedTarget();
    if (!relatedTarget)
        return false;
    return contains(relatedTarget->toNode());
}

void MediaControlRootElementChromium::defaultEventHandler(Event* event)
{
    MediaControls::defaultEventHandler(event);

    if (event->type() == eventNames().mouseoverEvent) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = true;
            if (!m_mediaController->canPlay())
                makeOpaque();
        }
    } else if (event->type() == eventNames().mouseoutEvent) {
        if (!containsRelatedTarget(event))
            m_isMouseOverControls = false;
    }
}

void MediaControlRootElementChromium::changedClosedCaptionsVisibility()
{
}

void MediaControlRootElementChromium::changedMute()
{
    m_panelMuteButton->changedMute();
}

void MediaControlRootElementChromium::changedVolume()
{
    m_volumeSlider->setVolume(m_mediaController->volume());
}

void MediaControlRootElementChromium::enteredFullscreen()
{
}

void MediaControlRootElementChromium::exitedFullscreen()
{
}

void MediaControlRootElementChromium::showVolumeSlider()
{
    if (!m_mediaController->hasAudio())
        return;

    m_volumeSliderContainer->show();
}

#if ENABLE(VIDEO_TRACK)
void MediaControlRootElementChromium::createTextTrackDisplay()
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

void MediaControlRootElementChromium::showTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->show();
}

void MediaControlRootElementChromium::hideTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->hide();
}

void MediaControlRootElementChromium::updateTextTrackDisplay()
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
    
const AtomicString& MediaControlRootElementChromium::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls"));
    return id;
}

}

#endif
