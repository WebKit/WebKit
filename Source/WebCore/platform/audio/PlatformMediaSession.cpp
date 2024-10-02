/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformMediaSession.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NowPlayingInfo.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/MediaTime.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformMediaSession);

String convertEnumerationToString(PlatformMediaSession::State state)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Idle"),
        MAKE_STATIC_STRING_IMPL("Autoplaying"),
        MAKE_STATIC_STRING_IMPL("Playing"),
        MAKE_STATIC_STRING_IMPL("Paused"),
        MAKE_STATIC_STRING_IMPL("Interrupted"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::State::Idle), "PlatformMediaSession::Idle is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::State::Autoplaying) == 1, "PlatformMediaSession::Autoplaying is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::State::Playing) == 2, "PlatformMediaSession::Playing is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::State::Paused) == 3, "PlatformMediaSession::Paused is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::State::Interrupted) == 4, "PlatformMediaSession::Interrupted is not 4 as expected");
    ASSERT(static_cast<size_t>(state) < std::size(values));
    return values[static_cast<size_t>(state)];
}

String convertEnumerationToString(PlatformMediaSession::InterruptionType type)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NoInterruption"),
        MAKE_STATIC_STRING_IMPL("SystemSleep"),
        MAKE_STATIC_STRING_IMPL("EnteringBackground"),
        MAKE_STATIC_STRING_IMPL("SystemInterruption"),
        MAKE_STATIC_STRING_IMPL("SuspendedUnderLock"),
        MAKE_STATIC_STRING_IMPL("InvisibleAutoplay"),
        MAKE_STATIC_STRING_IMPL("ProcessInactive"),
        MAKE_STATIC_STRING_IMPL("PlaybackSuspended"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::InterruptionType::NoInterruption), "PlatformMediaSession::NoInterruption is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::SystemSleep) == 1, "PlatformMediaSession::SystemSleep is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::EnteringBackground) == 2, "PlatformMediaSession::EnteringBackground is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::SystemInterruption) == 3, "PlatformMediaSession::SystemInterruption is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::SuspendedUnderLock) == 4, "PlatformMediaSession::SuspendedUnderLock is not 4 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::InvisibleAutoplay) == 5, "PlatformMediaSession::InvisibleAutoplay is not 5 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::ProcessInactive) == 6, "PlatformMediaSession::ProcessInactive is not 6 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InterruptionType::PlaybackSuspended) == 7, "PlatformMediaSession::PlaybackSuspended is not 7 as expected");
    ASSERT(static_cast<size_t>(type) < std::size(values));
    return values[static_cast<size_t>(type)];
}

String convertEnumerationToString(PlatformMediaSession::MediaType mediaType)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("Video"),
        MAKE_STATIC_STRING_IMPL("VideoAudio"),
        MAKE_STATIC_STRING_IMPL("Audio"),
        MAKE_STATIC_STRING_IMPL("WebAudio"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::MediaType::None), "PlatformMediaSession::MediaType::None is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::MediaType::Video) == 1, "PlatformMediaSession::MediaType::Video is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::MediaType::VideoAudio) == 2, "PlatformMediaSession::MediaType::VideoAudio is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::MediaType::Audio) == 3, "PlatformMediaSession::MediaType::Audio is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::MediaType::WebAudio) == 4, "PlatformMediaSession::MediaType::WebAudio is not 4 as expected");

    ASSERT(static_cast<size_t>(mediaType) < std::size(values));
    return values[static_cast<size_t>(mediaType)];
}

String convertEnumerationToString(PlatformMediaSession::RemoteControlCommandType command)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NoCommand"),
        MAKE_STATIC_STRING_IMPL("PlayCommand"),
        MAKE_STATIC_STRING_IMPL("PauseCommand"),
        MAKE_STATIC_STRING_IMPL("StopCommand"),
        MAKE_STATIC_STRING_IMPL("TogglePlayPauseCommand"),
        MAKE_STATIC_STRING_IMPL("BeginSeekingBackwardCommand"),
        MAKE_STATIC_STRING_IMPL("EndSeekingBackwardCommand"),
        MAKE_STATIC_STRING_IMPL("BeginSeekingForwardCommand"),
        MAKE_STATIC_STRING_IMPL("EndSeekingForwardCommand"),
        MAKE_STATIC_STRING_IMPL("SeekToPlaybackPositionCommand"),
        MAKE_STATIC_STRING_IMPL("SkipForwardCommand"),
        MAKE_STATIC_STRING_IMPL("SkipBackwardCommand"),
        MAKE_STATIC_STRING_IMPL("NextTrackCommand"),
        MAKE_STATIC_STRING_IMPL("PreviousTrackCommand"),
        MAKE_STATIC_STRING_IMPL("BeginScrubbingCommand"),
        MAKE_STATIC_STRING_IMPL("EndScrubbingCommand"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::NoCommand), "PlatformMediaSession::RemoteControlCommandType::NoCommand is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::PlayCommand) == 1, "PlatformMediaSession::RemoteControlCommandType::PlayCommand is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::PauseCommand) == 2, "PlatformMediaSession::RemoteControlCommandType::PauseCommand is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::StopCommand) == 3, "PlatformMediaSession::RemoteControlCommandType::StopCommand is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand) == 4, "PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand is not 4 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand) == 5, "PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand is not 5 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand) == 6, "PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand is not 6 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand) == 7, "PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand is not 7 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand) == 8, "PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand is not 8 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand) == 9, "PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand is not 9 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand) == 10, "PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand is not 10 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand) == 11, "PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand is not 11 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::NextTrackCommand) == 12, "PlatformMediaSession::RemoteControlCommandType::NextTrackCommand is not 12 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand) == 13, "PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand is not 13 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand) == 14, "PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand is not 14 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand) == 15, "PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand is not 15 as expected");

    ASSERT(static_cast<size_t>(command) < std::size(values));
    return values[static_cast<size_t>(command)];
}

std::unique_ptr<PlatformMediaSession> PlatformMediaSession::create(PlatformMediaSessionManager& manager, PlatformMediaSessionClient& client)
{
    return std::unique_ptr<PlatformMediaSession>(new PlatformMediaSession(manager, client));
}

PlatformMediaSession::PlatformMediaSession(PlatformMediaSessionManager&, PlatformMediaSessionClient& client)
    : m_client(client)
    , m_mediaSessionIdentifier(MediaSessionIdentifier::generate())
{
}

PlatformMediaSession::~PlatformMediaSession()
{
    setActive(false);
}

void PlatformMediaSession::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    if (m_active)
        PlatformMediaSessionManager::sharedManager().addSession(*this);
    else
        PlatformMediaSessionManager::sharedManager().removeSession(*this);
}

void PlatformMediaSession::setState(State state)
{
    if (state == m_state)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_state = state;
    if (m_state == State::Playing && canProduceAudio())
        m_hasPlayedAudiblySinceLastInterruption = true;
    PlatformMediaSessionManager::sharedManager().sessionStateChanged(*this);
}

size_t PlatformMediaSession::activeInterruptionCount() const
{
    size_t count = 0;
    for (auto& interruption : m_interruptionStack) {
        if (!interruption.ignored)
            count++;
    }
    return count;
}

PlatformMediaSession::InterruptionType PlatformMediaSession::interruptionType() const
{
    if (!m_interruptionStack.size())
        return InterruptionType::NoInterruption;

    return m_interruptionStack.last().type;
}

void PlatformMediaSession::beginInterruption(InterruptionType type)
{
    ASSERT(type != InterruptionType::NoInterruption);

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state, ", interruption count = ", m_interruptionStack.size(), ", type = ", type);

    if (activeInterruptionCount()) {
        m_interruptionStack.append({ type, true });
        return;
    }
    if (client().shouldOverrideBackgroundPlaybackRestriction(type)) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning early because client says to override interruption");
        m_interruptionStack.append({ type, true });
        return;
    }
    m_interruptionStack.append({ type, false });

    m_stateToRestore = state();
    m_notifyingClient = true;
    setState(State::Interrupted);
    client().suspendPlayback();
    m_notifyingClient = false;
}

