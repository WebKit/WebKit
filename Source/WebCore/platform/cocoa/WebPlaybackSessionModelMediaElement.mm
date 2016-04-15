/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebPlaybackSessionModelMediaElement.h"

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "DOMEventInternal.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "WebVideoFullscreenInterface.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/DOMEventListener.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextTrackList.h>
#import <WebCore/TimeRanges.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

WebPlaybackSessionModelMediaElement::WebPlaybackSessionModelMediaElement()
    : EventListener(EventListener::CPPEventListenerType)
{
}

WebPlaybackSessionModelMediaElement::~WebPlaybackSessionModelMediaElement()
{
}

void WebPlaybackSessionModelMediaElement::setWebPlaybackSessionInterface(WebPlaybackSessionInterface* interface)
{
    if (interface == m_playbackSessionInterface)
        return;

    m_playbackSessionInterface = interface;

    if (m_playbackSessionInterface) {
        m_playbackSessionInterface->resetMediaState();
        if (m_mediaElement)
            m_playbackSessionInterface->setWirelessVideoPlaybackDisabled(m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled(*m_mediaElement));
    }
}

void WebPlaybackSessionModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (m_mediaElement == mediaElement)
        return;

    if (m_playbackSessionInterface)
        m_playbackSessionInterface->resetMediaState();

    if (m_mediaElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->removeEventListener(eventName, this, false);
    }
    m_isListening = false;

    m_mediaElement = mediaElement;

    if (!m_mediaElement)
        return;

    for (auto& eventName : observedEventNames())
        m_mediaElement->addEventListener(eventName, this, false);
    m_isListening = true;

    updateForEventName(eventNameAll());

    if (m_playbackSessionInterface)
        m_playbackSessionInterface->setWirelessVideoPlaybackDisabled(m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled(*m_mediaElement));
}

void WebPlaybackSessionModelMediaElement::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event* event)
{
    updateForEventName(event->type());
}

void WebPlaybackSessionModelMediaElement::updateForEventName(const WTF::AtomicString& eventName)
{
    if (!m_mediaElement || !m_playbackSessionInterface)
        return;

    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().durationchangeEvent) {
        m_playbackSessionInterface->setDuration(m_mediaElement->duration());
        // These is no standard event for minFastReverseRateChange; duration change is a reasonable proxy for it.
        // It happens every time a new item becomes ready to play.
        m_playbackSessionInterface->setCanPlayFastReverse(m_mediaElement->minFastReverseRate() < 0.0);
    }

    if (all
        || eventName == eventNames().pauseEvent
        || eventName == eventNames().playEvent
        || eventName == eventNames().ratechangeEvent)
        m_playbackSessionInterface->setRate(!m_mediaElement->paused(), m_mediaElement->playbackRate());

    if (all
        || eventName == eventNames().timeupdateEvent) {
        m_playbackSessionInterface->setCurrentTime(m_mediaElement->currentTime(), [[NSProcessInfo processInfo] systemUptime]);
        m_playbackSessionInterface->setBufferedTime(m_mediaElement->maxBufferedTime());
        // FIXME: 130788 - find a better event to update seekable ranges from.
        m_playbackSessionInterface->setSeekableRanges(*m_mediaElement->seekable());
    }

    if (all
        || eventName == eventNames().addtrackEvent
        || eventName == eventNames().removetrackEvent)
        updateLegibleOptions();

    if (all
        || eventName == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        bool enabled = m_mediaElement->webkitCurrentPlaybackTargetIsWireless();
        WebPlaybackSessionInterface::ExternalPlaybackTargetType targetType = WebPlaybackSessionInterface::TargetTypeNone;
        String localizedDeviceName;

        if (m_mediaElement->mediaControlsHost()) {
            static NeverDestroyed<String> airplay(ASCIILiteral("airplay"));
            static NeverDestroyed<String> tvout(ASCIILiteral("tvout"));

            String type = m_mediaElement->mediaControlsHost()->externalDeviceType();
            if (type == airplay)
                targetType = WebPlaybackSessionInterface::TargetTypeAirPlay;
            else if (type == tvout)
                targetType = WebPlaybackSessionInterface::TargetTypeTVOut;
            localizedDeviceName = m_mediaElement->mediaControlsHost()->externalDeviceDisplayName();
        }
        m_playbackSessionInterface->setExternalPlayback(enabled, targetType, localizedDeviceName);
        m_playbackSessionInterface->setWirelessVideoPlaybackDisabled(m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled(*m_mediaElement));
    }
}

void WebPlaybackSessionModelMediaElement::play()
{
    if (m_mediaElement)
        m_mediaElement->play();
}

void WebPlaybackSessionModelMediaElement::pause()
{
    if (m_mediaElement)
        m_mediaElement->pause();
}

void WebPlaybackSessionModelMediaElement::togglePlayState()
{
    if (m_mediaElement)
        m_mediaElement->togglePlayState();
}

void WebPlaybackSessionModelMediaElement::beginScrubbing()
{
    if (m_mediaElement)
        m_mediaElement->beginScrubbing();
}

void WebPlaybackSessionModelMediaElement::endScrubbing()
{
    if (m_mediaElement)
        m_mediaElement->endScrubbing();
}

