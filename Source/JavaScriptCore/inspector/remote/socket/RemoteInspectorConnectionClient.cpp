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

#include "config.h"
#include "RemoteInspectorConnectionClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorSocketEndpoint.h"
#include <wtf/JSONValues.h>
#include <wtf/RunLoop.h>

namespace Inspector {

RemoteInspectorConnectionClient::~RemoteInspectorConnectionClient()
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    endpoint.invalidateClient(*this);
}

Optional<ConnectionID> RemoteInspectorConnectionClient::connectInet(const char* serverAddr, uint16_t serverPort)
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    return endpoint.connectInet(serverAddr, serverPort, *this);
}

Optional<ConnectionID> RemoteInspectorConnectionClient::listenInet(const char* address, uint16_t port)
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    return endpoint.listenInet(address, port, *this);
}

Optional<ConnectionID> RemoteInspectorConnectionClient::createClient(PlatformSocketType socket)
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    return endpoint.createClient(socket, *this);
}

void RemoteInspectorConnectionClient::send(ConnectionID id, const uint8_t* data, size_t size)
{
    auto message = MessageParser::createMessage(data, size);
    if (message.isEmpty())
        return;

    auto& endpoint = RemoteInspectorSocketEndpoint::singleton();
    endpoint.send(id, message.data(), message.size());
}

void RemoteInspectorConnectionClient::didReceive(ConnectionID clientID, Vector<uint8_t>&& data)
{
    ASSERT(!isMainThread());

    LockHolder lock(m_parsersLock);
    auto result = m_parsers.ensure(clientID, [this, clientID] {
        return MessageParser([this, clientID](Vector<uint8_t>&& data) {
            if (auto event = RemoteInspectorConnectionClient::extractEvent(clientID, WTFMove(data))) {
                RunLoop::main().dispatch([this, event = WTFMove(*event)] {
                    const auto& methodName = event.methodName;
                    auto& methods = dispatchMap();
                    if (methods.contains(methodName)) {
                        auto call = methods.get(methodName);
                        (this->*call)(event);
                    } else
                        LOG_ERROR("Unknown event: %s", methodName.utf8().data());
                });
            }
        });
    });
    result.iterator->value.pushReceivedData(data.data(), data.size());
}

Optional<RemoteInspectorConnectionClient::Event> RemoteInspectorConnectionClient::extractEvent(ConnectionID clientID, Vector<uint8_t>&& data)
{
    if (data.isEmpty())
        return WTF::nullopt;

    String jsonData = String::fromUTF8(data);

    RefPtr<JSON::Value> messageValue;
    if (!JSON::Value::parseJSON(jsonData, messageValue))
        return WTF::nullopt;

    RefPtr<JSON::Object> messageObject;
    if (!messageValue->asObject(messageObject))
        return WTF::nullopt;

    Event event;
    if (!messageObject->getString("event"_s, event.methodName))
        return WTF::nullopt;

    event.clientID = clientID;

    ConnectionID connectionID;
    if (messageObject->getInteger("connectionID"_s, connectionID))
        event.connectionID = connectionID;

    TargetID targetID;
    if (messageObject->getInteger("targetID"_s, targetID))
        event.targetID = targetID;

    String message;
    if (messageObject->getString("message"_s, message))
        event.message = message;

    return event;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
