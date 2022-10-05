/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include <wtf/Algorithms.h>
#include <wtf/Logger.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const Seconds taskDelayInterval { 100_ms };

#define ALWAYS_LOG_MEDIASESSIONMANAGER logger().logAlways

struct ClientState {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit ClientState(WebMediaSessionManagerClient& client, PlaybackTargetClientContextIdentifier contextId)
        : client(client)
        , contextId(contextId)
    {
    }

    bool operator==(const ClientState& other) const
    {
        return contextId == other.contextId && &client == &other.client;
    }

    WebMediaSessionManagerClient& client;
    PlaybackTargetClientContextIdentifier contextId;
    WebCore::MediaProducerMediaStateFlags flags;
    bool requestedPicker { false };
    bool previouslyRequestedPicker { false };
    bool configurationRequired { true };
    bool playedToEnd { false };
};

static bool flagsAreSet(MediaProducerMediaStateFlags value, MediaProducerMediaStateFlags flags)
{
    return value.containsAny(flags);
}

String mediaProducerStateString(MediaProducerMediaStateFlags flags)
{
    StringBuilder string;
    string.append(" { ");
    if (flags & MediaProducerMediaState::IsPlayingAudio)
        string.append("IsPlayingAudio+");
    if (flags & MediaProducerMediaState::IsPlayingVideo)
        string.append("IsPlayingVideo+");
    if (flags & MediaProducerMediaState::IsPlayingToExternalDevice)
        string.append("IsPlayingToExternalDevice+");
    if (flags & MediaProducerMediaState::HasPlaybackTargetAvailabilityListener)
        string.append("HasTargetAvailabilityListener+");
    if (flags & MediaProducerMediaState::RequiresPlaybackTargetMonitoring)
        string.append("RequiresTargetMonitoring+");
    if (flags & MediaProducerMediaState::ExternalDeviceAutoPlayCandidate)
        string.append("ExternalDeviceAutoPlayCandidate+");
    if (flags & MediaProducerMediaState::DidPlayToEnd)
        string.append("DidPlayToEnd+");
    if (flags & MediaProducerMediaState::HasAudioOrVideo)
        string.append("HasAudioOrVideo+");
    if (string.isEmpty())
        string.append("IsNotPlaying");
    else
        string.shrink(string.length() - 1);
    string.append(" }");
    return string.toString();
}

class WebMediaSessionLogger {
    WTF_MAKE_NONCOPYABLE(WebMediaSessionLogger);
    WTF_MAKE_FAST_ALLOCATED;
public:

    static std::unique_ptr<WebMediaSessionLogger> create(WebMediaSessionManager& manager)
    {
        return makeUnique<WebMediaSessionLogger>(manager);
    }

    template<typename... Arguments>
    inline void logAlways(const char* methodName, ClientState* state, const Arguments&... arguments) const
    {
        if (!state->client.alwaysOnLoggingAllowed())
            return;

        m_logger->logAlways(LogMedia, makeString("WebMediaSessionManager::", methodName, ' '), state->contextId.toUInt64(), state->flags, arguments...);
    }

    template<typename... Arguments>
    inline void logAlways(const char* methodName, const Arguments&... arguments) const
    {
        if (!m_manager.alwaysOnLoggingAllowed())
            return;

        m_logger->logAlways(LogMedia, makeString("WebMediaSessionManager::", methodName, ' '), arguments...);
    }

private:
    friend std::unique_ptr<WebMediaSessionLogger> std::make_unique<WebMediaSessionLogger>(WebMediaSessionManager&);
    WebMediaSessionLogger(WebMediaSessionManager& manager)
        : m_manager(manager)
        , m_logger(Logger::create(this))
    {
    }

    WebMediaSessionManager& m_manager;
    Ref<Logger> m_logger;
};

WebMediaSessionLogger& WebMediaSessionManager::logger()
{
    if (!m_logger)
        m_logger = WebMediaSessionLogger::create(*this);

    return *m_logger;
}

bool WebMediaSessionManager::alwaysOnLoggingAllowed() const
{
    return allOf(m_clientState, [] (auto& state) {
        return state->client.alwaysOnLoggingAllowed();
    });
}

void WebMediaSessionManager::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    if (m_mockPickerEnabled == enabled)
        return;

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__);
    m_mockPickerEnabled = enabled;
}

void WebMediaSessionManager::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::MockState state)
{
    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__);
    mockPicker().setState(name, state);
}

void WebMediaSessionManager::mockMediaPlaybackTargetPickerDismissPopup()
{
    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__);
    mockPicker().dismissPopup();
}

MediaPlaybackTargetPickerMock& WebMediaSessionManager::mockPicker()
{
    if (!m_pickerOverride)
        m_pickerOverride = makeUnique<MediaPlaybackTargetPickerMock>(*this);

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

PlaybackTargetClientContextIdentifier WebMediaSessionManager::addPlaybackTargetPickerClient(WebMediaSessionManagerClient& client, PlaybackTargetClientContextIdentifier contextId)
{
    size_t index = find(&client, contextId);
    ASSERT(index == notFound);
    if (index != notFound)
        return { };

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, contextId.toUInt64());
    m_clientState.append(makeUnique<ClientState>(client, contextId));

    if (m_externalOutputDeviceAvailable || m_playbackTarget)
        scheduleDelayedTask(InitialConfigurationTask | TargetClientsConfigurationTask);

    return contextId;
}

