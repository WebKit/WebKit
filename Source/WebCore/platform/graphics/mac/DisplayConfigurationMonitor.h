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

#pragma once

#if PLATFORM(MAC)

#include <CoreGraphics/CGDisplayConfiguration.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Vector.h>

namespace WebCore {

enum DisplayConfigurationClientIdentifierType { };
using DisplayConfigurationClientIdentifier = ObjectIdentifier<DisplayConfigurationClientIdentifierType>;

class DisplayConfigurationMonitor {
public:
    ~DisplayConfigurationMonitor();
    WEBCORE_EXPORT static DisplayConfigurationMonitor& singleton();

    class Client {
    public:
        Client();
        virtual ~Client();
        virtual void displayWasReconfigured() = 0;

        DisplayConfigurationClientIdentifier m_identifier;
    };
    // If a SerialFunctionDispatcher is provided, then displayWasReconfigured will
    // be dispatched there. The dispatcher's lifetime must be equal to or longer than
    // the client, and both add/remove must be called while the dispatcher is current.
    void addClient(Client&, SerialFunctionDispatcher* = nullptr);
    void removeClient(Client&);

    WEBCORE_EXPORT void dispatchDisplayWasReconfigured(CGDisplayChangeSummaryFlags);
private:
    DisplayConfigurationMonitor();
    Lock m_lock;
    struct ClientEntry {
        Client* m_client;
        SerialFunctionDispatcher* m_dispatcher { nullptr };

        bool operator==(const ClientEntry& other) const
        {
            return m_client == other.m_client;
        }
    };
    Vector<ClientEntry> m_clients WTF_GUARDED_BY_LOCK(m_lock);
    friend NeverDestroyed<DisplayConfigurationMonitor>;
};

}
#endif
