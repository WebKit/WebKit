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
#import "PlaybackSessionModelMediaElement.h"

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "AddEventListenerOptions.h"
#import "AudioTrackList.h"
#import "Event.h"
#import "EventListener.h"
#import "EventNames.h"
#import "HTMLVideoElement.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "MediaSelectionOption.h"
#import "Page.h"
#import "PageGroup.h"
#import "TextTrackList.h"
#import "TimeRanges.h"
#import <QuartzCore/CoreAnimation.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>

namespace WebCore {

PlaybackSessionModelMediaElement::PlaybackSessionModelMediaElement()
    : EventListener(EventListener::CPPEventListenerType)
{
}

PlaybackSessionModelMediaElement::~PlaybackSessionModelMediaElement()
{
}

void PlaybackSessionModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (m_mediaElement == mediaElement) {
        if (m_mediaElement) {
            for (auto& client : m_clients)
                client->isPictureInPictureSupportedChanged(isPictureInPictureSupported());
        }
        return;
    }

    auto& events = eventNames();

    if (m_mediaElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->removeEventListener(eventName, *this, false);
        if (auto* audioTracks = m_mediaElement->audioTracks()) {
            audioTracks->removeEventListener(events.addtrackEvent, *this, false);
            audioTracks->removeEventListener(events.changeEvent, *this, false);
            audioTracks->removeEventListener(events.removetrackEvent, *this, false);
        }

        if (auto* textTracks = m_mediaElement->audioTracks()) {
            textTracks->removeEventListener(events.addtrackEvent, *this, false);
            textTracks->removeEventListener(events.changeEvent, *this, false);
            textTracks->removeEventListener(events.removetrackEvent, *this, false);
        }
    }
    m_isListening = false;

    if (m_mediaElement)
        m_mediaElement->resetPlaybackSessionState();

    m_mediaElement = mediaElement;

    if (m_mediaElement) {
        for (auto& eventName : observedEventNames())
            m_mediaElement->addEventListener(eventName, *this, false);

        auto& audioTracks = m_mediaElement->ensureAudioTracks();
        audioTracks.addEventListener(events.addtrackEvent, *this, false);
        audioTracks.addEventListener(events.changeEvent, *this, false);
        audioTracks.addEventListener(events.removetrackEvent, *this, false);

        auto& textTracks = m_mediaElement->ensureTextTracks();
        textTracks.addEventListener(events.addtrackEvent, *this, false);
        textTracks.addEventListener(events.changeEvent, *this, false);
        textTracks.addEventListener(events.removetrackEvent, *this, false);
        m_isListening = true;
    }

    updateForEventName(eventNameAll());

    for (auto& client : m_clients)
        client->isPictureInPictureSupportedChanged(isPictureInPictureSupported());
}

void PlaybackSessionModelMediaElement::mediaEngineChanged()
{
    bool wirelessVideoPlaybackDisabled = this->wirelessVideoPlaybackDisabled();
    for (auto& client : m_clients)
        client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
}

void PlaybackSessionModelMediaElement::handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event& event)
{
    updateForEventName(event.type());
}