void WebMediaSessionManager::removePlaybackTargetPickerClient(WebMediaSessionManagerClient& client, PlaybackTargetClientContextIdentifier contextId)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, m_clientState[index].get());

    m_clientState.remove(index);
    scheduleDelayedTask(TargetMonitoringConfigurationTask | TargetClientsConfigurationTask);
}

void WebMediaSessionManager::removeAllPlaybackTargetPickerClients(WebMediaSessionManagerClient& client)
{
    if (m_clientState.isEmpty())
        return;

    for (size_t i = m_clientState.size(); i > 0; --i) {
        if (&m_clientState[i - 1]->client == &client) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, m_clientState[i - 1].get());
            m_clientState.remove(i - 1);
        }
    }
    scheduleDelayedTask(TargetMonitoringConfigurationTask | TargetClientsConfigurationTask);
}

void WebMediaSessionManager::showPlaybackTargetPicker(WebMediaSessionManagerClient& client, PlaybackTargetClientContextIdentifier contextId, const IntRect& rect, bool, bool useDarkAppearance)
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

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, m_clientState[index].get());

    bool hasActiveRoute = flagsAreSet(m_clientState[index]->flags, MediaProducerMediaState::IsPlayingToExternalDevice);
    targetPicker().showPlaybackTargetPicker(client.platformView(), FloatRect(rect), hasActiveRoute, useDarkAppearance);
}

void WebMediaSessionManager::clientStateDidChange(WebMediaSessionManagerClient& client, PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags newFlags)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    auto& changedClientState = m_clientState[index];
    MediaProducerMediaStateFlags oldFlags = changedClientState->flags;
    if (newFlags == oldFlags)
        return;

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, m_clientState[index].get(), "new flags = ", newFlags);

    changedClientState->flags = newFlags;

    constexpr MediaProducerMediaStateFlags updateConfigurationFlags { MediaProducerMediaState::RequiresPlaybackTargetMonitoring, MediaProducerMediaState::HasPlaybackTargetAvailabilityListener, MediaProducerMediaState::HasAudioOrVideo };
    if ((oldFlags & updateConfigurationFlags) != (newFlags & updateConfigurationFlags))
        scheduleDelayedTask(TargetMonitoringConfigurationTask);

    constexpr MediaProducerMediaStateFlags playingToTargetFlags { MediaProducerMediaState::IsPlayingToExternalDevice, MediaProducerMediaState::IsPlayingVideo };
    if ((oldFlags & playingToTargetFlags) != (newFlags & playingToTargetFlags)) {
        if (flagsAreSet(oldFlags, MediaProducerMediaState::IsPlayingVideo) && !flagsAreSet(newFlags, MediaProducerMediaState::IsPlayingVideo) && flagsAreSet(newFlags, MediaProducerMediaState::DidPlayToEnd))
            changedClientState->playedToEnd = true;
        scheduleDelayedTask(WatchdogTimerConfigurationTask);
    }

    if (!m_playbackTarget || !m_playbackTarget->hasActiveRoute() || !flagsAreSet(newFlags, MediaProducerMediaState::ExternalDeviceAutoPlayCandidate))
        return;

    // Do not interrupt another element already playing to a device.
    for (auto& state : m_clientState) {
        if (state == changedClientState)
            continue;

        if (flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingToExternalDevice) && flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingVideo)) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " returning early");
            return;
        }
    }

    // Do not begin playing to the device unless playback has just started.
    if (!flagsAreSet(newFlags, MediaProducerMediaState::IsPlayingVideo) || flagsAreSet(oldFlags, MediaProducerMediaState::IsPlayingVideo)) {
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "returning early, playback didn't just start");
        return;
    }

    for (auto& state : m_clientState) {
        if (state == changedClientState)
            continue;
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " calling setShouldPlayToPlaybackTarget(false)");
        state->client.setShouldPlayToPlaybackTarget(state->contextId, false);
    }

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, changedClientState.get(), " calling setShouldPlayToPlaybackTarget(true)");
    changedClientState->client.setShouldPlayToPlaybackTarget(changedClientState->contextId, true);

    if (index && m_clientState.size() > 1)
        std::swap(m_clientState.at(index), m_clientState.at(0));
}

void WebMediaSessionManager::setPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "has active route = ", target->hasActiveRoute());
    m_playbackTarget = WTFMove(target);
    m_targetChanged = true;
    scheduleDelayedTask(TargetClientsConfigurationTask);
}

void WebMediaSessionManager::externalOutputDeviceAvailableDidChange(bool available)
{
    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, available);
    m_externalOutputDeviceAvailable = available;
    for (auto& state : m_clientState)
        state->client.externalOutputDeviceAvailableDidChange(state->contextId, available);
}

