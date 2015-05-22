/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebMediaSessionManager.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#include "FloatRect.h"
#include "Logging.h"
#include "MediaPlaybackTargetPickerMac.h"
#include "WebMediaSessionManagerClient.h"

namespace WebCore {

static const double taskDelayInterval = 1.0 / 10.0;

struct ClientState {
    explicit ClientState(WebMediaSessionManagerClient& client, uint64_t contextId)
        : client(client)
        , contextId(contextId)
    {
    }

    bool operator == (ClientState const& other) const
    {
        return contextId == other.contextId && &client == &other.client;
    }

    WebMediaSessionManagerClient& client;
    uint64_t contextId { 0 };
    WebCore::MediaProducer::MediaStateFlags flags { WebCore::MediaProducer::IsNotPlaying };
    bool requestedPicker { false };
    bool configurationRequired { true };
};

static bool flagsAreSet(MediaProducer::MediaStateFlags value, unsigned flags)
{
    return value & flags;
}

WebMediaSessionManager::WebMediaSessionManager()
    : m_taskTimer(RunLoop::current(), this, &WebMediaSessionManager::taskTimerFired)
{
}

WebMediaSessionManager::~WebMediaSessionManager()
{
}

uint64_t WebMediaSessionManager::addPlaybackTargetPickerClient(WebMediaSessionManagerClient& client, uint64_t contextId)
{
    size_t index = find(&client, contextId);
    ASSERT(index == notFound);
    if (index != notFound)
        return 0;

    LOG(Media, "WebMediaSessionManager::addPlaybackTargetPickerClient(%p + %llu)", &client, contextId);

    m_clientState.append(std::make_unique<ClientState>(client, contextId));

    if (m_externalOutputDeviceAvailable || m_playbackTarget)
        scheduleDelayedTask(InitialConfigurationTask | TargetClientsConfigurationTask);

    return contextId;
}

void WebMediaSessionManager::removePlaybackTargetPickerClient(WebMediaSessionManagerClient& client, uint64_t contextId)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    LOG(Media, "WebMediaSessionManager::removePlaybackTargetPickerClient(%p + %llu)", &client, contextId);

    m_clientState.remove(index);
    scheduleDelayedTask(TargetMonitoringConfigurationTask | TargetClientsConfigurationTask);
}

void WebMediaSessionManager::removeAllPlaybackTargetPickerClients(WebMediaSessionManagerClient& client)
{
    LOG(Media, "WebMediaSessionManager::removeAllPlaybackTargetPickerClients(%p)", &client);

    for (size_t i = m_clientState.size(); i > 0; --i) {
        if (&m_clientState[i - 1]->client == &client)
            m_clientState.remove(i - 1);
    }
    scheduleDelayedTask(TargetMonitoringConfigurationTask | TargetClientsConfigurationTask);
}

void WebMediaSessionManager::showPlaybackTargetPicker(WebMediaSessionManagerClient& client, uint64_t contextId, const IntRect& rect, bool)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    auto& clientRequestingPicker = m_clientState[index];
    for (auto& state : m_clientState)
        state->requestedPicker = state == clientRequestingPicker;

    bool hasActiveRoute = flagsAreSet(m_clientState[index]->flags, MediaProducer::IsPlayingToExternalDevice);
    LOG(Media, "WebMediaSessionManager::showPlaybackTargetPicker(%p + %llu) - hasActiveRoute = %i", &client, contextId, (int)hasActiveRoute);
    targetPicker().showPlaybackTargetPicker(FloatRect(rect), hasActiveRoute);
}

void WebMediaSessionManager::clientStateDidChange(WebMediaSessionManagerClient& client, uint64_t contextId, MediaProducer::MediaStateFlags newFlags)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    auto& changedClientState = m_clientState[index];
    MediaProducer::MediaStateFlags oldFlags = changedClientState->flags;
    LOG(Media, "WebMediaSessionManager::clientStateDidChange(%p + %llu) - new flags = 0x%x, old flags = 0x%x", &client, contextId, newFlags, oldFlags);
    if (newFlags == oldFlags)
        return;

    changedClientState->flags = newFlags;
    if (!flagsAreSet(oldFlags, MediaProducer::RequiresPlaybackTargetMonitoring) && flagsAreSet(newFlags, MediaProducer::RequiresPlaybackTargetMonitoring))
        scheduleDelayedTask(TargetMonitoringConfigurationTask);

    if (!flagsAreSet(newFlags, MediaProducer::ExternalDeviceAutoPlayCandidate))
        return;

    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute())
        return;

    // Do not interrupt another element already playing to a device.
    bool anotherClientHasActiveTarget = false;
    for (auto& state : m_clientState) {
        if (flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice)) {
            if (flagsAreSet(state->flags, MediaProducer::IsPlayingVideo))
                return;
            anotherClientHasActiveTarget = true;
        }
    }

    // Do not take the target if another client has it and the client reporting a state change is not playing.
    if (anotherClientHasActiveTarget && !flagsAreSet(newFlags, MediaProducer::IsPlayingVideo))
        return;

    for (auto& state : m_clientState) {
        if (state == changedClientState)
            continue;
        state->client.setShouldPlayToPlaybackTarget(state->contextId, false);
    }

    changedClientState->client.setShouldPlayToPlaybackTarget(changedClientState->contextId, true);

    if (index && m_clientState.size() > 1)
        std::swap(m_clientState.at(index), m_clientState.at(0));
}

