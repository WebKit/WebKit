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

#import "AudioTrackList.h"
#import "DocumentPage.h"
#import "Event.h"
#import "EventListener.h"
#import "EventNames.h"
#import "ExceptionOr.h"
#import "HTMLVideoElement.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "MediaSelectionOption.h"
#import "NodeDocument.h"
#import "PageGroup.h"
#import "TextTrackList.h"
#import "TimeRanges.h"
#import "UserGestureIndicator.h"
#import "VideoTrack.h"
#import "VideoTrackConfiguration.h"
#import "VideoTrackList.h"
#import <QuartzCore/CoreAnimation.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionModelMediaElement);

PlaybackSessionModelMediaElement::PlaybackSessionModelMediaElement()
    : EventListener(EventListener::CPPEventListenerType)
    , m_soundStageSize { AudioSessionSoundStageSize::Automatic }
    , m_videoTrackConfigurationObserver { Observer<void()>::create([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->videoTrackConfigurationChanged();
    }) }
{
}

PlaybackSessionModelMediaElement::~PlaybackSessionModelMediaElement()
{
}

void PlaybackSessionModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    RefPtr oldMediaElement = m_mediaElement;
    RefPtr newMediaElement = mediaElement;

    if (oldMediaElement == newMediaElement) {
        if (oldMediaElement) {
            for (auto& client : m_clients)
                client->isPictureInPictureSupportedChanged(isPictureInPictureSupported());
        }
        return;
    }

    auto& events = eventNames();

    if (oldMediaElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            oldMediaElement->removeEventListener(eventName, *this, { .capture = false });
        if (auto* audioTracks = oldMediaElement->audioTracks()) {
            audioTracks->removeEventListener(events.addtrackEvent, *this, { .capture = false });
            audioTracks->removeEventListener(events.changeEvent, *this, { .capture = false });
            audioTracks->removeEventListener(events.removetrackEvent, *this, { .capture = false });
        }

        if (auto* videoTracks = oldMediaElement->videoTracks()) {
            videoTracks->removeEventListener(events.addtrackEvent, *this, { .capture = false });
            videoTracks->removeEventListener(events.changeEvent, *this, { .capture = false });
            videoTracks->removeEventListener(events.removetrackEvent, *this, { .capture = false });
        }

        if (auto* textTracks = oldMediaElement->textTracks()) {
            textTracks->removeEventListener(events.addtrackEvent, *this, { .capture = false });
            textTracks->removeEventListener(events.changeEvent, *this, { .capture = false });
            textTracks->removeEventListener(events.removetrackEvent, *this, { .capture = false });
        }
    }
    m_isListening = false;

    if (oldMediaElement)
        oldMediaElement->resetPlaybackSessionState();

    m_mediaElement = newMediaElement;

    if (newMediaElement) {
        for (auto& eventName : observedEventNames())
            newMediaElement->addEventListener(eventName, *this);

        Ref audioTracks = newMediaElement->ensureAudioTracks();
        audioTracks->addEventListener(events.addtrackEvent, *this);
        audioTracks->addEventListener(events.changeEvent, *this);
        audioTracks->addEventListener(events.removetrackEvent, *this);

        Ref videoTracks = newMediaElement->ensureVideoTracks();
        videoTracks->addEventListener(events.addtrackEvent, *this);
        videoTracks->addEventListener(events.changeEvent, *this);
        videoTracks->addEventListener(events.removetrackEvent, *this);

        Ref textTracks = newMediaElement->ensureTextTracks();
        textTracks->addEventListener(events.addtrackEvent, *this);
        textTracks->addEventListener(events.changeEvent, *this);
        textTracks->addEventListener(events.removetrackEvent, *this);
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

void PlaybackSessionModelMediaElement::updateAll()
{
    updateForEventName(eventNameAll());
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
        || eventName == eventNames().canplayEvent)
        updateRate();

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
        auto seekableTimeRangesLastModifiedTime = this->seekableTimeRangesLastModifiedTime();
        auto liveUpdateInterval = this->liveUpdateInterval();
        for (auto& client : m_clients) {
            client->bufferedTimeChanged(bufferedTime);
            client->seekableRangesChanged(seekableRanges(), seekableTimeRangesLastModifiedTime, liveUpdateInterval);
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
        bool isInWindowFullscreenActive = this->isInWindowFullscreenActive();

        for (auto& client : m_clients) {
            client->pictureInPictureActiveChanged(isPictureInPictureActive);
            client->isInWindowFullscreenActiveChanged(isInWindowFullscreenActive);
        }
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

void PlaybackSessionModelMediaElement::videoTrackConfigurationChanged()
{
    maybeUpdateVideoMetadata();
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
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->play();

    // If the media element is already playing, it will not result in a "play"
    // event, so explicitly send a rateChanged() notification, to update the
    // cached playback state:
    updateRate();
}

void PlaybackSessionModelMediaElement::pause()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->pause();

    // If the media element is already paused, it will not result in a "pause"
    // event, so explicitly send a rateChanged() notification, to update the
    // cached playback state:
    updateRate();
}

void PlaybackSessionModelMediaElement::togglePlayState()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->togglePlayState();
}

