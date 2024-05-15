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

#include "Session.h"
#include "WebDriverService.h"
#include <algorithm>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

WebSocketServer::WebSocketServer(WebSocketMessageHandler& messageHandler, WebDriverService& service)
    : m_messageHandler(messageHandler), m_service(service)
{
}

void WebSocketServer::addToConnectionsNotAssociatedToASession(WebSocketMessageHandler::Connection connection)
{
    m_connectionsNotAssociatedToASession.push_back(connection);
}

bool WebSocketServer::isConnectionNotAssociatedToSession(WebSocketMessageHandler::Connection& connection)
{
    return std::find(m_connectionsNotAssociatedToASession.begin(), m_connectionsNotAssociatedToASession.end(), connection) != m_connectionsNotAssociatedToASession.end();
}

void WebSocketServer::removeConnectionNotAssociatedWithSession(WebSocketMessageHandler::Connection& connection)
{
    m_connectionsNotAssociatedToASession.erase(std::find(m_connectionsNotAssociatedToASession.begin(), m_connectionsNotAssociatedToASession.end(), connection));
}
void WebSocketServer::removeConnectionAssociatedToSession(WebSocketMessageHandler::Connection& connection)
{
    std::erase_if(m_connectionToSession, [&](const auto& pair) {
        return pair.first == connection;
    });
}

// FIXME we might want to move the reply message data into the created message
WebSocketMessageHandler::Message WebSocketMessageHandler::Message::createReply(const WebSocketMessageHandler::Message& replyBody) const
{
    return { connection, replyBody.data, replyBody.dataLength };
}

std::optional<RefPtr<Session>> WebSocketServer::getSessionForConnection(WebSocketMessageHandler::Connection& connection)
{
    String sessionId = nullString();

    for (const auto& pair : m_connectionToSession) {
        if (pair.first == connection) {
            sessionId = pair.second;
            break;
        }
    }

    if (sessionId.isNull())
        return std::nullopt;

    auto existingSession = m_service.session();
    if (!existingSession || (*existingSession)->id() != sessionId)
        return std::nullopt;

    return existingSession;
}

std::optional<WebSocketMessageHandler::Connection> WebSocketServer::getConnectionForSession(const String& sessionId)
{
    for (const auto& pair : m_connectionToSession) {
        if (pair.second == sessionId)
            return pair.first;
    }

    return std::nullopt;
}

String WebSocketServer::getSessionIDForWebSocketResource(const String& resource)
{
    // https://w3c.github.io/webdriver-bidi/#get-a-session-id-for-a-websocket-resource

    // To get a session ID for a WebSocket resource given resource name:
    // 1. If resource name doesn’t begin with the byte string "/session/", return null.
    if (!resource.startsWith("/session/"_s))
        return nullString();
    // 2. Let session id be the bytes in resource name following the "/session/" prefix.
    auto sessionId = resource.substring(9);

    // 3. If session id is not the string representation of a UUID, return null.
    auto uuid = WTF::UUID::parse(sessionId);
    if (!uuid || !uuid->isValid())
        return nullString();

    // 4. Return session id;
    return sessionId;
}

RefPtr<WebSocketListener> WebSocketServer::startListeningForAWebSocketConnectionGivenSession(String sessionId)
{
    WTFLogAlways("WebSocketServer::startListeningForAWebSocketConnectionGivenSession(%s)", sessionId.utf8().data());
    // https://w3c.github.io/webdriver-bidi/#start-listening-for-a-websocket-connection
    // To start listening for a WebSocket connection given a session session:

    // 1. If there is an existing WebSocket listener in active listeners which the remote end would like to reuse,
    // let listener be that listener. Otherwise let listener be a new WebSocket listener with implementation-defined
    // host, port, secure flag, and an empty list of WebSocket resources.
    // Implementation note: As we support only one session at a time, we always reuse the same listener
    auto listener = m_listener;

    // 2. Let resource name be the result of constructing a WebSocket resource name given session.
    auto resourceName = constructWebSocketResourceNameGivenSession(sessionId);

    WTFLogAlways("created resource name: %s", resourceName.utf8().data());
    // 3. Append resource name to the list of WebSocket resources for listener.
    listener->resources.push_back(resourceName);

    // 4. Append listener to the remote end's active listeners.
    // Implementation note: We don't have a list of active listeners, as we support only one session at a time.

    // 5. Return listener.
    return listener;
}

String WebSocketServer::constructWebSocketResourceNameGivenSession(String sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-resource-name

    // 1. If session is null, return "/session"
    if (sessionId.isNull())
        return "/session"_s;

    // 2. Return the result of concatenating the string "/session/" with session’s session ID.
    return "/session/"_s + sessionId;
}

String WebSocketServer::constructWebSocketURLForListenerAndSession(const RefPtr<WebSocketListener> listener, String sessionId)
{
    // https://w3c.github.io/webdriver-bidi/#construct-a-websocket-url
    // Let resource name be the result of construct a WebSocket resource name with session.
    auto resourceName = constructWebSocketResourceNameGivenSession(sessionId);

    // Return a WebSocket URI constructed with host set to listener’s host, port set to listener’s port, path set to resource name, following the wss-URI construct if listener’s secure flag is set and the ws-URL construct otherwise.
    // FIXME: Support secure flag

    auto host = listener->host;
    if (host == "all"_s)
        host = "localhost"_s;

    return "ws://"_s + host + ":" + String::number(listener->port) + resourceName;
}

WebSocketMessageHandler::Message WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode errorCode, std::optional<Connection> connection, std::optional<String> errorMessage, std::optional<String> commandId)
{
    if (!connection)
        return { };

    auto reply = JSON::Object::create();

    if (commandId)
        reply->setString("id"_s, *commandId);

    if (errorMessage)
        reply->setString("message"_s, *errorMessage);

    // FIXME Refactor httpStatusCode as a static method in CommandResult
    auto httpCommandResult = CommandResult::fail(errorCode);
    reply->setInteger("error"_s, httpCommandResult.httpStatusCode());

    auto serializedReply = reply->toJSONString().utf8().data();
    auto serializedReplyLength = strlen(serializedReply);
    return { connection, serializedReply, serializedReplyLength };
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