void PlaybackSessionModelMediaElement::updateForEventName(const WTF::AtomString& eventName)
{
    if (m_clients.isEmpty())
        return;

    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().durationchangeEvent) {
        double duration = this->duration();
        for (auto& client : m_clients)
            client->durationChanged(duration);
        // These is no standard event for minFastReverseRateChange; duration change is a reasonable proxy for it.
        // It happens every time a new item becomes ready to play.
        bool canPlayFastReverse = this->canPlayFastReverse();
        for (auto& client : m_clients)
            client->canPlayFastReverseChanged(canPlayFastReverse);
    }

    if (all
        || eventName == eventNames().playEvent
        || eventName == eventNames().playingEvent) {
        for (auto& client : m_clients)
            client->playbackStartedTimeChanged(playbackStartedTime());
    }

    if (all
        || eventName == eventNames().pauseEvent
        || eventName == eventNames().playEvent
        || eventName == eventNames().ratechangeEvent
        || eventName == eventNames().waitingEvent
        || eventName == eventNames().canplayEvent) {
        OptionSet<PlaybackSessionModel::PlaybackState> playbackState;
        if (isPlaying())
            playbackState.add(PlaybackSessionModel::PlaybackState::Playing);
        if (isStalled())
            playbackState.add(PlaybackSessionModel::PlaybackState::Stalled);

        double playbackRate =  this->playbackRate();
        double defaultPlaybackRate = this->defaultPlaybackRate();
        for (auto& client : m_clients)
            client->rateChanged(playbackState, playbackRate, defaultPlaybackRate);
    }

    if (all
        || eventName == eventNames().timeupdateEvent) {
        auto currentTime = this->currentTime();
        auto anchorTime = [[NSProcessInfo processInfo] systemUptime];
        for (auto& client : m_clients)
            client->currentTimeChanged(currentTime, anchorTime);
    }

    if (all
        || eventName == eventNames().progressEvent) {
        auto bufferedTime = this->bufferedTime();
        auto seekableRanges = this->seekableRanges();
        auto seekableTimeRangesLastModifiedTime = this->seekableTimeRangesLastModifiedTime();
        auto liveUpdateInterval = this->liveUpdateInterval();
        for (auto& client : m_clients) {
            client->bufferedTimeChanged(bufferedTime);
            client->seekableRangesChanged(seekableRanges, seekableTimeRangesLastModifiedTime, liveUpdateInterval);
        }
    }

    if (all
        || eventName == eventNames().addtrackEvent
        || eventName == eventNames().removetrackEvent)
        updateMediaSelectionOptions();

    if (all
        || eventName == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        bool enabled = externalPlaybackEnabled();
        ExternalPlaybackTargetType targetType = externalPlaybackTargetType();
        String localizedDeviceName = externalPlaybackLocalizedDeviceName();

        bool wirelessVideoPlaybackDisabled = this->wirelessVideoPlaybackDisabled();

        for (auto& client : m_clients) {
            client->externalPlaybackChanged(enabled, targetType, localizedDeviceName);
            client->wirelessVideoPlaybackDisabledChanged(wirelessVideoPlaybackDisabled);
        }
    }

    if (all
        || eventName == eventNames().webkitpresentationmodechangedEvent) {
        bool isPictureInPictureActive = this->isPictureInPictureActive();

        for (auto& client : m_clients)
            client->pictureInPictureActiveChanged(isPictureInPictureActive);
    }


    // We don't call updateMediaSelectionIndices() in the all case, since
    // updateMediaSelectionOptions() will also update the selection indices.
    if (eventName == eventNames().changeEvent)
        updateMediaSelectionIndices();

    if (all
        || eventName == eventNames().volumechangeEvent) {
        for (auto& client : m_clients) {
            client->mutedChanged(isMuted());
            client->volumeChanged(volume());
        }
    }
}
void PlaybackSessionModelMediaElement::addClient(PlaybackSessionModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void PlaybackSessionModelMediaElement::removeClient(PlaybackSessionModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void PlaybackSessionModelMediaElement::play()
{
    if (m_mediaElement)
        m_mediaElement->play();
}

void PlaybackSessionModelMediaElement::pause()
{
    if (m_mediaElement)
        m_mediaElement->pause();
}

void PlaybackSessionModelMediaElement::togglePlayState()
{
    if (m_mediaElement)
        m_mediaElement->togglePlayState();
}

void PlaybackSessionModelMediaElement::beginScrubbing()
{
    if (m_mediaElement)
        m_mediaElement->beginScrubbing();
}

void PlaybackSessionModelMediaElement::endScrubbing()
{
    if (m_mediaElement)
        m_mediaElement->endScrubbing();
}

void PlaybackSessionModelMediaElement::seekToTime(double time, double toleranceBefore, double toleranceAfter)
{
    if (m_mediaElement)
        m_mediaElement->setCurrentTimeWithTolerance(time, toleranceBefore, toleranceAfter);
}

void PlaybackSessionModelMediaElement::fastSeek(double time)
{
    if (m_mediaElement)
        m_mediaElement->fastSeek(time);
}

void PlaybackSessionModelMediaElement::beginScanningForward()
{
    if (m_mediaElement)
        m_mediaElement->beginScanning(MediaControllerInterface::Forward);
}

void PlaybackSessionModelMediaElement::beginScanningBackward()
{
    if (m_mediaElement)
        m_mediaElement->beginScanning(MediaControllerInterface::Backward);
}

void PlaybackSessionModelMediaElement::endScanning()
{
    if (m_mediaElement)
        m_mediaElement->endScanning();
}

void PlaybackSessionModelMediaElement::setDefaultPlaybackRate(double defaultPlaybackRate)
{
    if (m_mediaElement)
        m_mediaElement->setDefaultPlaybackRate(defaultPlaybackRate);
}

void PlaybackSessionModelMediaElement::setPlaybackRate(double playbackRate)
{
    if (m_mediaElement)
        m_mediaElement->setPlaybackRate(playbackRate);
}

void PlaybackSessionModelMediaElement::selectAudioMediaOption(uint64_t selectedAudioIndex)
{
    if (!m_mediaElement)
        return;

    for (size_t i = 0, size = m_audioTracksForMenu.size(); i < size; ++i)
        m_audioTracksForMenu[i]->setEnabled(i == selectedAudioIndex);
}

void PlaybackSessionModelMediaElement::selectLegibleMediaOption(uint64_t index)
{
    if (!m_mediaElement)
        return;

    TextTrack* textTrack;
    if (index < m_legibleTracksForMenu.size())
        textTrack = m_legibleTracksForMenu[static_cast<size_t>(index)].get();
    else
        textTrack = &TextTrack::captionMenuOffItem();

    m_mediaElement->setSelectedTextTrack(textTrack);
}

void PlaybackSessionModelMediaElement::togglePictureInPicture()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    ASSERT(is<HTMLVideoElement>(*m_mediaElement));
    if (!is<HTMLVideoElement>(*m_mediaElement))
        return;

    auto& element = downcast<HTMLVideoElement>(*m_mediaElement);
    if (element.fullscreenMode() == MediaPlayerEnums::VideoFullscreenModePictureInPicture)
        element.setPresentationMode(HTMLVideoElement::VideoPresentationMode::Inline);
    else
        element.setPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
#endif
}

void PlaybackSessionModelMediaElement::toggleMuted()
{
    setMuted(!isMuted());
}

void PlaybackSessionModelMediaElement::setMuted(bool muted)
{
    if (m_mediaElement)
        m_mediaElement->setMuted(muted);
}

void PlaybackSessionModelMediaElement::setVolume(double volume)
{
    if (m_mediaElement)
        m_mediaElement->setVolume(volume);
}

void PlaybackSessionModelMediaElement::setPlayingOnSecondScreen(bool value)
{
    if (m_mediaElement)
        m_mediaElement->setPlayingOnSecondScreen(value);
}

void PlaybackSessionModelMediaElement::sendRemoteCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (m_mediaElement)
        m_mediaElement->mediaSession().didReceiveRemoteControlCommand(command, argument);
}

