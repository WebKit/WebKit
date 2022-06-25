/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteControllableTarget.h"
#include "RemoteInspectorMessageParser.h"
#include "RemoteInspectorSocketEndpoint.h"
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/Lock.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class MessageParser;

class JS_EXPORT_PRIVATE RemoteInspectorConnectionClient : public RemoteInspectorSocketEndpoint::Client {
public:
    ~RemoteInspectorConnectionClient() override;

    std::optional<ConnectionID> connectInet(const char* serverAddr, uint16_t serverPort);
    std::optional<ConnectionID> createClient(PlatformSocketType);
    void send(ConnectionID, const uint8_t* data, size_t);

    void didReceive(RemoteInspectorSocketEndpoint&, ConnectionID, Vector<uint8_t>&&) final;

    struct Event {
        String methodName;
        ConnectionID clientID { };
        std::optional<ConnectionID> connectionID;
        std::optional<TargetID> targetID;
        std::optional<String> message;
    };

    using CallHandler = void (RemoteInspectorConnectionClient::*)(const Event&);
    virtual HashMap<String, CallHandler>& dispatchMap() = 0;

protected:
    std::optional<Vector<Ref<JSON::Object>>> parseTargetListJSON(const String&);

    static std::optional<Event> extractEvent(ConnectionID, Vector<uint8_t>&&);

    HashMap<ConnectionID, MessageParser> m_parsers WTF_GUARDED_BY_LOCK(m_parsersLock);
    Lock m_parsersLock;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