void WebMediaSessionManager::setPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    m_playbackTarget = WTF::move(target);
    scheduleDelayedTask(TargetClientsConfigurationTask);
}

void WebMediaSessionManager::externalOutputDeviceAvailableDidChange(bool available)
{
    LOG(Media, "WebMediaSessionManager::externalOutputDeviceAvailableDidChange - clients = %zu, available = %i", m_clientState.size(), (int)available);

    m_externalOutputDeviceAvailable = available;
    for (auto& state : m_clientState)
        state->client.externalOutputDeviceAvailableDidChange(state->contextId, available);
}

void WebMediaSessionManager::configureNewClients()
{
    for (auto& state : m_clientState) {
        if (!state->configurationRequired)
            continue;

        state->configurationRequired = false;
        if (m_externalOutputDeviceAvailable)
            state->client.externalOutputDeviceAvailableDidChange(state->contextId, true);

        if (m_playbackTarget)
            state->client.setPlaybackTarget(state->contextId, *m_playbackTarget.copyRef());
    }
}

void WebMediaSessionManager::configurePlaybackTargetClients()
{
    size_t indexOfClientThatRequestedPicker = notFound;
    size_t indexOfAutoPlayCandidate = notFound;
    size_t indexOfClientWillPlayToTarget = notFound;
    bool haveActiveRoute = m_playbackTarget && m_playbackTarget->hasActiveRoute();

    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];

        if (indexOfClientThatRequestedPicker == notFound && state->requestedPicker)
            indexOfClientThatRequestedPicker = i;

        if (indexOfClientWillPlayToTarget == notFound && flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice))
            indexOfClientWillPlayToTarget = i;

        if (indexOfAutoPlayCandidate == notFound && flagsAreSet(state->flags, MediaProducer::ExternalDeviceAutoPlayCandidate) && !flagsAreSet(state->flags, MediaProducer::IsPlayingVideo))
            indexOfAutoPlayCandidate = i;
    }

    if (indexOfClientThatRequestedPicker != notFound)
        indexOfClientWillPlayToTarget = indexOfClientThatRequestedPicker;
    if (indexOfClientWillPlayToTarget == notFound && haveActiveRoute && indexOfAutoPlayCandidate != notFound)
        indexOfClientWillPlayToTarget = indexOfAutoPlayCandidate;

    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];

        if (m_playbackTarget)
            state->client.setPlaybackTarget(state->contextId, *m_playbackTarget.copyRef());

        if (i != indexOfClientWillPlayToTarget)
            state->client.setShouldPlayToPlaybackTarget(state->contextId, false);
        else if (!flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice))
            state->client.setShouldPlayToPlaybackTarget(state->contextId, haveActiveRoute);

        state->configurationRequired = false;
        state->requestedPicker = false;
    }
}

void WebMediaSessionManager::configurePlaybackTargetMonitoring()
{
    bool monitoringRequired = false;
    for (auto& state : m_clientState) {
        if (state->flags & MediaProducer::RequiresPlaybackTargetMonitoring) {
            monitoringRequired = true;
            break;
        }
    }

    LOG(Media, "WebMediaSessionManager::configurePlaybackTargetMonitoring - monitoringRequired = %i", (int)monitoringRequired);

    if (monitoringRequired)
        targetPicker().startingMonitoringPlaybackTargets();
    else
        targetPicker().stopMonitoringPlaybackTargets();
}

void WebMediaSessionManager::scheduleDelayedTask(ConfigurationTasks tasks)
{
    m_taskFlags |= tasks;
    m_taskTimer.startOneShot(taskDelayInterval);
}

void WebMediaSessionManager::taskTimerFired()
{
    if (m_taskFlags & InitialConfigurationTask)
        configureNewClients();
    if (m_taskFlags & TargetClientsConfigurationTask)
        configurePlaybackTargetClients();
    if (m_taskFlags & TargetMonitoringConfigurationTask)
        configurePlaybackTargetMonitoring();

    m_taskFlags = NoTask;
}

size_t WebMediaSessionManager::find(WebMediaSessionManagerClient* client, uint64_t contextId)
{
    for (size_t i = 0; i < m_clientState.size(); ++i) {
        if (m_clientState[i]->contextId == contextId && &m_clientState[i]->client == client)
            return i;
    }

    return notFound;
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
