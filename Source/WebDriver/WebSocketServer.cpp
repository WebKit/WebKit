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
#include "Session.h"
#include "WebDriverService.h"
#include <algorithm>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

WebSocketServer::WebSocketServer(WebSocketMessageHandler& messageHandler, WebDriverService& service)
    : m_messageHandler(messageHandler)
    , m_service(service)
{
}

void WebSocketServer::addStaticConnection(WebSocketMessageHandler::Connection&& connection)
{
    m_staticConnections.push_back(WTFMove(connection));
}

void WebSocketServer::addConnection(WebSocketMessageHandler::Connection&& connection, const String& sessionId)
{
    m_connectionToSession.add(WTFMove(connection), sessionId);
}

bool WebSocketServer::isStaticConnection(const WebSocketMessageHandler::Connection& connection)
{
    return std::count(m_staticConnections.begin(), m_staticConnections.end(), connection);
}

void WebSocketServer::removeStaticConnection(const WebSocketMessageHandler::Connection& connection)
{
    m_staticConnections.erase(std::find(m_staticConnections.begin(), m_staticConnections.end(), connection));
}

void WebSocketServer::removeConnection(const WebSocketMessageHandler::Connection& connection)
{
    auto it = m_connectionToSession.find(connection);
    if (it != m_connectionToSession.end())
        m_connectionToSession.remove(it);
}

RefPtr<Session> WebSocketServer::session(const WebSocketMessageHandler::Connection& connection)
{
    String sessionId;

    for (const auto& pair : m_connectionToSession) {
        if (pair.key == connection) {
            sessionId = pair.value;
            break;
        }
    }

    if (sessionId.isNull())
        return { };

    const auto& existingSession = m_service.session();
    if (!existingSession || (existingSession->id() != sessionId))
        return { };

    return existingSession;
}

std::optional<WebSocketMessageHandler::Connection> WebSocketServer::connection(const String& sessionId)
{
    for (auto& pair : m_connectionToSession) {
        if (pair.value == sessionId)
            return pair.key;
    }

    return { };
}

String WebSocketServer::getSessionID(const String& resource)
{
    // https://w3c.github.io/webdriver-bidi/#get-a-session-id-for-a-websocket-resource

    constexpr auto sessionPrefix = "/session/"_s;
    if (!resource.startsWith(sessionPrefix))
        return nullString();

    auto sessionId = resource.substring(sessionPrefix.length());
    auto uuid = WTF::UUID::parse(sessionId);
    if (!uuid || !uuid->isValid())
        return nullString();

    return sessionId;
}

RefPtr<WebSocketListener> WebSocketServer::startListening(const String& sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#start-listening-for-a-websocket-connection
    // FIXME Add more listeners when start supporting multiple sessions
    auto resourceName = getResourceName(sessionId);
    m_listener->resources.push_back(resourceName);
    return m_listener;
}

String WebSocketServer::getResourceName(const String& sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-resource-name
    if (sessionId.isNull())
        return "/session"_s;

    return makeString("/session/"_s, sessionId);
}

String WebSocketServer::getWebSocketURL(const RefPtr<WebSocketListener> listener, const String& sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-url
    // FIXME: Support secure flag
    // https://bugs.webkit.org/show_bug.cgi?id=280680

    auto resourceName = getResourceName(sessionId);

    auto host = listener->host;
    if (host == "all"_s)
        host = "localhost"_s;

    return makeString("ws://"_s, host, ":"_s, String::number(listener->port), resourceName);
}

void WebSocketServer::removeResourceForSession(const String& sessionId)
{
    auto resourceName = getResourceName(sessionId);
    m_listener->resources.erase(std::remove(m_listener->resources.begin(), m_listener->resources.end(), resourceName), m_listener->resources.end());
}

WebSocketMessageHandler::Message WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode errorCode, std::optional<Connection> connection, std::optional<String> errorMessage, std::optional<int> commandId)
{
    if (!connection)
        return { };

    auto reply = JSON::Object::create();

    if (commandId)
        reply->setInteger("id"_s, *commandId);

    if (errorMessage)
        reply->setString("message"_s, *errorMessage);

    reply->setInteger("error"_s, CommandResult::errorCodeToHTTPStatusCode(errorCode));

    return { *connection, reply->toJSONString().utf8() };
}

WebSocketMessageHandler::Message WebSocketMessageHandler::Message::reply(const String& type, unsigned id, Ref<JSON::Value>&& result)
{
    auto reply = JSON::Object::create();
    reply->setString("type"_s, type);
    reply->setInteger("id"_s, id);

    if (auto resultObject = result->asObject())
        reply->setObject("result"_s, resultObject.releaseNonNull());
    else
        reply->setObject("result"_s, JSON::Object::create());

    // Connection will be set when sending the message back to the client, from the incoming message.
    return { nullptr, reply->toJSONString().utf8() };
}

std::optional<Command> Command::fromData(const char* data, size_t dataLength)
{
    auto messageValue = JSON::Value::parseJSON(String::fromUTF8(std::span<const char>(data, dataLength)));
    if (!messageValue)
        return std::nullopt;

    auto messageObject = messageValue->asObject();
    if (!messageObject)
        return std::nullopt;

    auto id = messageObject->getString("id"_s);
    if (!id)
        return std::nullopt;

    auto method = messageObject->getString("method"_s);
    if (!method)
        return std::nullopt;

    auto params = messageObject->getObject("params"_s);
    if (!params)
        return std::nullopt;

    Command command {
        .id = id,
        .method = method,
        .parameters = params
    };

    return command;
}


} // namespace WebDriver
