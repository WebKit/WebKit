/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

namespace WebCore {

static constexpr Seconds clientDataBufferingTimerThrottleDelay { 100_ms };

#if !RELEASE_LOG_DISABLED
String convertEnumerationToString(PlatformMediaSession::State state)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Idle"),
        MAKE_STATIC_STRING_IMPL("Autoplaying"),
        MAKE_STATIC_STRING_IMPL("Playing"),
        MAKE_STATIC_STRING_IMPL("Paused"),
        MAKE_STATIC_STRING_IMPL("Interrupted"),
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::Idle), "PlatformMediaSession::Idle is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::Autoplaying) == 1, "PlatformMediaSession::Autoplaying is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::Playing) == 2, "PlatformMediaSession::Playing is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::Paused) == 3, "PlatformMediaSession::Paused is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::Interrupted) == 4, "PlatformMediaSession::Interrupted is not 4 as expected");
    ASSERT(static_cast<size_t>(state) < WTF_ARRAY_LENGTH(values));
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
    static_assert(!static_cast<size_t>(PlatformMediaSession::NoInterruption), "PlatformMediaSession::NoInterruption is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::SystemSleep) == 1, "PlatformMediaSession::SystemSleep is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::EnteringBackground) == 2, "PlatformMediaSession::EnteringBackground is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::SystemInterruption) == 3, "PlatformMediaSession::SystemInterruption is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::SuspendedUnderLock) == 4, "PlatformMediaSession::SuspendedUnderLock is not 4 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::InvisibleAutoplay) == 5, "PlatformMediaSession::InvisibleAutoplay is not 5 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::ProcessInactive) == 6, "PlatformMediaSession::ProcessInactive is not 6 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::PlaybackSuspended) == 7, "PlatformMediaSession::PlaybackSuspended is not 7 as expected");
    ASSERT(static_cast<size_t>(type) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(type)];
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
    };
    static_assert(!static_cast<size_t>(PlatformMediaSession::NoCommand), "PlatformMediaSession::NoCommand is not 0 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::PlayCommand) == 1, "PlatformMediaSession::PlayCommand is not 1 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::PauseCommand) == 2, "PlatformMediaSession::PauseCommand is not 2 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::StopCommand) == 3, "PlatformMediaSession::StopCommand is not 3 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::TogglePlayPauseCommand) == 4, "PlatformMediaSession::TogglePlayPauseCommand is not 4 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::BeginSeekingBackwardCommand) == 5, "PlatformMediaSession::BeginSeekingBackwardCommand is not 5 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::EndSeekingBackwardCommand) == 6, "PlatformMediaSession::EndSeekingBackwardCommand is not 6 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::BeginSeekingForwardCommand) == 7, "PlatformMediaSession::BeginSeekingForwardCommand is not 7 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::EndSeekingForwardCommand) == 8, "PlatformMediaSession::EndSeekingForwardCommand is not 8 as expected");
    static_assert(static_cast<size_t>(PlatformMediaSession::SeekToPlaybackPositionCommand) == 9, "PlatformMediaSession::SeekToPlaybackPositionCommand is not 9 as expected");
    ASSERT(static_cast<size_t>(command) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(command)];
}

#endif

std::unique_ptr<PlatformMediaSession> PlatformMediaSession::create(PlatformMediaSessionManager& manager, PlatformMediaSessionClient& client)
{
    return std::unique_ptr<PlatformMediaSession>(new PlatformMediaSession(manager, client));
}

PlatformMediaSession::PlatformMediaSession(PlatformMediaSessionManager& manager, PlatformMediaSessionClient& client)
    : m_manager(makeWeakPtr(manager))
    , m_client(client)
    , m_mediaSessionIdentifier(MediaSessionIdentifier::generate())
#if !RELEASE_LOG_DISABLED
    , m_logger(client.logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
{
    manager.addSession(*this);
}

PlatformMediaSession::~PlatformMediaSession()
{
    if (m_manager)
        m_manager->removeSession(*this);
}

void PlatformMediaSession::setState(State state)
{
    if (state == m_state)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_state = state;
    if (m_state == State::Playing)
        m_hasPlayedSinceLastInterruption = true;
    m_manager->sessionStateChanged(*this);
}

void PlatformMediaSession::beginInterruption(InterruptionType type)
{
    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state, ", interruption type = ", type, ", interruption count = ", m_interruptionCount);

    // When interruptions are overridden, m_interruptionType doesn't get set.
    // Give nested interruptions a chance when the previous interruptions were overridden.
    if (++m_interruptionCount > 1 && m_interruptionType != NoInterruption)
        return;

    if (client().shouldOverrideBackgroundPlaybackRestriction(type)) {
        ALWAYS_LOG(LOGIDENTIFIER, "returning early because client says to override interruption");
        return;
    }

    m_stateToRestore = state();
    m_notifyingClient = true;
    setState(Interrupted);
    m_interruptionType = type;
    client().suspendPlayback();
    m_notifyingClient = false;
}