void PlatformMediaSession::endInterruption(OptionSet<EndInterruptionFlags> flags)
{
    if (m_interruptionStack.isEmpty()) {
        ALWAYS_LOG(LOGIDENTIFIER, "!! ignoring spurious interruption end !!");
        return;
    }

    auto interruption = m_interruptionStack.takeLast();
    ALWAYS_LOG(LOGIDENTIFIER, "flags = ", (int)flags.toRaw(), ", interruption count = ", m_interruptionStack.size(), " type = ", interruptionType());

    if (activeInterruptionCount() || interruption.ignored)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "restoring state ", m_stateToRestore);

    State stateToRestore = m_stateToRestore;
    m_stateToRestore = State::Idle;
    setState(stateToRestore);

    if (stateToRestore == State::Autoplaying)
        client().resumeAutoplaying();

    bool shouldResume = flags.contains(EndInterruptionFlags::MayResumePlaying) && stateToRestore == State::Playing;
    client().mayResumePlayback(shouldResume);
}

void PlatformMediaSession::clientWillBeginAutoplaying()
{
    if (m_notifyingClient)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);
    if (state() == State::Interrupted) {
        m_stateToRestore = State::Autoplaying;
        ALWAYS_LOG(LOGIDENTIFIER, "      setting stateToRestore to \"Autoplaying\"");
        return;
    }

    setState(State::Autoplaying);
}

bool PlatformMediaSession::clientWillBeginPlayback()
{
    if (m_notifyingClient)
        return true;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);

    SetForScope preparingToPlay(m_preparingToPlay, true);

    if (!PlatformMediaSessionManager::sharedManager().sessionWillBeginPlayback(*this)) {
        if (state() == State::Interrupted)
            m_stateToRestore = State::Playing;
        return false;
    }

    m_stateToRestore = State::Playing;
    setState(State::Playing);
    return true;
}

bool PlatformMediaSession::processClientWillPausePlayback(DelayCallingUpdateNowPlaying shouldDelayCallingUpdateNowPlaying)
{
    if (m_notifyingClient)
        return true;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);
    if (state() == State::Interrupted) {
        m_stateToRestore = State::Paused;
        ALWAYS_LOG(LOGIDENTIFIER, "      setting stateToRestore to \"Paused\"");
        return false;
    }

    setState(State::Paused);
    PlatformMediaSessionManager::sharedManager().sessionWillEndPlayback(*this, shouldDelayCallingUpdateNowPlaying);
    return true;
}

bool PlatformMediaSession::clientWillPausePlayback()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    return processClientWillPausePlayback(DelayCallingUpdateNowPlaying::No);
}

void PlatformMediaSession::clientWillBeDOMSuspended()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    processClientWillPausePlayback(DelayCallingUpdateNowPlaying::Yes);
}

void PlatformMediaSession::pauseSession()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (state() == State::Interrupted)
        m_stateToRestore = State::Paused;

    m_client.suspendPlayback();
}

void PlatformMediaSession::stopSession()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_client.suspendPlayback();
    PlatformMediaSessionManager::sharedManager().removeSession(*this);
}

PlatformMediaSession::MediaType PlatformMediaSession::mediaType() const
{
    return m_client.mediaType();
}

PlatformMediaSession::MediaType PlatformMediaSession::presentationType() const
{
    return m_client.presentationType();
}

bool PlatformMediaSession::canReceiveRemoteControlCommands() const
{
    return m_client.canReceiveRemoteControlCommands();
}

void PlatformMediaSession::didReceiveRemoteControlCommand(RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    ALWAYS_LOG(LOGIDENTIFIER, command);

    m_client.didReceiveRemoteControlCommand(command, argument);
}

bool PlatformMediaSession::supportsSeeking() const
{
    return m_client.supportsSeeking();
}

bool PlatformMediaSession::isSuspended() const
{
    return m_client.isSuspended();
}

bool PlatformMediaSession::isPlaying() const
{
    return m_client.isPlaying();
}

bool PlatformMediaSession::isAudible() const
{
    return m_client.isAudible();
}

bool PlatformMediaSession::isEnded() const
{
    return m_client.isEnded();
}

MediaTime PlatformMediaSession::duration() const
{
    return m_client.mediaSessionDuration();
}

bool PlatformMediaSession::shouldOverrideBackgroundLoadingRestriction() const
{
    return m_client.shouldOverrideBackgroundLoadingRestriction();
}

