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
#import <WebCore/AudioTrackList.h>
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

    if (!m_playbackSessionInterface)
        return;

    m_playbackSessionInterface->resetMediaState();
    m_playbackSessionInterface->setDuration(duration());
    m_playbackSessionInterface->setCurrentTime(currentTime(), [[NSProcessInfo processInfo] systemUptime]);
    m_playbackSessionInterface->setBufferedTime(bufferedTime());
    m_playbackSessionInterface->setRate(isPlaying(), playbackRate());
    m_playbackSessionInterface->setSeekableRanges(seekableRanges());
    m_playbackSessionInterface->setCanPlayFastReverse(canPlayFastReverse());
    m_playbackSessionInterface->setWirelessVideoPlaybackDisabled(wirelessVideoPlaybackDisabled());
    updateLegibleOptions();
}

void WebPlaybackSessionModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (m_mediaElement == mediaElement)
        return;

    if (m_playbackSessionInterface)
        m_playbackSessionInterface->resetMediaState();

    if (m_mediaElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->removeEventListener(eventName, *this, false);
    }
    m_isListening = false;

    m_mediaElement = mediaElement;

    if (!m_mediaElement)
        return;

    for (auto& eventName : observedEventNames())
        m_mediaElement->addEventListener(eventName, *this, false);
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
        m_playbackSessionInterface->setSeekableRanges(m_mediaElement->seekable());
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
            auto type = m_mediaElement->mediaControlsHost()->externalDeviceType();
            if (type == MediaControlsHost::DeviceType::Airplay)
                targetType = WebPlaybackSessionInterface::TargetTypeAirPlay;
            else if (type == MediaControlsHost::DeviceType::Tvout)
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
    if (!m_mediaElement)
        return;

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
    if (!m_mediaElement)
        return;

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
    if (!m_mediaElement)
        return;

    if (!m_mediaElement->document().page())
        return;

    auto& captionPreferences = m_mediaElement->document().page()->group().captionPreferences();
    auto& textTracks = m_mediaElement->textTracks();
    if (textTracks.length())
        m_legibleTracksForMenu = captionPreferences.sortedTrackListForMenu(&textTracks);
    else
        m_legibleTracksForMenu.clear();

    auto& audioTracks = m_mediaElement->audioTracks();
    if (audioTracks.length() > 1)
        m_audioTracksForMenu = captionPreferences.sortedTrackListForMenu(&audioTracks);
    else
        m_audioTracksForMenu.clear();

    m_playbackSessionInterface->setAudioMediaSelectionOptions(audioMediaSelectionOptions(), audioMediaSelectedIndex());
    m_playbackSessionInterface->setLegibleMediaSelectionOptions(legibleMediaSelectionOptions(), legibleMediaSelectedIndex());
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

double WebPlaybackSessionModelMediaElement::duration() const
{
    return m_mediaElement ? m_mediaElement->duration() : 0;
}

double WebPlaybackSessionModelMediaElement::currentTime() const
{
    return m_mediaElement ? m_mediaElement->currentTime() : 0;
}

double WebPlaybackSessionModelMediaElement::bufferedTime() const
{
    return m_mediaElement ? m_mediaElement->maxBufferedTime() : 0;
}

bool WebPlaybackSessionModelMediaElement::isPlaying() const
{
    return m_mediaElement ? !m_mediaElement->paused() : false;
}

float WebPlaybackSessionModelMediaElement::playbackRate() const
{
    return m_mediaElement ? m_mediaElement->playbackRate() : 0;
}

Ref<TimeRanges> WebPlaybackSessionModelMediaElement::seekableRanges() const
{
    return m_mediaElement ? m_mediaElement->seekable() : TimeRanges::create();
}

bool WebPlaybackSessionModelMediaElement::canPlayFastReverse() const
{
    return m_mediaElement ? m_mediaElement->minFastReverseRate() < 0.0 : false;
}

Vector<WTF::String> WebPlaybackSessionModelMediaElement::audioMediaSelectionOptions() const
{
    Vector<String> audioTrackDisplayNames;

    if (!m_mediaElement || !m_mediaElement->document().page())
        return audioTrackDisplayNames;

    auto& captionPreferences = m_mediaElement->document().page()->group().captionPreferences();

    for (auto& audioTrack : m_audioTracksForMenu)
        audioTrackDisplayNames.append(captionPreferences.displayNameForTrack(audioTrack.get()));

    return audioTrackDisplayNames;
}

uint64_t WebPlaybackSessionModelMediaElement::audioMediaSelectedIndex() const
{
    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        if (m_audioTracksForMenu[index]->enabled())
            return index;
    }
    return 0;
}

Vector<WTF::String> WebPlaybackSessionModelMediaElement::legibleMediaSelectionOptions() const
{
    Vector<String> trackDisplayNames;

    if (!m_mediaElement || !m_mediaElement->document().page())
        return trackDisplayNames;

    auto& captionPreferences = m_mediaElement->document().page()->group().captionPreferences();

    for (auto& track : m_legibleTracksForMenu)
        trackDisplayNames.append(captionPreferences.displayNameForTrack(track.get()));

    return trackDisplayNames;
}

uint64_t WebPlaybackSessionModelMediaElement::legibleMediaSelectedIndex() const
{
    uint64_t selectedIndex = 0;
    uint64_t offIndex = 0;
    bool trackMenuItemSelected = false;

    if (!m_mediaElement || !m_mediaElement->mediaControlsHost())
        return selectedIndex;

    AtomicString displayMode = m_mediaElement->mediaControlsHost()->captionDisplayMode();
    TextTrack* offItem = m_mediaElement->mediaControlsHost()->captionMenuOffItem();
    TextTrack* automaticItem = m_mediaElement->mediaControlsHost()->captionMenuAutomaticItem();

    for (size_t index = 0; index < m_legibleTracksForMenu.size(); index++) {
        auto& track = m_legibleTracksForMenu[index];
        if (track == offItem)
            offIndex = index;

        if (track == automaticItem && displayMode == MediaControlsHost::automaticKeyword()) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }

        if (displayMode != MediaControlsHost::automaticKeyword() && track->mode() == TextTrack::Mode::Showing) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }
    }

    if (offIndex && !trackMenuItemSelected && displayMode == MediaControlsHost::forcedOnlyKeyword())
        selectedIndex = offIndex;

    return selectedIndex;
}

bool WebPlaybackSessionModelMediaElement::externalPlaybackEnabled() const
{
    return m_mediaElement ? m_mediaElement->webkitCurrentPlaybackTargetIsWireless() : false;
}

bool WebPlaybackSessionModelMediaElement::wirelessVideoPlaybackDisabled() const
{
    return m_mediaElement ? m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled(*m_mediaElement) : false;
}

#endif