void PlatformMediaSession::endInterruption(EndInterruptionFlags flags)
{
    ALWAYS_LOG(LOGIDENTIFIER, "flags = ", (int)flags, ", stateToRestore = ", m_stateToRestore, ", interruption count = ", m_interruptionCount);

    if (!m_interruptionCount) {
        ALWAYS_LOG(LOGIDENTIFIER, "!! ignoring spurious interruption end !!");
        return;
    }

    if (--m_interruptionCount)
        return;

    if (m_interruptionType == NoInterruption)
        return;

    State stateToRestore = m_stateToRestore;
    m_stateToRestore = Idle;
    m_interruptionType = NoInterruption;
    setState(stateToRestore);

    if (stateToRestore == Autoplaying)
        client().resumeAutoplaying();

    bool shouldResume = flags & MayResumePlaying && stateToRestore == Playing;
    client().mayResumePlayback(shouldResume);
}

void PlatformMediaSession::clientWillBeginAutoplaying()
{
    if (m_notifyingClient)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);
    if (state() == Interrupted) {
        m_stateToRestore = Autoplaying;
        ALWAYS_LOG(LOGIDENTIFIER, "      setting stateToRestore to \"Autoplaying\"");
        return;
    }

    setState(Autoplaying);
}

bool PlatformMediaSession::clientWillBeginPlayback()
{
    if (m_notifyingClient)
        return true;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);

    if (!m_manager->sessionWillBeginPlayback(*this)) {
        if (state() == Interrupted)
            m_stateToRestore = Playing;
        return false;
    }

    setState(Playing);
    return true;
}

bool PlatformMediaSession::processClientWillPausePlayback(DelayCallingUpdateNowPlaying shouldDelayCallingUpdateNowPlaying)
{
    if (m_notifyingClient)
        return true;

    ALWAYS_LOG(LOGIDENTIFIER, "state = ", m_state);
    if (state() == Interrupted) {
        m_stateToRestore = Paused;
        ALWAYS_LOG(LOGIDENTIFIER, "      setting stateToRestore to \"Paused\"");
        return false;
    }
    
    setState(Paused);
    m_manager->sessionWillEndPlayback(*this, shouldDelayCallingUpdateNowPlaying);
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
    m_client.suspendPlayback();
}

void PlatformMediaSession::stopSession()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_client.suspendPlayback();
    m_manager->removeSession(*this);
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

void PlatformMediaSession::didReceiveRemoteControlCommand(RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument* argument)
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

bool PlatformMediaSession::shouldOverrideBackgroundLoadingRestriction() const
{
    return m_client.shouldOverrideBackgroundLoadingRestriction();
}

void PlatformMediaSession::isPlayingToWirelessPlaybackTargetChanged(bool isWireless)
{
    if (isWireless == m_isPlayingToWirelessPlaybackTarget)
        return;

    m_isPlayingToWirelessPlaybackTarget = isWireless;

    // Save and restore the interruption count so it doesn't get out of sync if beginInterruption is called because
    // if we in the background.
    int interruptionCount = m_interruptionCount;
    m_manager->sessionIsPlayingToWirelessPlaybackTargetChanged(*this);
    m_interruptionCount = interruptionCount;
}

PlatformMediaSession::DisplayType PlatformMediaSession::displayType() const
{
    return m_client.displayType();
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

void PlatformMediaSession::canProduceAudioChanged()
{
    m_manager->sessionCanProduceAudioChanged();
}

void PlatformMediaSession::clientCharacteristicsChanged()
{
    m_manager->clientCharacteristicsChanged(*this);
}

bool PlatformMediaSession::canPlayConcurrently(const PlatformMediaSession& otherSession) const
{
    return m_client.hasMediaStreamSource() && otherSession.m_client.hasMediaStreamSource();
}

bool PlatformMediaSession::shouldOverridePauseDuringRouteChange() const
{
    return m_client.shouldOverridePauseDuringRouteChange();
}

PlatformMediaSessionManager& PlatformMediaSession::manager()
{
    return *m_manager;
}

Optional<NowPlayingInfo> PlatformMediaSession::nowPlayingInfo() const
{
    return { };
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PlatformMediaSession::logChannel() const
{
    return LogMedia;
}
#endif

}

#endif