void WebMediaSessionManager::playbackTargetPickerWasDismissed()
{
    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__);
    m_playbackTargetPickerDismissed = true;
    scheduleDelayedTask(TargetClientsConfigurationTask);
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

        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), ", requestedPicker = ", state->requestedPicker);

        if ((m_targetChanged || m_playbackTargetPickerDismissed) && state->requestedPicker)
            indexOfClientThatRequestedPicker = i;

        if (indexOfClientWillPlayToTarget == notFound && flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingToExternalDevice))
            indexOfClientWillPlayToTarget = i;

        if (indexOfClientWillPlayToTarget == notFound && haveActiveRoute && state->previouslyRequestedPicker)
            indexOfLastClientToRequestPicker = i;
    }

    if (indexOfClientThatRequestedPicker != notFound)
        indexOfClientWillPlayToTarget = indexOfClientThatRequestedPicker;
    if (indexOfClientWillPlayToTarget == notFound && indexOfLastClientToRequestPicker != notFound)
        indexOfClientWillPlayToTarget = indexOfLastClientToRequestPicker;
    if (indexOfClientWillPlayToTarget == notFound && haveActiveRoute && flagsAreSet(m_clientState[0]->flags, MediaProducerMediaState::ExternalDeviceAutoPlayCandidate) && !flagsAreSet(m_clientState[0]->flags, MediaProducerMediaState::IsPlayingVideo))
        indexOfClientWillPlayToTarget = 0;

    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];

        if (m_playbackTarget)
            state->client.setPlaybackTarget(state->contextId, *m_playbackTarget.copyRef());

        if (i != indexOfClientWillPlayToTarget || !haveActiveRoute) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " calling setShouldPlayToPlaybackTarget(false)");
            state->client.setShouldPlayToPlaybackTarget(state->contextId, false);
        }

        if (state->requestedPicker && m_playbackTargetPickerDismissed) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " calling playbackTargetPickerWasDismissed");
            state->client.playbackTargetPickerWasDismissed(state->contextId);
        }

        state->configurationRequired = false;
        if (m_targetChanged || m_playbackTargetPickerDismissed)
            state->requestedPicker = false;
    }

    if (haveActiveRoute && indexOfClientWillPlayToTarget != notFound) {
        auto& state = m_clientState[indexOfClientWillPlayToTarget];
        if (!flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingToExternalDevice)) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " calling setShouldPlayToPlaybackTarget(true)");
            state->client.setShouldPlayToPlaybackTarget(state->contextId, true);
        }
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
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get());
        if (state->flags & MediaProducerMediaState::RequiresPlaybackTargetMonitoring) {
            monitoringRequired = true;
            break;
        }
        if (state->flags & MediaProducerMediaState::HasPlaybackTargetAvailabilityListener)
            hasAvailabilityListener = true;
        if (state->flags & MediaProducerMediaState::HasAudioOrVideo)
            haveClientWithMedia = true;
    }

    if (monitoringRequired || (hasAvailabilityListener && haveClientWithMedia)) {
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "starting monitoring");
        targetPicker().startingMonitoringPlaybackTargets();
    } else {
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "stopping monitoring");
        targetPicker().stopMonitoringPlaybackTargets();
    }
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
    if (m_taskFlags & WatchdogTimerConfigurationTask)
        configureWatchdogTimer();

    m_taskFlags = NoTask;
}

size_t WebMediaSessionManager::find(WebMediaSessionManagerClient* client, PlaybackTargetClientContextIdentifier contextId)
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
        if (m_watchdogTimer.isActive()) {
            ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "stopping timer");
            m_currentWatchdogInterval = { };
            m_watchdogTimer.stop();
        }

        return;
    }

    bool stopTimer = false;
    bool didPlayToEnd = false;
    for (auto& state : m_clientState) {

        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, state.get(), " playedToEnd = ", state->playedToEnd);

        if (flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingToExternalDevice) && flagsAreSet(state->flags, MediaProducerMediaState::IsPlayingVideo))
            stopTimer = true;
        if (state->playedToEnd)
            didPlayToEnd = true;
        state->playedToEnd = false;
    }

    if (stopTimer) {
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "stopping timer");
        m_currentWatchdogInterval = { };
        m_watchdogTimer.stop();
    } else {
        Seconds interval = didPlayToEnd ? watchdogTimerIntervalAfterPlayingToEnd : watchdogTimerIntervalAfterPausing;
        if (interval != m_currentWatchdogInterval || !m_watchdogTimer.isActive()) {
            m_watchdogTimer.startOneShot(interval);
        }
        ALWAYS_LOG_MEDIASESSIONMANAGER(__func__, "timer scheduled for ", interval.value(), " seconds");
        m_currentWatchdogInterval = interval;
    }
}

void WebMediaSessionManager::watchdogTimerFired()
{
    if (!m_playbackTarget)
        return;

    ALWAYS_LOG_MEDIASESSIONMANAGER(__func__);
    targetPicker().invalidatePlaybackTargets();
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