void PlaybackSessionModelMediaElement::beginScrubbing()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->beginScrubbing();
}

void PlaybackSessionModelMediaElement::endScrubbing()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->endScrubbing();
}

void PlaybackSessionModelMediaElement::seekToTime(double time, double toleranceBefore, double toleranceAfter)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setCurrentTimeWithTolerance(time, toleranceBefore, toleranceAfter);
}

void PlaybackSessionModelMediaElement::fastSeek(double time)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->fastSeek(time);
}

void PlaybackSessionModelMediaElement::beginScanningForward()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->beginScanning(MediaControllerInterface::Forward);
}

void PlaybackSessionModelMediaElement::beginScanningBackward()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->beginScanning(MediaControllerInterface::Backward);
}

void PlaybackSessionModelMediaElement::endScanning()
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->endScanning();
}

void PlaybackSessionModelMediaElement::setDefaultPlaybackRate(double defaultPlaybackRate)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setDefaultPlaybackRate(defaultPlaybackRate);
}

void PlaybackSessionModelMediaElement::setPlaybackRate(double playbackRate)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setPlaybackRate(playbackRate);
}

void PlaybackSessionModelMediaElement::selectAudioMediaOption(uint64_t selectedAudioIndex)
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return;

    for (size_t i = 0, size = m_audioTracksForMenu.size(); i < size; ++i)
        m_audioTracksForMenu[i]->setEnabled(i == selectedAudioIndex);
}

void PlaybackSessionModelMediaElement::selectLegibleMediaOption(uint64_t index)
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return;

    RefPtr<TextTrack> textTrack;
    if (index < m_legibleTracksForMenu.size())
        textTrack = m_legibleTracksForMenu[static_cast<size_t>(index)].copyRef();
    else
        textTrack = TextTrack::captionMenuOffItemSingleton();

    mediaElement->setSelectedTextTrack(textTrack.get());
}

void PlaybackSessionModelMediaElement::togglePictureInPicture()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    if (element->fullscreenMode() == MediaPlayerEnums::VideoFullscreenModePictureInPicture)
        element->setPresentationMode(HTMLVideoElement::VideoPresentationMode::Inline);
    else
        element->setPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
#endif
}

void PlaybackSessionModelMediaElement::enterInWindowFullscreen()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    UserGestureIndicator indicator { IsProcessingUserGesture::Yes, &element->document() };

    if (element->fullscreenMode() != MediaPlayerEnums::VideoFullscreenModeInWindow)
        element->setPresentationModeIgnoringPermissionsPolicy(HTMLVideoElement::VideoPresentationMode::InWindow);
#endif
}

void PlaybackSessionModelMediaElement::exitInWindowFullscreen()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    UserGestureIndicator indicator { IsProcessingUserGesture::Yes, &element->document() };

    if (element->fullscreenMode() == MediaPlayerEnums::VideoFullscreenModeInWindow)
        element->setPresentationMode(HTMLVideoElement::VideoPresentationMode::Inline);
#endif
}

void PlaybackSessionModelMediaElement::enterFullscreen()
{
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    UserGestureIndicator indicator { IsProcessingUserGesture::Yes, &element->document() };
    element->enterFullscreenIgnoringPermissionsPolicy();
}

void PlaybackSessionModelMediaElement::setPlayerIdentifierForVideoElement()
{
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    element->setPlayerIdentifierForVideoElement();
}

void PlaybackSessionModelMediaElement::exitFullscreen()
{
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_mediaElement);
    ASSERT(element);
    if (!element)
        return;

    UserGestureIndicator indicator { IsProcessingUserGesture::Yes, &element->document() };
    element->webkitExitFullscreen();
}

void PlaybackSessionModelMediaElement::toggleMuted()
{
    setMuted(!isMuted());
}

void PlaybackSessionModelMediaElement::setMuted(bool muted)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setMuted(muted);
}

void PlaybackSessionModelMediaElement::setVolume(double volume)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setVolume(volume);
}