void PlaybackSessionModelMediaElement::updateMediaSelectionOptions()
{
    if (!m_mediaElement)
        return;

    if (!m_mediaElement->document().page())
        return;

    auto& captionPreferences = m_mediaElement->document().page()->group().ensureCaptionPreferences();
    auto* textTracks = m_mediaElement->textTracks();
    if (textTracks && textTracks->length())
        m_legibleTracksForMenu = captionPreferences.sortedTrackListForMenu(textTracks, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
    else
        m_legibleTracksForMenu.clear();

    auto* audioTracks = m_mediaElement->audioTracks();
    if (audioTracks && audioTracks->length() > 1)
        m_audioTracksForMenu = captionPreferences.sortedTrackListForMenu(audioTracks);
    else
        m_audioTracksForMenu.clear();

    auto audioOptions = audioMediaSelectionOptions();
    auto audioIndex = audioMediaSelectedIndex();
    auto legibleOptions = legibleMediaSelectionOptions();
    auto legibleIndex = legibleMediaSelectedIndex();

    for (auto& client : m_clients) {
        client->audioMediaSelectionOptionsChanged(audioOptions, audioIndex);
        client->legibleMediaSelectionOptionsChanged(legibleOptions, legibleIndex);
    }
}

void PlaybackSessionModelMediaElement::updateMediaSelectionIndices()
{
    auto audioIndex = audioMediaSelectedIndex();
    auto legibleIndex = legibleMediaSelectedIndex();

    for (auto& client : m_clients) {
        client->audioMediaSelectionIndexChanged(audioIndex);
        client->legibleMediaSelectionIndexChanged(legibleIndex);
    }
}

double PlaybackSessionModelMediaElement::playbackStartedTime() const
{
    if (!m_mediaElement)
        return 0;

    return m_mediaElement->playbackStartedTime();
}

const Vector<AtomString>& PlaybackSessionModelMediaElement::observedEventNames()
{
    // FIXME(157452): Remove the right-hand constructor notation once NeverDestroyed supports initializer_lists.
    static NeverDestroyed<Vector<AtomString>> names = Vector<AtomString>({
        eventNames().canplayEvent,
        eventNames().durationchangeEvent,
        eventNames().pauseEvent,
        eventNames().playEvent,
        eventNames().ratechangeEvent,
        eventNames().timeupdateEvent,
        eventNames().progressEvent,
        eventNames().volumechangeEvent,
        eventNames().waitingEvent,
        eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent,
    });
    return names.get();
}

const AtomString&  PlaybackSessionModelMediaElement::eventNameAll()
{
    static MainThreadNeverDestroyed<const AtomString> eventNameAll("allEvents"_s);
    return eventNameAll;
}

double PlaybackSessionModelMediaElement::duration() const
{
    return m_mediaElement ? m_mediaElement->duration() : 0;
}

double PlaybackSessionModelMediaElement::currentTime() const
{
    return m_mediaElement ? m_mediaElement->currentTime() : 0;
}

double PlaybackSessionModelMediaElement::bufferedTime() const
{
    return m_mediaElement ? m_mediaElement->maxBufferedTime() : 0;
}

bool PlaybackSessionModelMediaElement::isPlaying() const
{
    return m_mediaElement ? !m_mediaElement->paused() : false;
}

bool PlaybackSessionModelMediaElement::isStalled() const
{
    return m_mediaElement && m_mediaElement->readyState() <= HTMLMediaElement::HAVE_CURRENT_DATA;
}

double PlaybackSessionModelMediaElement::defaultPlaybackRate() const
{
    return m_mediaElement ? m_mediaElement->defaultPlaybackRate() : 0;
}

double PlaybackSessionModelMediaElement::playbackRate() const
{
    return m_mediaElement ? m_mediaElement->playbackRate() : 0;
}

Ref<TimeRanges> PlaybackSessionModelMediaElement::seekableRanges() const
{
    return m_mediaElement && m_mediaElement->supportsSeeking() ? m_mediaElement->seekable() : TimeRanges::create();
}

double PlaybackSessionModelMediaElement::seekableTimeRangesLastModifiedTime() const
{
    return m_mediaElement ? m_mediaElement->seekableTimeRangesLastModifiedTime() : 0;
}

double PlaybackSessionModelMediaElement::liveUpdateInterval() const
{
    return m_mediaElement ? m_mediaElement->liveUpdateInterval() : 0;
}
    
bool PlaybackSessionModelMediaElement::canPlayFastReverse() const
{
    return m_mediaElement ? m_mediaElement->minFastReverseRate() < 0.0 : false;
}

Vector<MediaSelectionOption> PlaybackSessionModelMediaElement::audioMediaSelectionOptions() const
{
    if (!m_mediaElement || !m_mediaElement->document().page())
        return { };

    auto& captionPreferences = m_mediaElement->document().page()->group().ensureCaptionPreferences();
    return m_audioTracksForMenu.map([&](auto& audioTrack) {
        return captionPreferences.mediaSelectionOptionForTrack(audioTrack.get());
    });
}

uint64_t PlaybackSessionModelMediaElement::audioMediaSelectedIndex() const
{
    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        if (m_audioTracksForMenu[index]->enabled())
            return index;
    }
    return std::numeric_limits<uint64_t>::max();
}

