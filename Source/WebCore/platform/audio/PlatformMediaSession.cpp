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

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "PlatformMediaSessionManager.h"

namespace WebCore {

const double kClientDataBufferingTimerThrottleDelay = 0.1;

#if !LOG_DISABLED
static const char* stateName(PlatformMediaSession::State state)
{
#define CASE(state) case PlatformMediaSession::state: return #state
    switch (state) {
    CASE(Idle);
    CASE(Playing);
    CASE(Paused);
    CASE(Interrupted);
    }

    ASSERT_NOT_REACHED();
    return "";
}
#endif

std::unique_ptr<PlatformMediaSession> PlatformMediaSession::create(PlatformMediaSessionClient& client)
{
    return std::make_unique<PlatformMediaSession>(client);
}

PlatformMediaSession::PlatformMediaSession(PlatformMediaSessionClient& client)
    : m_client(client)
    , m_clientDataBufferingTimer(*this, &PlatformMediaSession::clientDataBufferingTimerFired)
    , m_state(Idle)
    , m_stateToRestore(Idle)
    , m_notifyingClient(false)
{
    ASSERT(m_client.mediaType() >= None && m_client.mediaType() <= WebAudio);
    PlatformMediaSessionManager::sharedManager().addSession(*this);
}

PlatformMediaSession::~PlatformMediaSession()
{
    PlatformMediaSessionManager::sharedManager().removeSession(*this);
}

void PlatformMediaSession::setState(State state)
{
    LOG(Media, "PlatformMediaSession::setState(%p) - %s", this, stateName(state));
    m_state = state;
}

void PlatformMediaSession::doInterruption()
{
    m_stateToRestore = state();
    m_notifyingClient = true;
    setState(Interrupted);
    client().suspendPlayback();
    m_notifyingClient = false;
}

bool PlatformMediaSession::shouldDoInterruption(InterruptionType type)
{
    return type != EnteringBackground || !client().overrideBackgroundPlaybackRestriction();
}

void PlatformMediaSession::beginInterruption(InterruptionType type)
{
    LOG(Media, "PlatformMediaSession::beginInterruption(%p), state = %s, interruption count = %i", this, stateName(m_state), m_interruptionCount);

    if (++m_interruptionCount > 1 || !shouldDoInterruption(type))
        return;

    doInterruption();
}

void PlatformMediaSession::forceInterruption(InterruptionType type)
{
    LOG(Media, "PlatformMediaSession::forceInterruption(%p), state = %s, interruption count = %i", this, stateName(m_state), m_interruptionCount);

    // beginInterruption() must have been called before calling this function.
    if (!m_interruptionCount) {
        ASSERT_NOT_REACHED();
        return;
    }

    // The purpose of this function is to override the decision which was made by
    // beginInterruption(). If it was decided to interrupt the media session there,
    // then nothing should be done here.
    if (shouldDoInterruption(type))
        return;

    doInterruption();
}

void PlatformMediaSession::endInterruption(EndInterruptionFlags flags)
{
    LOG(Media, "PlatformMediaSession::endInterruption(%p) - flags = %i, stateToRestore = %s, interruption count = %i", this, (int)flags, stateName(m_stateToRestore), m_interruptionCount);

    if (!m_interruptionCount) {
        LOG(Media, "PlatformMediaSession::endInterruption(%p) - !! ignoring spurious interruption end !!", this);
        return;
    }

    if (--m_interruptionCount)
        return;

    State stateToRestore = m_stateToRestore;
    m_stateToRestore = Idle;
    setState(Paused);

    bool shouldResume = flags & MayResumePlaying && stateToRestore == Playing;
    client().mayResumePlayback(shouldResume);
}

bool PlatformMediaSession::clientWillBeginPlayback()
{
    if (m_notifyingClient)
        return true;

    if (!PlatformMediaSessionManager::sharedManager().sessionWillBeginPlayback(*this)) {
        if (state() == Interrupted)
            m_stateToRestore = Playing;
        return false;
    }

    setState(Playing);
    updateClientDataBuffering();
    return true;
}

bool PlatformMediaSession::clientWillPausePlayback()
{
    if (m_notifyingClient)
        return true;

    LOG(Media, "PlatformMediaSession::clientWillPausePlayback(%p)- state = %s", this, stateName(m_state));
    if (state() == Interrupted) {
        m_stateToRestore = Paused;
        LOG(Media, "      setting stateToRestore to \"Paused\"");
        return false;
    }
    
    setState(Paused);
    PlatformMediaSessionManager::sharedManager().sessionWillEndPlayback(*this);
    if (!m_clientDataBufferingTimer.isActive())
        m_clientDataBufferingTimer.startOneShot(kClientDataBufferingTimerThrottleDelay);
    return true;
}

void PlatformMediaSession::pauseSession()
{
    LOG(Media, "PlatformMediaSession::pauseSession(%p)", this);
    m_client.suspendPlayback();
}

PlatformMediaSession::MediaType PlatformMediaSession::mediaType() const
{
    return m_client.mediaType();
}

PlatformMediaSession::MediaType PlatformMediaSession::presentationType() const
{
    return m_client.presentationType();
}

String PlatformMediaSession::title() const
{
    return m_client.mediaSessionTitle();
}

double PlatformMediaSession::duration() const
{
    return m_client.mediaSessionDuration();
}

double PlatformMediaSession::currentTime() const
{
    return m_client.mediaSessionCurrentTime();
}
    
bool PlatformMediaSession::canReceiveRemoteControlCommands() const
{
    return m_client.canReceiveRemoteControlCommands();
}

void PlatformMediaSession::didReceiveRemoteControlCommand(RemoteControlCommandType command)
{
    m_client.didReceiveRemoteControlCommand(command);
}

void PlatformMediaSession::visibilityChanged()
{
    if (!m_clientDataBufferingTimer.isActive())
        m_clientDataBufferingTimer.startOneShot(kClientDataBufferingTimerThrottleDelay);
}

void PlatformMediaSession::clientDataBufferingTimerFired()
{
    LOG(Media, "PlatformMediaSession::clientDataBufferingTimerFired(%p)- visible = %s", this, m_client.elementIsHidden() ? "false" : "true");

    updateClientDataBuffering();

#if PLATFORM(IOS)
    PlatformMediaSessionManager::sharedManager().configureWireLessTargetMonitoring();
#endif

    if (m_state != Playing || !m_client.elementIsHidden())
        return;

    PlatformMediaSessionManager::SessionRestrictions restrictions = PlatformMediaSessionManager::sharedManager().restrictions(mediaType());
    if ((restrictions & PlatformMediaSessionManager::BackgroundTabPlaybackRestricted) == PlatformMediaSessionManager::BackgroundTabPlaybackRestricted)
        pauseSession();
}

void PlatformMediaSession::updateClientDataBuffering()
{
    if (m_clientDataBufferingTimer.isActive())
        m_clientDataBufferingTimer.stop();

    m_client.setShouldBufferData(PlatformMediaSessionManager::sharedManager().sessionCanLoadMedia(*this));
}

bool PlatformMediaSession::isHidden() const
{
    return m_client.elementIsHidden();
}

PlatformMediaSession::DisplayType PlatformMediaSession::displayType() const
{
    return m_client.displayType();
}

String PlatformMediaSessionClient::mediaSessionTitle() const
{
    return String();
}

double PlatformMediaSessionClient::mediaSessionDuration() const
{
    return MediaPlayer::invalidTime();
}

double PlatformMediaSessionClient::mediaSessionCurrentTime() const
{
    return MediaPlayer::invalidTime();
}
}
#endif
