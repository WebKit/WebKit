/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "TrackPrivateBase.h"
#include <wtf/SharedTask.h>

#if ENABLE(VIDEO)

#include "Logging.h"

namespace WebCore {

std::optional<AtomString> TrackPrivateBase::trackUID() const
{
    return std::nullopt;
}

std::optional<bool> TrackPrivateBase::defaultEnabled() const
{
    return std::nullopt;
}

bool TrackPrivateBase::operator==(const TrackPrivateBase& track) const
{
    return id() == track.id()
        && label() == track.label()
        && language() == track.language()
        && trackIndex() == track.trackIndex()
        && trackUID() == track.trackUID()
        && defaultEnabled() == track.defaultEnabled()
        && startTimeVariance() == track.startTimeVariance();
}

void TrackPrivateBase::notifyClients(Task&& task)
{
    Ref sharedTask = createSharedTask<void(TrackPrivateBaseClient&)>(WTFMove(task));
    // We ensure not to hold the lock for too long by making a copy (which are cheap)
    // as we could potentially get a re-entrant call which would cause a deadlock.
    Vector<ClientRecord> clients;
    {
        Locker locker { m_lock };
        clients = m_clients;
    }
    for (auto& tuple : clients) {
        auto& [dispatcher, weakClient, mainThread] = tuple;
        if (dispatcher) {
            dispatcher->get()([weakClient = WTFMove(weakClient), sharedTask] {
                if (weakClient)
                    sharedTask->run(*weakClient);
            });
        }
    }
}

void TrackPrivateBase::notifyMainThreadClient(Task&& task)
{
    // There will only ever be one main thread client.
    // We call the first one found.
    Vector<ClientRecord> clients;
    {
        Locker locker { m_lock };
        clients = m_clients;
    }
    for (auto& tuple : clients) {
        auto& [dispatcher, weakClient, mainThread] = tuple;
        if (dispatcher && mainThread) {
            dispatcher->get()([weakClient = WTFMove(weakClient), task = WTFMove(task)] {
                if (weakClient)
                    task(*weakClient);
            });
            break;
        }
    }
}

size_t TrackPrivateBase::addClient(TrackPrivateBaseClient::Dispatcher&& dispatcher, TrackPrivateBaseClient& client)
{
    Locker locker { m_lock };
    size_t index = m_clients.size();
    m_clients.append(std::make_tuple(SharedDispatcher::create(WTFMove(dispatcher)), WeakPtr { client }, isMainThread()));
    return index;
}

void TrackPrivateBase::removeClient(uint32_t index)
{
    Locker locker { m_lock };
    if (m_clients.size() > index)
        return;
    m_clients[index] = std::make_tuple<RefPtr<SharedDispatcher>, WeakPtr<TrackPrivateBaseClient>, bool>({ }, { }, false);
}

bool TrackPrivateBase::hasClients() const
{
    Locker locker { m_lock };
    return m_clients.size();
}

bool TrackPrivateBase::hasOneClient() const
{
    Locker locker { m_lock };
    return m_clients.size() == 1;
}

#if !RELEASE_LOG_DISABLED

static uint64_t s_uniqueId = 0;

void TrackPrivateBase::setLogger(const Logger& logger, const void* logIdentifier)
{
    m_logger = &logger;
    m_logIdentifier = childLogIdentifier(logIdentifier, ++s_uniqueId);
}

WTFLogChannel& TrackPrivateBase::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif
