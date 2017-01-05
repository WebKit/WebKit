/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#import "AudioTrackList.h"
#import "Event.h"
#import "EventListener.h"
#import "EventNames.h"
#import "HTMLElement.h"
#import "HTMLMediaElement.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "Page.h"
#import "PageGroup.h"
#import "SoftLinking.h"
#import "TextTrackList.h"
#import "TimeRanges.h"
#import "WebVideoFullscreenInterface.h"
#import <QuartzCore/CoreAnimation.h>
#import <wtf/NeverDestroyed.h>

namespace WebCore {

WebPlaybackSessionModelMediaElement::WebPlaybackSessionModelMediaElement()
    : EventListener(EventListener::CPPEventListenerType)
{
}

WebPlaybackSessionModelMediaElement::~WebPlaybackSessionModelMediaElement()
{
}

void WebPlaybackSessionModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (m_mediaElement == mediaElement)
        return;

    if (m_mediaElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->removeEventListener(eventName, *this, false);
    }
    m_isListening = false;

    if (m_mediaElement)
        m_mediaElement->resetPlaybackSessionState();

    m_mediaElement = mediaElement;

    if (m_mediaElement) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->addEventListener(eventName, *this, false);
        m_isListening = true;
    }

    updateForEventName(eventNameAll());
}

void WebPlaybackSessionModelMediaElement::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event* event)
{
    updateForEventName(event->type());
}

void WebPlaybackSessionModelMediaElement::updateForEventName(const WTF::AtomicString& eventName)
{
    if (m_clients.isEmpty())
        return;

    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().durationchangeEvent) {
        double duration = this->duration();
        for (auto client : m_clients)
            client->durationChanged(duration);
        // These is no standard event for minFastReverseRateChange; duration change is a reasonable proxy for it.
        // It happens every time a new item becomes ready to play.
        bool canPlayFastReverse = this->canPlayFastReverse();
        for (auto client : m_clients)
            client->canPlayFastReverseChanged(canPlayFastReverse);
    }

    if (all
        || eventName == eventNames().playEvent
        || eventName == eventNames().playingEvent) {
        for (auto client : m_clients)
            client->playbackStartedTimeChanged(playbackStartedTime());
    }

    if (all
        || eventName == eventNames().pauseEvent
        || eventName == eventNames().playEvent
        || eventName == eventNames().ratechangeEvent) {
        bool isPlaying = this->isPlaying();
        float playbackRate = this->playbackRate();
        for (auto client : m_clients)
            client->rateChanged(isPlaying, playbackRate);
    }

    if (all
        || eventName == eventNames().timeupdateEvent) {
        auto currentTime = this->currentTime();
        auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
        auto bufferedTime = this->bufferedTime();
        auto seekableRanges = this->seekableRanges();

        for (auto client : m_clients) {
            client->currentTimeChanged(currentTime, anchorTime);
            client->bufferedTimeChanged(bufferedTime);
            // FIXME: 130788 - find a better event from which to update seekable ranges.
            client->seekableRangesChanged(seekableRanges);
        }
    }

    if (all
        || eventName == eventNames().addtrackEvent
        || eventName == eventNames().removetrackEvent)
        updateLegibleOptions();

    if (all
        || eventName == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        bool enabled = externalPlaybackEnabled();
        ExternalPlaybackTargetType targetType = externalPlaybackTargetType();
        String localizedDeviceName = externalPlaybackLocalizedDeviceName();

        bool wirelessVideoPlaybackDisabled = this->wirelessVideoPlaybackDisabled();

        for (auto client : m_clients) {
            client->externalPlaybackChanged(enabled, targetType, localizedDeviceName);
            client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
        }
    }
}
void WebPlaybackSessionModelMediaElement::addClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void WebPlaybackSessionModelMediaElement::removeClient(WebPlaybackSessionModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
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

    for (size_t i = 0, size = m_audioTracksForMenu.size(); i < size; ++i)
        m_audioTracksForMenu[i]->setEnabled(i == selectedAudioIndex);
}

