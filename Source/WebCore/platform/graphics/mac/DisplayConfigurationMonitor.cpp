/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "DisplayConfigurationMonitor.h"

#if PLATFORM(MAC)

#include <wtf/MainThread.h>

namespace WebCore {

DisplayConfigurationMonitor::Client::Client()
{
    m_identifier = DisplayConfigurationClientIdentifier::generateThreadSafe();
}

DisplayConfigurationMonitor::Client::~Client() = default;

DisplayConfigurationMonitor::DisplayConfigurationMonitor() = default;

DisplayConfigurationMonitor::~DisplayConfigurationMonitor() = default;

DisplayConfigurationMonitor& DisplayConfigurationMonitor::singleton()
{
    static NeverDestroyed<DisplayConfigurationMonitor> instance;
    return instance;
}

void DisplayConfigurationMonitor::addClient(Client& client, SerialFunctionDispatcher* dispatcher)
{
    Locker locker { m_lock };
    if (dispatcher)
        assertIsCurrent(*dispatcher);
    else
        assertIsMainThread();
    ASSERT(!m_clients.contains(ClientEntry { &client }));
    m_clients.append({ &client, dispatcher });
}

void DisplayConfigurationMonitor::removeClient(Client& client)
{
    Locker locker { m_lock };
    size_t index = m_clients.find(ClientEntry { &client });
    ASSERT(index != notFound);
    if (m_clients.at(index).m_dispatcher)
        assertIsCurrent(*m_clients.at(index).m_dispatcher);
    else
        assertIsMainThread();
    m_clients.remove(index);
}

void DisplayConfigurationMonitor::dispatchDisplayWasReconfigured(CGDisplayChangeSummaryFlags flags)
{
    if (!(flags & kCGDisplaySetModeFlag))
        return;

    Vector<ClientEntry> copy;
    {
        Locker locker { m_lock };
        copy = copyToVector(m_clients);

        // Dispatch notifications for Clients on other threads while we hold the lock, to
        // ensure the dispatcher is still alive. Once on the other dispatcher, we'll
        // re-check the validity of the client and then notify without the lock hold.
        for (auto& [client, dispatcher] : copy) {
            if (dispatcher) {
                dispatcher->dispatch([this, client = client, ident = client->m_identifier]() {
                    {
                        Locker locker { m_lock };
                        size_t index = m_clients.find(ClientEntry { client });
                        if (index == notFound)
                            return;
                        if (m_clients.at(index).m_client->m_identifier != ident)
                            return;
                    }
                    client->displayWasReconfigured();
                });
            }
        }
    }

    // Notify the main-thread Clients without the lock held.
    for (auto& [client, dispatcher] : copy) {
        if (!dispatcher)
            client->displayWasReconfigured();
    }
}

}

#endif
