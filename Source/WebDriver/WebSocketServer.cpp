/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WebSocketServer.h"

#include "CommandResult.h"
#include "Logging.h"
#include "Session.h"
#include "WebDriverService.h"
#include <algorithm>
#include <optional>
#include <ranges>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

WebSocketServer::WebSocketServer(WebSocketMessageHandler& messageHandler)
    : m_messageHandler(messageHandler)
{
}

void WebSocketServer::addStaticConnection(WebSocketMessageHandler::Connection&& connection)
{
    m_staticConnections.push_back(WTF::move(connection));
}

void WebSocketServer::addConnection(WebSocketMessageHandler::Connection&& connection, StringImpl* sessionId)
{
    m_connectionToSession.add(WTF::move(connection), sessionId);
}

bool WebSocketServer::isStaticConnection(const WebSocketMessageHandler::Connection& connection)
{
    return std::ranges::count(m_staticConnections, connection);
}

void WebSocketServer::removeStaticConnection(const WebSocketMessageHandler::Connection& connection)
{
    m_staticConnections.erase(std::ranges::find(m_staticConnections, connection));
}

void WebSocketServer::removeConnection(const WebSocketMessageHandler::Connection& connection)
{
    auto it = m_connectionToSession.find(connection);
    if (it != m_connectionToSession.end())
        m_connectionToSession.remove(it);
}

RefPtr<StringImpl> WebSocketServer::sessionID(const WebSocketMessageHandler::Connection& connection) const
{
    for (const auto& pair : m_connectionToSession) {
        if (pair.key == connection)
            return pair.value;
    }

    return { };
}

std::optional<WebSocketMessageHandler::Connection> WebSocketServer::connection(StringImpl* sessionId)
{
    for (auto& pair : m_connectionToSession) {
        if (equal(pair.value, sessionId))
            return pair.key;
    }

    return { };
}

RefPtr<StringImpl> WebSocketServer::getSessionID(StringImpl* resource)
{
    // https://w3c.github.io/webdriver-bidi/#get-a-session-id-for-a-websocket-resource

    constexpr auto sessionPrefix = "/session/"_s;
    if (!startsWith(resource, sessionPrefix))
        return nullString();

    auto sessionId = substring(resource, sessionPrefix.length());
    auto uuid = WTF::UUID::parse(sessionId);
    if (!uuid || !uuid->isValid())
        return nullString();

    return sessionId;
}

RefPtr<WebSocketListener> WebSocketServer::startListening(StringImpl* sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#start-listening-for-a-websocket-connection
    // FIXME Add more listeners when start supporting multiple sessions
    auto resourceName = getResourceName(sessionId);
    m_listener->resources.push_back(resourceName);
    return m_listener;
}

RefPtr<StringImpl> WebSocketServer::getResourceName(StringImpl* sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-resource-name
    if (!sessionId)
        return "/session"_si;

    return makeString("/session/"_s, sessionId);
}

RefPtr<StringImpl> WebSocketServer::getWebSocketURL(const RefPtr<WebSocketListener> listener, StringImpl* sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-url
    // FIXME: Support secure flag
    // https://bugs.webkit.org/show_bug.cgi?id=280680

    auto resourceName = getResourceName(sessionId);

    auto host = listener->host;
    if (equal(host, "all"_s))
        host = "localhost"_si;

    return makeString("ws://"_s, host, ":"_s, makeString(listener->port), resourceName);
}

void WebSocketServer::removeResourceForSession(StringImpl* sessionId)
{
    auto resourceName = getResourceName(sessionId);
    std::erase_if(m_listener->resources, [&](auto& resource) {
        return equal(resource, resourceName);
    });
}

WebSocketMessageHandler::Message WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode errorCode, std::optional<Connection> connection, std::optional<RefPtr<StringImpl>> errorMessage, std::optional<int> commandId)
{
    auto reply = JSON::Object::create();

    if (commandId)
        reply->setInteger("id"_si, *commandId);

    if (errorMessage)
        reply->setString("message"_si, *errorMessage);

    reply->setString("error"_si, CommandResult::errorCodeToString(errorCode));
    reply->setString("type"_si, "error"_si);

    return { (connection ? (*connection) : nullptr), utf8(reply->toJSONString()) };
}

WebSocketMessageHandler::Message WebSocketMessageHandler::Message::reply(StringImpl* type, unsigned id, Ref<JSON::Value>&& result)
{
    auto reply = JSON::Object::create();
    reply->setString("type"_si, type);
    reply->setInteger("id"_si, id);

    if (auto resultObject = result->asObject())
        reply->setObject("result"_si, resultObject.releaseNonNull());
    else
        reply->setObject("result"_si, JSON::Object::create());

    // Connection will be set when sending the message back to the client, from the incoming message.
    return { nullptr, utf8(reply->toJSONString()) };
}

std::optional<Command> Command::fromData(const char* data, size_t dataLength)
{
    auto messageValue = JSON::Value::parseJSON(StringImpl::fromUTF8(std::span<const char>(data, dataLength)));
    if (!messageValue)
        return std::nullopt;

    auto messageObject = messageValue->asObject();
    if (!messageObject)
        return std::nullopt;

    auto id = messageObject->getString("id"_si);
    if (!id)
        return std::nullopt;

    auto method = messageObject->getString("method"_si);
    if (!method)
        return std::nullopt;

    auto params = messageObject->getObject("params"_si);
    if (!params)
        return std::nullopt;

    Command command {
        .id = id,
        .method = method,
        .parameters = params
    };

    return command;
}

void WebSocketServer::sendMessage(StringImpl* sessionId, StringImpl* message)
{
    auto connection = this->connection(sessionId);
    if (!connection) {
        RELEASE_LOG_ERROR(WebDriverBiDi, "No connection found for session %s when trying to send message: %s", utf8(sessionId), utf8(message));
        return;
    }
    sendMessage(*connection, message);
}

void WebSocketServer::sendErrorResponse(StringImpl* sessionId, std::optional<unsigned> commandId, CommandResult::ErrorCode errorCode, StringImpl* errorMessage, std::optional<RefPtr<StringImpl>> stacktrace)
{
    auto connection = this->connection(sessionId);
    if (!connection) {
        RELEASE_LOG_ERROR(WebDriverBiDi, "No connection found for session %s when trying to send error response", utf8(sessionId));
        return;
    }
    sendErrorResponse(*connection, commandId, errorCode, errorMessage, stacktrace);
}

void WebSocketServer::sendErrorResponse(WebSocketMessageHandler::Connection connection, std::optional<unsigned> commandId, CommandResult::ErrorCode errorCode, StringImpl* errorMessage, std::optional<RefPtr<StringImpl>> stacktrace)
{
    auto errorObject = JSON::Object::create();

    errorObject->setString("type"_si, "error"_si);
    errorObject->setString("error"_si, CommandResult::errorCodeToString(errorCode));
    errorObject->setString("message"_si, errorMessage);
    if (commandId)
        errorObject->setInteger("id"_si, *commandId); // FIXME change to JSON::Value
    if (stacktrace)
        errorObject->setString("stacktrace"_si, *stacktrace);

    sendMessage(connection, errorObject->toJSONString());
}


} // namespace WebDriver