void PlatformMediaSession::isPlayingToWirelessPlaybackTargetChanged(bool isWireless)
{
    if (isWireless == m_isPlayingToWirelessPlaybackTarget)
        return;

    m_isPlayingToWirelessPlaybackTarget = isWireless;

    PlatformMediaSessionManager::sharedManager().sessionIsPlayingToWirelessPlaybackTargetChanged(*this);
}

PlatformMediaSession::DisplayType PlatformMediaSession::displayType() const
{
    return m_client.displayType();
}

bool PlatformMediaSession::blockedBySystemInterruption() const
{
    return activeInterruptionCount() && interruptionType() == PlatformMediaSession::InterruptionType::SystemInterruption;
}

bool PlatformMediaSession::activeAudioSessionRequired() const
{
    if (mediaType() == PlatformMediaSession::MediaType::None)
        return false;
    if (state() != PlatformMediaSession::State::Playing)
        return false;
    return canProduceAudio();
}

bool PlatformMediaSession::canProduceAudio() const
{
    return m_client.canProduceAudio();
}

bool PlatformMediaSession::hasMediaStreamSource() const
{
    return m_client.hasMediaStreamSource();
}

void PlatformMediaSession::canProduceAudioChanged()
{
    PlatformMediaSessionManager::sharedManager().sessionCanProduceAudioChanged();
}

void PlatformMediaSession::clientCharacteristicsChanged(bool positionChanged)
{
    PlatformMediaSessionManager::sharedManager().clientCharacteristicsChanged(*this, positionChanged);
}

static inline bool isPlayingAudio(PlatformMediaSession::MediaType mediaType)
{
#if ENABLE(VIDEO)
    return mediaType == MediaElementSession::MediaType::VideoAudio || mediaType == MediaElementSession::MediaType::Audio;
#else
    UNUSED_PARAM(mediaType);
    return false;
#endif
}

bool PlatformMediaSession::canPlayConcurrently(const PlatformMediaSession& otherSession) const
{
    auto mediaType = this->mediaType();
    auto otherMediaType = otherSession.mediaType();
    if (otherMediaType != mediaType && (!isPlayingAudio(mediaType) || !isPlayingAudio(otherMediaType)))
        return true;

    auto groupID = client().mediaSessionGroupIdentifier();
    auto otherGroupID = otherSession.client().mediaSessionGroupIdentifier();
    if (!groupID || !otherGroupID || groupID != otherGroupID)
        return false;

    return m_client.hasMediaStreamSource() || otherSession.m_client.hasMediaStreamSource();
}

bool PlatformMediaSession::shouldOverridePauseDuringRouteChange() const
{
    return m_client.shouldOverridePauseDuringRouteChange();
}

std::optional<NowPlayingInfo> PlatformMediaSession::nowPlayingInfo() const
{
    return client().nowPlayingInfo();
}

bool PlatformMediaSession::isNowPlayingEligible() const
{
    return client().isNowPlayingEligible();
};

WeakPtr<PlatformMediaSession> PlatformMediaSession::selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSession>>& sessions, PlaybackControlsPurpose purpose)
{
    return client().selectBestMediaSession(sessions, purpose);
}

void PlatformMediaSession::setActiveNowPlayingSession(bool isActiveNowPlayingSession)
{
    if (isActiveNowPlayingSession == m_isActiveNowPlayingSession)
        return;

    m_isActiveNowPlayingSession = isActiveNowPlayingSession;
    client().isActiveNowPlayingSessionChanged();
}

#if !RELEASE_LOG_DISABLED
const Logger& PlatformMediaSession::logger() const
{
    return client().logger();
}

uint64_t PlatformMediaSession::logIdentifier() const
{
    return client().logIdentifier();
}

WTFLogChannel& PlatformMediaSession::logChannel() const
{
    return LogMedia;
}

String PlatformMediaSession::description() const
{
    return makeString(convertEnumerationToString(mediaType()), ", "_s, convertEnumerationToString(state()));
}
#endif

MediaTime PlatformMediaSessionClient::mediaSessionDuration() const
{
    return MediaTime::invalidTime();
}

std::optional<NowPlayingInfo> PlatformMediaSessionClient::nowPlayingInfo() const
{
    return { };
}

} // namespace WebCore

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