void WebPlaybackSessionModelMediaElement::selectLegibleMediaOption(uint64_t index)
{
    if (!m_mediaElement)
        return;

    TextTrack* textTrack;
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

    auto audioOptions = audioMediaSelectionOptions();
    auto audioIndex = audioMediaSelectedIndex();
    auto legibleOptions = legibleMediaSelectionOptions();
    auto legibleIndex = legibleMediaSelectedIndex();

    for (auto client : m_clients) {
        client->audioMediaSelectionOptionsChanged(audioOptions, audioIndex);
        client->legibleMediaSelectionOptionsChanged(legibleOptions, legibleIndex);
    }
}

double WebPlaybackSessionModelMediaElement::playbackStartedTime() const
{
    if (!m_mediaElement)
        return 0;

    return m_mediaElement->playbackStartedTime();
}

const Vector<AtomicString>& WebPlaybackSessionModelMediaElement::observedEventNames()
{
    // FIXME(157452): Remove the right-hand constructor notation once NeverDestroyed supports initializer_lists.
    static NeverDestroyed<Vector<AtomicString>> names = Vector<AtomicString>({
        eventNames().durationchangeEvent,
        eventNames().pauseEvent,
        eventNames().playEvent,
        eventNames().ratechangeEvent,
        eventNames().timeupdateEvent,
        eventNames().addtrackEvent,
        eventNames().removetrackEvent,
        eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent,
    });
    return names.get();
}

const AtomicString&  WebPlaybackSessionModelMediaElement::eventNameAll()
{
    static NeverDestroyed<AtomicString> eventNameAll("allEvents", AtomicString::ConstructFromLiteral);
    return eventNameAll;
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

Vector<String> WebPlaybackSessionModelMediaElement::audioMediaSelectionOptions() const
{
    Vector<String> audioTrackDisplayNames;

    if (!m_mediaElement || !m_mediaElement->document().page())
        return audioTrackDisplayNames;

    auto& captionPreferences = m_mediaElement->document().page()->group().captionPreferences();

    audioTrackDisplayNames.reserveInitialCapacity(m_audioTracksForMenu.size());
    for (auto& audioTrack : m_audioTracksForMenu)
        audioTrackDisplayNames.uncheckedAppend(captionPreferences.displayNameForTrack(audioTrack.get()));

    return audioTrackDisplayNames;
}

uint64_t WebPlaybackSessionModelMediaElement::audioMediaSelectedIndex() const
{
    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        if (m_audioTracksForMenu[index]->enabled())
            return index;
    }
    return std::numeric_limits<uint64_t>::max();
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
    uint64_t selectedIndex = std::numeric_limits<uint64_t>::max();
    uint64_t offIndex = 0;
    bool trackMenuItemSelected = false;

    auto host = m_mediaElement ? m_mediaElement->mediaControlsHost() : nullptr;

    if (!host)
        return selectedIndex;

    AtomicString displayMode = host->captionDisplayMode();
    TextTrack* offItem = host->captionMenuOffItem();
    TextTrack* automaticItem = host->captionMenuAutomaticItem();

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
    return m_mediaElement && m_mediaElement->webkitCurrentPlaybackTargetIsWireless();
}

WebPlaybackSessionModel::ExternalPlaybackTargetType WebPlaybackSessionModelMediaElement::externalPlaybackTargetType() const
{
    if (!m_mediaElement || !m_mediaElement->mediaControlsHost())
        return TargetTypeNone;

    switch (m_mediaElement->mediaControlsHost()->externalDeviceType()) {
    default:
        ASSERT_NOT_REACHED();
        return TargetTypeNone;
    case MediaControlsHost::DeviceType::None:
        return TargetTypeNone;
    case MediaControlsHost::DeviceType::Airplay:
        return TargetTypeAirPlay;
    case MediaControlsHost::DeviceType::Tvout:
        return TargetTypeTVOut;
    }
}

String WebPlaybackSessionModelMediaElement::externalPlaybackLocalizedDeviceName() const
{
    if (m_mediaElement && m_mediaElement->mediaControlsHost())
        return m_mediaElement->mediaControlsHost()->externalDeviceDisplayName();
    return emptyString();
}

bool WebPlaybackSessionModelMediaElement::wirelessVideoPlaybackDisabled() const
{
    return m_mediaElement && m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled(*m_mediaElement);
}

}

#endif