Vector<MediaSelectionOption> PlaybackSessionModelMediaElement::legibleMediaSelectionOptions() const
{
    Vector<MediaSelectionOption> legibleOptions;

    if (!m_mediaElement || !m_mediaElement->document().page())
        return { };

    auto& captionPreferences = m_mediaElement->document().page()->group().ensureCaptionPreferences();
    return m_legibleTracksForMenu.map([&](auto& track) {
        return captionPreferences.mediaSelectionOptionForTrack(track.get());
    });
}

uint64_t PlaybackSessionModelMediaElement::legibleMediaSelectedIndex() const
{
    auto host = m_mediaElement ? m_mediaElement->mediaControlsHost() : nullptr;
    if (!host)
        return std::numeric_limits<uint64_t>::max();

    AtomString displayMode = host->captionDisplayMode();
    TextTrack& offItem = TextTrack::captionMenuOffItem();
    TextTrack& automaticItem = TextTrack::captionMenuAutomaticItem();

    std::optional<uint64_t> selectedIndex;
    std::optional<uint64_t> offIndex;

    for (size_t index = 0; index < m_legibleTracksForMenu.size(); index++) {
        auto& track = m_legibleTracksForMenu[index];

        if (track == &offItem)
            offIndex = index;

        if (displayMode == MediaControlsHost::automaticKeyword()) {
            if (track == &automaticItem)
                selectedIndex = index;
        } else {
            if (track->mode() == TextTrack::Mode::Showing)
                selectedIndex = index;
        }
    }

    if (!selectedIndex && displayMode == MediaControlsHost::forcedOnlyKeyword())
        selectedIndex = offIndex;

    return selectedIndex.value_or(std::numeric_limits<uint64_t>::max());
}