void PlaybackSessionModelMediaElement::setPlayingOnSecondScreen(bool value)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setPlayingOnSecondScreen(value);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
String PlaybackSessionModelMediaElement::spatialTrackingLabel() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->spatialTrackingLabel();
    return emptyString();
}

void PlaybackSessionModelMediaElement::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->setSpatialTrackingLabel(spatialTrackingLabel);
}
#endif

void PlaybackSessionModelMediaElement::sendRemoteCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    if (RefPtr mediaElement = m_mediaElement)
        mediaElement->mediaSession().didReceiveRemoteControlCommand(command, argument);
}

void PlaybackSessionModelMediaElement::updateMediaSelectionOptions()
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return;

    if (!mediaElement->document().page())
        return;

    Ref captionPreferences = mediaElement->document().page()->checkedGroup()->ensureCaptionPreferences();
    auto* textTracks = mediaElement->textTracks();
    if (textTracks && textTracks->length())
        m_legibleTracksForMenu = captionPreferences->sortedTrackListForMenu(textTracks, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
    else
        m_legibleTracksForMenu.clear();

    auto* audioTracks = mediaElement->audioTracks();
    if (audioTracks && audioTracks->length() > 1)
        m_audioTracksForMenu = captionPreferences->sortedTrackListForMenu(audioTracks);
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

    maybeUpdateVideoMetadata();
}

void PlaybackSessionModelMediaElement::maybeUpdateVideoMetadata()
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return;
    RefPtr videoTracks = mediaElement->videoTracks();
    RefPtr selectedItem = videoTracks ? videoTracks->selectedItem() : nullptr;

    // Occasionally, when tearing down an AVAssetTrack in a HLS stream, the tracks
    // exposed to web content are recreated, and a "removetrack" event is fired before
    // the subsequent "addtrack" event is fired. This leads to a brief moment when
    // there is no "selected" video track. In this case, ignore the update and
    // return early.
    if (!selectedItem && m_immersiveVideoMetadata) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "no selected item, but have cached immersive metadata; bailing");
        return;
    }

    auto immersiveVideoMetadata = selectedItem ? selectedItem->configuration().immersiveVideoMetadata() : std::nullopt;
    if (immersiveVideoMetadata != m_immersiveVideoMetadata) {
        m_immersiveVideoMetadata = WTF::move(immersiveVideoMetadata);
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "immersiveVideoMetadata: ", m_immersiveVideoMetadata);
        for (auto& client : m_clients)
            client->immersiveVideoMetadataChanged(immersiveVideoMetadata);
    }
}

void PlaybackSessionModelMediaElement::updateRate()
{
    auto playbackState = this->playbackState();
    double playbackRate =  this->playbackRate();
    double defaultPlaybackRate = this->defaultPlaybackRate();
    for (auto& client : m_clients)
        client->rateChanged(playbackState, playbackRate, defaultPlaybackRate);
}

void PlaybackSessionModelMediaElement::updateMediaSelectionIndices()
{
    auto audioIndex = audioMediaSelectedIndex();
    auto legibleIndex = legibleMediaSelectedIndex();

    for (auto& client : m_clients) {
        client->audioMediaSelectionIndexChanged(audioIndex);
        client->legibleMediaSelectionIndexChanged(legibleIndex);
    }

    maybeUpdateVideoMetadata();
}

double PlaybackSessionModelMediaElement::playbackStartedTime() const
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return 0;

    return mediaElement->playbackStartedTime();
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
        eventNames().webkitpresentationmodechangedEvent,
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
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->duration();
    return 0;
}

double PlaybackSessionModelMediaElement::currentTime() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->currentTime();
    return 0;
}

double PlaybackSessionModelMediaElement::bufferedTime() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->maxBufferedTime();
    return 0;
}

auto PlaybackSessionModelMediaElement::playbackState() const -> OptionSet<PlaybackState>
{
    OptionSet<PlaybackSessionModel::PlaybackState> playbackState;
    if (isPlaying())
        playbackState.add(PlaybackSessionModel::PlaybackState::Playing);
    if (isStalled())
        playbackState.add(PlaybackSessionModel::PlaybackState::Stalled);
    return playbackState;
}

bool PlaybackSessionModelMediaElement::isPlaying() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return !mediaElement->paused();
    return false;
}

bool PlaybackSessionModelMediaElement::isStalled() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->readyState() <= HTMLMediaElement::HAVE_CURRENT_DATA;
    return false;
}

double PlaybackSessionModelMediaElement::defaultPlaybackRate() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->defaultPlaybackRate();
    return 0;
}

