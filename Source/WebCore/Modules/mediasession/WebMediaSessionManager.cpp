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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include "FloatRect.h"
#include "Logging.h"
#include "MediaPlaybackTargetPickerMock.h"
#include "WebMediaSessionManagerClient.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const Seconds taskDelayInterval { 100_ms };

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
    bool previouslyRequestedPicker { false };
    bool configurationRequired { true };
    bool playedToEnd { false };
};

static bool flagsAreSet(MediaProducer::MediaStateFlags value, unsigned flags)
{
    return value & flags;
}

#if !LOG_DISABLED
static String mediaProducerStateString(MediaProducer::MediaStateFlags flags)
{
    StringBuilder string;
    if (flags & MediaProducer::IsPlayingAudio)
        string.append("IsPlayingAudio + ");
    if (flags & MediaProducer::IsPlayingVideo)
        string.append("IsPlayingVideo + ");
    if (flags & MediaProducer::IsPlayingToExternalDevice)
        string.append("IsPlayingToExternalDevice + ");
    if (flags & MediaProducer::HasPlaybackTargetAvailabilityListener)
        string.append("HasPlaybackTargetAvailabilityListener + ");
    if (flags & MediaProducer::RequiresPlaybackTargetMonitoring)
        string.append("RequiresPlaybackTargetMonitoring + ");
    if (flags & MediaProducer::ExternalDeviceAutoPlayCandidate)
        string.append("ExternalDeviceAutoPlayCandidate + ");
    if (flags & MediaProducer::DidPlayToEnd)
        string.append("DidPlayToEnd + ");
    if (flags & MediaProducer::HasAudioOrVideo)
        string.append("HasAudioOrVideo + ");
    if (string.isEmpty())
        string.append("IsNotPlaying");
    else
        string.resize(string.length() - 2);

    return string.toString();
}
#endif

void WebMediaSessionManager::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    LOG(Media, "WebMediaSessionManager::setMockMediaPlaybackTargetPickerEnabled - enabled = %i", (int)enabled);

    if (m_mockPickerEnabled == enabled)
        return;

    m_mockPickerEnabled = enabled;
}

void WebMediaSessionManager::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::State state)
{
    LOG(Media, "WebMediaSessionManager::setMockMediaPlaybackTargetPickerState - name = %s, state = %i", name.utf8().data(), (int)state);

    mockPicker().setState(name, state);
}

MediaPlaybackTargetPickerMock& WebMediaSessionManager::mockPicker()
{
    if (!m_pickerOverride)
        m_pickerOverride = std::make_unique<MediaPlaybackTargetPickerMock>(*this);

    return *m_pickerOverride.get();
}

WebCore::MediaPlaybackTargetPicker& WebMediaSessionManager::targetPicker()
{
    if (m_mockPickerEnabled)
        return mockPicker();

    return platformPicker();
}

WebMediaSessionManager::WebMediaSessionManager()
    : m_taskTimer(RunLoop::current(), this, &WebMediaSessionManager::taskTimerFired)
    , m_watchdogTimer(RunLoop::current(), this, &WebMediaSessionManager::watchdogTimerFired)
{
}

WebMediaSessionManager::~WebMediaSessionManager() = default;

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
    if (m_clientState.isEmpty())
        return;

    LOG(Media, "WebMediaSessionManager::removeAllPlaybackTargetPickerClients(%p)", &client);

    for (size_t i = m_clientState.size(); i > 0; --i) {
        if (&m_clientState[i - 1]->client == &client)
            m_clientState.remove(i - 1);
    }
    scheduleDelayedTask(TargetMonitoringConfigurationTask | TargetClientsConfigurationTask);
}

void WebMediaSessionManager::showPlaybackTargetPicker(WebMediaSessionManagerClient& client, uint64_t contextId, const IntRect& rect, bool, bool useDarkAppearance)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    auto& clientRequestingPicker = m_clientState[index];
    for (auto& state : m_clientState) {
        state->requestedPicker = state == clientRequestingPicker;
        state->previouslyRequestedPicker = state == clientRequestingPicker;
    }

    bool hasActiveRoute = flagsAreSet(m_clientState[index]->flags, MediaProducer::IsPlayingToExternalDevice);
    LOG(Media, "WebMediaSessionManager::showPlaybackTargetPicker(%p + %llu) - hasActiveRoute = %i", &client, contextId, (int)hasActiveRoute);
    targetPicker().showPlaybackTargetPicker(FloatRect(rect), hasActiveRoute, useDarkAppearance);
}

