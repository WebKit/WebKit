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
#include "MediaPlaybackTargetPickerMac.h"
#include "WebMediaSessionManagerClient.h"

namespace WebCore {

struct ClientState {
    explicit ClientState(WebMediaSessionManagerClient& client, uint64_t contextId)
        : client(&client)
        , contextId(contextId)
    {
    }

    WebMediaSessionManagerClient* client { nullptr };
    uint64_t contextId { 0 };
    WebCore::MediaProducer::MediaStateFlags flags { WebCore::MediaProducer::IsNotPlaying };
    bool requestedPicker { false };
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

    m_clientState.append(std::make_unique<ClientState>(client, contextId));

    if (m_externalOutputDeviceAvailable || m_playbackTarget) {

        TaskCallback callback = std::make_tuple(&client, contextId, [this](ClientState& state) {

            if (m_externalOutputDeviceAvailable)
                state.client->externalOutputDeviceAvailableDidChange(state.contextId, true);

            if (m_playbackTarget) {
                state.client->setPlaybackTarget(state.contextId, *m_playbackTarget.copyRef());

                if (m_clientState.size() == 1 && m_playbackTarget->hasActiveRoute())
                    state.client->setShouldPlayToPlaybackTarget(state.contextId, true);
            }
        });

        m_taskQueue.append(callback);
        m_taskTimer.startOneShot(0);
    }

    return contextId;
}

void WebMediaSessionManager::removePlaybackTargetPickerClient(WebMediaSessionManagerClient& client, uint64_t contextId)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    m_clientState.remove(index);
    configurePlaybackTargetMonitoring();

    if (m_playbackTarget && m_clientState.size() == 1 && m_playbackTarget->hasActiveRoute())
        m_clientState[0]->client->setShouldPlayToPlaybackTarget(m_clientState[0]->contextId, true);
}

void WebMediaSessionManager::removeAllPlaybackTargetPickerClients(WebMediaSessionManagerClient& client)
{
    for (size_t i = m_clientState.size(); i > 0; --i) {
        if (m_clientState[i - 1]->client == &client)
            m_clientState.remove(i - 1);
    }
}

void WebMediaSessionManager::showPlaybackTargetPicker(WebMediaSessionManagerClient& client, uint64_t contextId, const IntRect& rect, bool isVideo)
{
    size_t index = find(&client, contextId);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    for (auto& state : m_clientState)
        state->requestedPicker = (state->contextId == contextId && state->client == &client);

    targetPicker().showPlaybackTargetPicker(FloatRect(rect), isVideo);
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

    changedClientState->flags = newFlags;
    if (!flagsAreSet(oldFlags, MediaProducer::RequiresPlaybackTargetMonitoring) && flagsAreSet(newFlags, MediaProducer::RequiresPlaybackTargetMonitoring))
        configurePlaybackTargetMonitoring();

    if (!flagsAreSet(newFlags, MediaProducer::IsPlayingVideo))
        return;

    // Do not interrupt another element already playing to a device.
    for (auto& state : m_clientState) {
        if (state->contextId == contextId && state->client == &client)
            continue;

        if (flagsAreSet(state->flags, MediaProducer::IsPlayingVideo) && flagsAreSet(state->flags, MediaProducer::IsPlayingToExternalDevice))
            return;
    }

    if (m_playbackTarget && m_playbackTarget->hasActiveRoute()) {

        for (auto& state : m_clientState) {
            if (state->contextId == contextId && state->client == &client)
                continue;
            state->client->setShouldPlayToPlaybackTarget(state->contextId, false);
        }

        changedClientState->client->setShouldPlayToPlaybackTarget(changedClientState->contextId, true);
    }

    if (index && m_clientState.size() > 1)
        std::swap(m_clientState.at(index), m_clientState.at(0));
}

void WebMediaSessionManager::setPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    m_playbackTarget = WTF::move(target);

    size_t indexThatRequestedPicker = notFound;
    for (size_t i = 0; i < m_clientState.size(); ++i) {
        auto& state = m_clientState[i];
        state->client->setPlaybackTarget(state->contextId, *m_playbackTarget.copyRef());
        if (state->requestedPicker) {
            indexThatRequestedPicker = i;
            continue;
        }
        state->client->setShouldPlayToPlaybackTarget(state->contextId, false);
        state->requestedPicker = false;
    }

    if (indexThatRequestedPicker == notFound)
        return;

    auto& state = m_clientState[indexThatRequestedPicker];
    state->client->setShouldPlayToPlaybackTarget(state->contextId, true);
    state->requestedPicker = false;
}

void WebMediaSessionManager::externalOutputDeviceAvailableDidChange(bool available)
{
    if (m_externalOutputDeviceAvailable == available)
        return;

    m_externalOutputDeviceAvailable = available;
    for (auto& state : m_clientState)
        state->client->externalOutputDeviceAvailableDidChange(state->contextId, available);
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

    if (monitoringRequired)
        targetPicker().startingMonitoringPlaybackTargets();
    else
        targetPicker().stopMonitoringPlaybackTargets();
}

void WebMediaSessionManager::taskTimerFired()
{
    auto taskQueue = WTF::move(m_taskQueue);
    if (taskQueue.isEmpty())
        return;

    for (auto& task : taskQueue) {
        size_t index = find(std::get<0>(task), std::get<1>(task));

        if (index == notFound)
            continue;

        std::get<2>(task)(*m_clientState[index]);
    }
}

size_t WebMediaSessionManager::find(WebMediaSessionManagerClient* client, uint64_t contextId)
{
    for (size_t i = 0; i < m_clientState.size(); ++i) {
        if (m_clientState[i]->contextId == contextId && m_clientState[i]->client == client)
            return i;
    }

    return notFound;
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
