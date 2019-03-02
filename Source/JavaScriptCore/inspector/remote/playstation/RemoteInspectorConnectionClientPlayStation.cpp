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

#include "RemoteInspectorSocket.h"
#include <wtf/JSONValues.h>
#include <wtf/RunLoop.h>

namespace Inspector {

void RemoteInspectorConnectionClient::didReceiveWebInspectorEvent(ClientID clientID, Vector<uint8_t>&& data)
{
    ASSERT(!isMainThread());

    if (data.isEmpty())
        return;

    String jsonData = String::fromUTF8(data);

    RefPtr<JSON::Value> messageValue;
    if (!JSON::Value::parseJSON(jsonData, messageValue))
        return;

    RefPtr<JSON::Object> messageObject;
    if (!messageValue->asObject(messageObject))
        return;

    String methodName;
    if (!messageObject->getString("event"_s, methodName))
        return;

    struct Event event;
    event.clientID = clientID;

    uint64_t connectionID;
    if (messageObject->getInteger("connectionID"_s, connectionID))
        event.connectionID = connectionID;

    uint64_t targetID;
    if (messageObject->getInteger("targetID"_s, targetID))
        event.targetID = targetID;

    String message;
    if (messageObject->getString("message"_s, message))
        event.message = message;

    RunLoop::main().dispatch([this, methodName, event = WTFMove(event)] {
        auto& methods = dispatchMap();
        if (methods.contains(methodName)) {
            auto call = methods.get(methodName);
            (this->*call)(event);
        } else
            LOG_ERROR("Unknown event: %s", methodName.utf8().data());
    });
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