void WebMediaSessionManager::clientStateDidChange(WebMediaSessionManagerClient& client, uint64_t contextId, MediaProducer::MediaStateFlags newFlags)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    auto& changedClientState = m_clientState[index];
    MediaProducer::MediaStateFlags oldFlags = changedClientState->flags;
    if (newFlags == oldFlags)
        return;

    LOG(Media, "WebMediaSessionManager::clientStateDidChange(%p + %llu) - new flags = %s, old flags = %s", &client, contextId, mediaProducerStateString(newFlags).utf8().data(), mediaProducerStateString(oldFlags).utf8().data());

    changedClientState->flags = newFlags;

    MediaProducer::MediaStateFlags updateConfigurationFlags = MediaProducer::RequiresPlaybackTargetMonitoring | MediaProducer::HasPlaybackTargetAvailabilityListener | MediaProducer::HasAudioOrVideo;
    if ((oldFlags & updateConfigurationFlags) != (newFlags & updateConfigurationFlags))
        scheduleDelayedTask(TargetMonitoringConfigurationTask);

    MediaProducer::MediaStateFlags playingToTargetFlags = MediaProducer::IsPlayingToExternalDevice | MediaProducer::IsPlayingVideo;
    if ((oldFlags & playingToTargetFlags) != (newFlags & playingToTargetFlags)) {
        if (flagsAreSet(oldFlags, MediaProducer::IsPlayingVideo) && !flagsAreSet(newFlags, MediaProducer::IsPlayingVideo) && flagsAreSet(newFlags, MediaProducer::DidPlayToEnd))
            changedClientState->playedToEnd = true;
        scheduleDelayedTask(WatchdogTimerConfigurationTask);
    }

    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute() || !flagsAreSet(newFlags, MediaProducer::ExternalDeviceAutoPlayCandidate))
        return;

    // Do not interrupt another element already playing to a device.
    for (auto& state : m_clientState) {
        if (state == changedClientState)
            continue;

        if (flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice) && flagsAreSet(state->flags, MediaProducer::IsPlayingVideo))
            return;
    }

    // Do not begin playing to the device unless playback has just started.
    if (!flagsAreSet(newFlags, MediaProducer::IsPlayingVideo) || flagsAreSet(oldFlags, MediaProducer::IsPlayingVideo))
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
    m_playbackTarget = WTFMove(target);
    m_targetChanged = true;
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
    if (m_clientState.isEmpty())
        return;

    size_t indexOfClientThatRequestedPicker = notFound;
    size_t indexOfLastClientToRequestPicker = notFound;
    size_t indexOfClientWillPlayToTarget = notFound;
    bool haveActiveRoute = m_playbackTarget && m_playbackTarget->hasActiveRoute();

    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];

        LOG(Media, "WebMediaSessionManager::configurePlaybackTargetClients %zu - client (%p + %llu) requestedPicker = %i, flags = %s", i, &state->client, state->contextId, state->requestedPicker, mediaProducerStateString(state->flags).utf8().data());

        if (m_targetChanged && state->requestedPicker)
            indexOfClientThatRequestedPicker = i;

        if (indexOfClientWillPlayToTarget == notFound && flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice))
            indexOfClientWillPlayToTarget = i;

        if (indexOfClientWillPlayToTarget == notFound && haveActiveRoute && state->previouslyRequestedPicker)
            indexOfLastClientToRequestPicker = i;
    }

    if (indexOfClientThatRequestedPicker != notFound)
        indexOfClientWillPlayToTarget = indexOfClientThatRequestedPicker;
    if (indexOfClientWillPlayToTarget == notFound && indexOfLastClientToRequestPicker != notFound)
        indexOfClientWillPlayToTarget = indexOfLastClientToRequestPicker;
    if (indexOfClientWillPlayToTarget == notFound && haveActiveRoute && flagsAreSet(m_clientState[0]->flags, MediaProducer::ExternalDeviceAutoPlayCandidate) && !flagsAreSet(m_clientState[0]->flags, MediaProducer::IsPlayingVideo))
        indexOfClientWillPlayToTarget = 0;

    LOG(Media, "WebMediaSessionManager::configurePlaybackTargetClients - indexOfClientWillPlayToTarget = %zu", indexOfClientWillPlayToTarget);

    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];

        if (m_playbackTarget)
            state->client.setPlaybackTarget(state->contextId, *m_playbackTarget.copyRef());

        if (i != indexOfClientWillPlayToTarget || !haveActiveRoute)
            state->client.setShouldPlayToPlaybackTarget(state->contextId, false);

        state->configurationRequired = false;
        if (m_targetChanged)
            state->requestedPicker = false;
    }

    if (haveActiveRoute && indexOfClientWillPlayToTarget != notFound) {
        auto& state = m_clientState[indexOfClientWillPlayToTarget];
        if (!flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice))
            state->client.setShouldPlayToPlaybackTarget(state->contextId, true);
    }

    m_targetChanged = false;
    configureWatchdogTimer();
}

