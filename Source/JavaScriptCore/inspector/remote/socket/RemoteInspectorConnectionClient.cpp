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

std::optional<ConnectionID> RemoteInspectorConnectionClient::connectInet(const char* serverAddr, uint16_t serverPort)
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    return endpoint.connectInet(serverAddr, serverPort, *this);
}

std::optional<ConnectionID> RemoteInspectorConnectionClient::createClient(PlatformSocketType socket)
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

void RemoteInspectorConnectionClient::didReceive(RemoteInspectorSocketEndpoint&, ConnectionID clientID, Vector<uint8_t>&& data)
{
    ASSERT(!isMainThread());

    Locker locker { m_parsersLock };
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

std::optional<RemoteInspectorConnectionClient::Event> RemoteInspectorConnectionClient::extractEvent(ConnectionID clientID, Vector<uint8_t>&& data)
{
    if (data.isEmpty())
        return std::nullopt;

    String jsonData = String::fromUTF8(data);

    auto messageValue = JSON::Value::parseJSON(jsonData);
    if (!messageValue)
        return std::nullopt;

    auto messageObject = messageValue->asObject();
    if (!messageObject)
        return std::nullopt;

    Event event;

    event.methodName = messageObject->getString("event"_s);
    if (!event.methodName)
        return std::nullopt;

    event.clientID = clientID;

    if (auto connectionID = messageObject->getInteger("connectionID"_s))
        event.connectionID = *connectionID;

    if (auto targetID = messageObject->getInteger("targetID"_s))
        event.targetID = *targetID;

    event.message = messageObject->getString("message"_s);

    return event;
}

std::optional<Vector<Ref<JSON::Object>>> RemoteInspectorConnectionClient::parseTargetListJSON(const String& message)
{
    auto messageValue = JSON::Value::parseJSON(message);
    if (!messageValue)
        return std::nullopt;

    auto messageArray = messageValue->asArray();
    if (!messageArray)
        return std::nullopt;

    Vector<Ref<JSON::Object>> targetList;
    for (auto& itemValue : *messageArray) {
        if (auto itemObject = itemValue->asObject())
            targetList.append(itemObject.releaseNonNull());
        else
            LOG_ERROR("Invalid type of value in targetList. It must be object.");
    }
    return targetList;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