double PlaybackSessionModelMediaElement::playbackRate() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->playbackRate();
    return 0;
}

PlatformTimeRanges PlaybackSessionModelMediaElement::seekableRanges() const
{
    if (RefPtr mediaElement = m_mediaElement; mediaElement && mediaElement->supportsSeeking())
        return mediaElement->platformSeekable();
    return { };
}

double PlaybackSessionModelMediaElement::seekableTimeRangesLastModifiedTime() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->seekableTimeRangesLastModifiedTime();
    return 0;
}

double PlaybackSessionModelMediaElement::liveUpdateInterval() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->liveUpdateInterval();
    return 0;
}

bool PlaybackSessionModelMediaElement::canPlayFastReverse() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->minFastReverseRate() < 0.0;
    return false;
}

Vector<MediaSelectionOption> PlaybackSessionModelMediaElement::audioMediaSelectionOptions() const
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement || !mediaElement->document().page())
        return { };

    Ref captionPreferences = mediaElement->document().page()->checkedGroup()->ensureCaptionPreferences();
    return m_audioTracksForMenu.map([&](auto& audioTrack) {
        return captionPreferences->mediaSelectionOptionForTrack(audioTrack.get());
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

    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement || !mediaElement->document().page())
        return { };

    Ref captionPreferences = mediaElement->document().page()->checkedGroup()->ensureCaptionPreferences();
    return m_legibleTracksForMenu.map([&](auto& track) {
        return captionPreferences->mediaSelectionOptionForTrack(track.get());
    });
}

uint64_t PlaybackSessionModelMediaElement::legibleMediaSelectedIndex() const
{
    RefPtr mediaElement = m_mediaElement;
    auto host = mediaElement ? mediaElement->mediaControlsHost() : nullptr;
    if (!host)
        return std::numeric_limits<uint64_t>::max();

    AtomString displayMode = host->captionDisplayMode();

    std::optional<uint64_t> selectedIndex;
    std::optional<uint64_t> offIndex;

    for (size_t index = 0; index < m_legibleTracksForMenu.size(); index++) {
        auto& track = m_legibleTracksForMenu[index];

        if (track.ptr() == &TextTrack::captionMenuOffItemSingleton())
            offIndex = index;

        if (displayMode == MediaControlsHost::automaticKeyword()) {
            if (track.ptr() == &TextTrack::captionMenuAutomaticItemSingleton())
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
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->webkitCurrentPlaybackTargetIsWireless();
    return false;
}

PlaybackSessionModel::ExternalPlaybackTargetType PlaybackSessionModelMediaElement::externalPlaybackTargetType() const
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement || !mediaElement->mediaControlsHost())
        return ExternalPlaybackTargetType::TargetTypeNone;

    switch (mediaElement->mediaControlsHost()->externalDeviceType()) {
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
    if (RefPtr mediaElement = m_mediaElement; mediaElement && mediaElement->mediaControlsHost())
        return mediaElement->mediaControlsHost()->externalDeviceDisplayName();
    return emptyString();
}

bool PlaybackSessionModelMediaElement::wirelessVideoPlaybackDisabled() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->mediaSession().wirelessVideoPlaybackDisabled();
    return false;
}

bool PlaybackSessionModelMediaElement::isMuted() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->muted();
    return false;
}

double PlaybackSessionModelMediaElement::volume() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->volume();
    return 0;
}

bool PlaybackSessionModelMediaElement::isPictureInPictureSupported() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->isVideo();
    return false;
}

bool PlaybackSessionModelMediaElement::isPictureInPictureActive() const
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return false;

    return (mediaElement->fullscreenMode() & HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture;
}

bool PlaybackSessionModelMediaElement::isInWindowFullscreenActive() const
{
    RefPtr mediaElement = m_mediaElement;
    if (!mediaElement)
        return false;

    return (mediaElement->fullscreenMode() & HTMLMediaElementEnums::VideoFullscreenModeInWindow) == HTMLMediaElementEnums::VideoFullscreenModeInWindow;
}

#if !RELEASE_LOG_DISABLED
uint64_t PlaybackSessionModelMediaElement::logIdentifier() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return mediaElement->logIdentifier();
    return 0;
}

const Logger* PlaybackSessionModelMediaElement::loggerPtr() const
{
    if (RefPtr mediaElement = m_mediaElement)
        return &mediaElement->logger();
    return nullptr;
}

WTFLogChannel& PlaybackSessionModelMediaElement::logChannel() const
{
    return LogMedia;
}

#endif

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