void WebMediaSessionManager::configurePlaybackTargetMonitoring()
{
    bool monitoringRequired = false;
    bool hasAvailabilityListener = false;
    bool haveClientWithMedia = false;
    for (auto& state : m_clientState) {
        if (state->flags & MediaProducer::RequiresPlaybackTargetMonitoring) {
            monitoringRequired = true;
            break;
        }
        if (state->flags & MediaProducer::HasPlaybackTargetAvailabilityListener)
            hasAvailabilityListener = true;
        if (state->flags & MediaProducer::HasAudioOrVideo)
            haveClientWithMedia = true;
    }

    LOG(Media, "WebMediaSessionManager::configurePlaybackTargetMonitoring - monitoringRequired = %i", static_cast<int>(monitoringRequired || (hasAvailabilityListener && haveClientWithMedia)));

    if (monitoringRequired || (hasAvailabilityListener && haveClientWithMedia))
        targetPicker().startingMonitoringPlaybackTargets();
    else
        targetPicker().stopMonitoringPlaybackTargets();
}

#if !LOG_DISABLED
String WebMediaSessionManager::toString(ConfigurationTasks tasks)
{
    StringBuilder string;
    if (tasks & InitialConfigurationTask)
        string.append("InitialConfigurationTask + ");
    if (tasks & TargetClientsConfigurationTask)
        string.append("TargetClientsConfigurationTask + ");
    if (tasks & TargetMonitoringConfigurationTask)
        string.append("TargetMonitoringConfigurationTask + ");
    if (tasks & WatchdogTimerConfigurationTask)
        string.append("WatchdogTimerConfigurationTask + ");
    if (string.isEmpty())
        string.append("NoTask");
    else
        string.resize(string.length() - 2);
    
    return string.toString();
}
#endif

void WebMediaSessionManager::scheduleDelayedTask(ConfigurationTasks tasks)
{
    LOG(Media, "WebMediaSessionManager::scheduleDelayedTask - %s", toString(tasks).utf8().data());

    m_taskFlags |= tasks;
    m_taskTimer.startOneShot(taskDelayInterval);
}

void WebMediaSessionManager::taskTimerFired()
{
    LOG(Media, "WebMediaSessionManager::taskTimerFired - tasks = %s", toString(m_taskFlags).utf8().data());

    if (m_taskFlags & InitialConfigurationTask)
        configureNewClients();
    if (m_taskFlags & TargetClientsConfigurationTask)
        configurePlaybackTargetClients();
    if (m_taskFlags & TargetMonitoringConfigurationTask)
        configurePlaybackTargetMonitoring();
    if (m_taskFlags & WatchdogTimerConfigurationTask)
        configureWatchdogTimer();

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

void WebMediaSessionManager::configureWatchdogTimer()
{
    static const Seconds watchdogTimerIntervalAfterPausing { 1_h };
    static const Seconds watchdogTimerIntervalAfterPlayingToEnd { 8_min };

    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute()) {
        m_watchdogTimer.stop();
        return;
    }

    bool stopTimer = false;
    bool didPlayToEnd = false;
    for (auto& state : m_clientState) {
        if (flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice) && flagsAreSet(state->flags, MediaProducer::IsPlayingVideo))
            stopTimer = true;
        if (state->playedToEnd)
            didPlayToEnd = true;
        state->playedToEnd = false;
    }

    if (stopTimer) {
        m_currentWatchdogInterval = { };
        m_watchdogTimer.stop();
        LOG(Media, "WebMediaSessionManager::configureWatchdogTimer - timer stopped");
    } else {
        Seconds interval = didPlayToEnd ? watchdogTimerIntervalAfterPlayingToEnd : watchdogTimerIntervalAfterPausing;
        if (interval != m_currentWatchdogInterval || !m_watchdogTimer.isActive()) {
            m_watchdogTimer.startOneShot(interval);
            LOG(Media, "WebMediaSessionManager::configureWatchdogTimer - timer scheduled for %.0f seconds", interval.value());
        }
        m_currentWatchdogInterval = interval;
    }
}

void WebMediaSessionManager::watchdogTimerFired()
{
    LOG(Media, "WebMediaSessionManager::watchdogTimerFired");
    if (!m_playbackTarget)
        return;

    targetPicker().invalidatePlaybackTargets();
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
