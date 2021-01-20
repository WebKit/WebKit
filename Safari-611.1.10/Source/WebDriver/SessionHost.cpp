/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "SessionHost.h"

#include <wtf/text/StringBuilder.h>

namespace WebDriver {

void SessionHost::inspectorDisconnected()
{
    // Browser closed or crashed, finish all pending commands with error.
    for (auto messageID : copyToVector(m_commandRequests.keys())) {
        auto responseHandler = m_commandRequests.take(messageID);
        responseHandler({ nullptr, true });
    }
}

long SessionHost::sendCommandToBackend(const String& command, RefPtr<JSON::Object>&& parameters, Function<void (CommandResponse&&)>&& responseHandler)
{
    static long lastSequenceID = 0;
    long sequenceID = ++lastSequenceID;
    m_commandRequests.add(sequenceID, WTFMove(responseHandler));
    StringBuilder messageBuilder;
    messageBuilder.appendLiteral("{\"id\":");
    messageBuilder.appendNumber(sequenceID);
    messageBuilder.appendLiteral(",\"method\":\"Automation.");
    messageBuilder.append(command);
    messageBuilder.append('"');
    if (parameters) {
        messageBuilder.appendLiteral(",\"params\":");
        messageBuilder.append(parameters->toJSONString());
    }
    messageBuilder.append('}');
    sendMessageToBackend(messageBuilder.toString());

    return sequenceID;
}

void SessionHost::dispatchMessage(const String& message)
{
    auto messageValue = JSON::Value::parseJSON(message);
    if (!messageValue)
        return;

    auto messageObject = messageValue->asObject();
    if (!messageObject)
        return;

    auto sequenceID = messageObject->getInteger("id"_s);
    if (!sequenceID)
        return;

    auto responseHandler = m_commandRequests.take(*sequenceID);
    ASSERT(responseHandler);

    CommandResponse response;
    if (auto errorObject = messageObject->getObject("error"_s)) {
        response.responseObject = WTFMove(errorObject);
        response.isError = true;
    } else if (auto resultObject = messageObject->getObject("result"_s)) {
        if (resultObject->size())
            response.responseObject = WTFMove(resultObject);
    }

    responseHandler(WTFMove(response));
}

} // namespace WebDriver