bool PlaybackSessionModelMediaElement::externalPlaybackEnabled() const
{
    return m_mediaElement && m_mediaElement->webkitCurrentPlaybackTargetIsWireless();
}

PlaybackSessionModel::ExternalPlaybackTargetType PlaybackSessionModelMediaElement::externalPlaybackTargetType() const
{
    if (!m_mediaElement || !m_mediaElement->mediaControlsHost())
        return ExternalPlaybackTargetType::TargetTypeNone;

    switch (m_mediaElement->mediaControlsHost()->externalDeviceType()) {
    default:
        ASSERT_NOT_REACHED();
        return ExternalPlaybackTargetType::TargetTypeNone;
    case MediaControlsHost::DeviceType::None:
        return ExternalPlaybackTargetType::TargetTypeNone;
    case MediaControlsHost::DeviceType::Airplay:
        return ExternalPlaybackTargetType::TargetTypeAirPlay;
    case MediaControlsHost::DeviceType::Tvout:
        return ExternalPlaybackTargetType::TargetTypeTVOut;
    }
}

String PlaybackSessionModelMediaElement::externalPlaybackLocalizedDeviceName() const
{
    if (m_mediaElement && m_mediaElement->mediaControlsHost())
        return m_mediaElement->mediaControlsHost()->externalDeviceDisplayName();
    return emptyString();
}

bool PlaybackSessionModelMediaElement::wirelessVideoPlaybackDisabled() const
{
    return m_mediaElement && m_mediaElement->mediaSession().wirelessVideoPlaybackDisabled();
}

bool PlaybackSessionModelMediaElement::isMuted() const
{
    return m_mediaElement ? m_mediaElement->muted() : false;
}

double PlaybackSessionModelMediaElement::volume() const
{
    return m_mediaElement ? m_mediaElement->volume() : 0;
}

bool PlaybackSessionModelMediaElement::isPictureInPictureSupported() const
{
    return m_mediaElement ? m_mediaElement->isVideo() : false;
}

bool PlaybackSessionModelMediaElement::isPictureInPictureActive() const
{
    if (!m_mediaElement)
        return false;

    return (m_mediaElement->fullscreenMode() & HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture;
}

#if !RELEASE_LOG_DISABLED
const void* PlaybackSessionModelMediaElement::logIdentifier() const
{
    return m_mediaElement ? m_mediaElement->logIdentifier() : nullptr;
}

const Logger* PlaybackSessionModelMediaElement::loggerPtr() const
{
    return m_mediaElement ? &m_mediaElement->logger() : nullptr;
}
#endif


}

#endif