void WebPlaybackSessionModelMediaElement::seekToTime(double time)
{
    if (m_mediaElement)
        m_mediaElement->setCurrentTime(time);
}

void WebPlaybackSessionModelMediaElement::fastSeek(double time)
{
    if (m_mediaElement)
        m_mediaElement->fastSeek(time);
}

void WebPlaybackSessionModelMediaElement::beginScanningForward()
{
    if (m_mediaElement)
        m_mediaElement->beginScanning(MediaControllerInterface::Forward);
}

void WebPlaybackSessionModelMediaElement::beginScanningBackward()
{
    if (m_mediaElement)
        m_mediaElement->beginScanning(MediaControllerInterface::Backward);
}

void WebPlaybackSessionModelMediaElement::endScanning()
{
    if (m_mediaElement)
        m_mediaElement->endScanning();
}

void WebPlaybackSessionModelMediaElement::selectAudioMediaOption(uint64_t selectedAudioIndex)
{
    ASSERT(selectedAudioIndex < std::numeric_limits<size_t>::max());
    AudioTrack* selectedAudioTrack = nullptr;

    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        auto& audioTrack = m_audioTracksForMenu[index];
        audioTrack->setEnabled(index == static_cast<size_t>(selectedAudioIndex));
        if (audioTrack->enabled())
            selectedAudioTrack = audioTrack.get();
    }

    m_mediaElement->audioTrackEnabledChanged(selectedAudioTrack);
}

void WebPlaybackSessionModelMediaElement::selectLegibleMediaOption(uint64_t index)
{
    ASSERT(index < std::numeric_limits<size_t>::max());
    TextTrack* textTrack = nullptr;

    if (index < m_legibleTracksForMenu.size())
        textTrack = m_legibleTracksForMenu[static_cast<size_t>(index)].get();
    else
        textTrack = TextTrack::captionMenuOffItem();

    m_mediaElement->setSelectedTextTrack(textTrack);
}

void WebPlaybackSessionModelMediaElement::updateLegibleOptions()
{
    AudioTrackList* audioTrackList = m_mediaElement->audioTracks();
    TextTrackList* trackList = m_mediaElement->textTracks();

    if ((!trackList && !audioTrackList) || !m_mediaElement->document().page() || !m_mediaElement->mediaControlsHost())
        return;

    AtomicString displayMode = m_mediaElement->mediaControlsHost()->captionDisplayMode();
    TextTrack* offItem = m_mediaElement->mediaControlsHost()->captionMenuOffItem();
    TextTrack* automaticItem = m_mediaElement->mediaControlsHost()->captionMenuAutomaticItem();

    auto& captionPreferences = m_mediaElement->document().page()->group().captionPreferences();
    m_legibleTracksForMenu = captionPreferences.sortedTrackListForMenu(trackList);
    m_audioTracksForMenu = captionPreferences.sortedTrackListForMenu(audioTrackList);

    Vector<String> audioTrackDisplayNames;
    uint64_t selectedAudioIndex = 0;

    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        auto& track = m_audioTracksForMenu[index];
        audioTrackDisplayNames.append(captionPreferences.displayNameForTrack(track.get()));

        if (track->enabled())
            selectedAudioIndex = index;
    }

    m_playbackSessionInterface->setAudioMediaSelectionOptions(audioTrackDisplayNames, selectedAudioIndex);

    Vector<String> trackDisplayNames;
    uint64_t selectedIndex = 0;
    uint64_t offIndex = 0;
    bool trackMenuItemSelected = false;

    for (size_t index = 0; index < m_legibleTracksForMenu.size(); index++) {
        auto& track = m_legibleTracksForMenu[index];
        trackDisplayNames.append(captionPreferences.displayNameForTrack(track.get()));

        if (track == offItem)
            offIndex = index;

        if (track == automaticItem && displayMode == MediaControlsHost::automaticKeyword()) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }

        if (displayMode != MediaControlsHost::automaticKeyword() && track->mode() == TextTrack::showingKeyword()) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }
    }

    if (offIndex && !trackMenuItemSelected && displayMode == MediaControlsHost::forcedOnlyKeyword()) {
        selectedIndex = offIndex;
        trackMenuItemSelected = true;
    }

    m_playbackSessionInterface->setLegibleMediaSelectionOptions(trackDisplayNames, selectedIndex);
}

const Vector<AtomicString>&  WebPlaybackSessionModelMediaElement::observedEventNames()
{
    static NeverDestroyed<Vector<AtomicString>> sEventNames;

    if (!sEventNames.get().size()) {
        sEventNames.get().append(eventNames().durationchangeEvent);
        sEventNames.get().append(eventNames().pauseEvent);
        sEventNames.get().append(eventNames().playEvent);
        sEventNames.get().append(eventNames().ratechangeEvent);
        sEventNames.get().append(eventNames().timeupdateEvent);
        sEventNames.get().append(eventNames().addtrackEvent);
        sEventNames.get().append(eventNames().removetrackEvent);
        sEventNames.get().append(eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent);
    }
    return sEventNames.get();
}

const AtomicString&  WebPlaybackSessionModelMediaElement::eventNameAll()
{
    static NeverDestroyed<AtomicString> sEventNameAll = "allEvents";
    return sEventNameAll;
}

#endif
