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
#include "RemoteInspectorServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <wtf/JSONValues.h>
#include <wtf/MainThread.h>

namespace Inspector {

void RemoteInspectorServer::addServerConnection(PlatformSocketType identifier)
{
    if (!m_server)
        return;

    if (auto id = m_server->createClient(identifier)) {
        LockHolder lock(m_connectionsLock);
        m_inspectorConnections.append(id.value());
    }
}

void RemoteInspectorServer::didAccept(ClientID clientID, RemoteInspectorSocket::DomainType type)
{
    ASSERT(!isMainThread());

    if (type == RemoteInspectorSocket::DomainType::Inet) {
        if (m_clientConnection) {
            LOG_ERROR("Inspector server can accept only 1 client");
            return;
        }
        m_clientConnection = clientID;
    } else if (type == RemoteInspectorSocket::DomainType::Unix) {
        LockHolder lock(m_connectionsLock);
        m_inspectorConnections.append(clientID);
    }
}

void RemoteInspectorServer::didClose(ClientID clientID)
{
    ASSERT(!isMainThread());

    if (clientID == m_clientConnection) {
        // Connection from the remote client closed.
        callOnMainThread([this] {
            clientConnectionClosed();
        });
        return;
    }

    // Connection from WebProcess closed.
    callOnMainThread([this, clientID] {
        connectionClosed(clientID);
    });
}

HashMap<String, RemoteInspectorConnectionClient::CallHandler>& RemoteInspectorServer::dispatchMap()
{
    static NeverDestroyed<HashMap<String, CallHandler>> dispatchMap = HashMap<String, CallHandler>({
        {"SetTargetList"_s, static_cast<CallHandler>(&RemoteInspectorServer::setTargetList)},
        {"SetupInspectorClient"_s, static_cast<CallHandler>(&RemoteInspectorServer::setupInspectorClient)},
        {"Setup"_s, static_cast<CallHandler>(&RemoteInspectorServer::setup)},
        {"FrontendDidClose"_s, static_cast<CallHandler>(&RemoteInspectorServer::close)},
        {"SendMessageToFrontend"_s, static_cast<CallHandler>(&RemoteInspectorServer::sendMessageToFrontend)},
        {"SendMessageToBackend"_s, static_cast<CallHandler>(&RemoteInspectorServer::sendMessageToBackend)},
    });

    return dispatchMap;
}

void RemoteInspectorServer::sendWebInspectorEvent(ClientID clientID, const String& event)
{
    const CString message = event.utf8();
    m_server->send(clientID, reinterpret_cast<const uint8_t*>(message.data()), message.length());
}

RemoteInspectorServer& RemoteInspectorServer::singleton()
{
    static NeverDestroyed<RemoteInspectorServer> server;
    return server;
}

bool RemoteInspectorServer::start(uint16_t port)
{
    m_server = RemoteInspectorSocketServer::create(this);

    if (!m_server->listenInet(port)) {
        m_server = nullptr;
        return false;
    }

    return true;
}

void RemoteInspectorServer::setTargetList(const Event& event)
{
    ASSERT(isMainThread());

    if (!m_clientConnection || !event.message)
        return;

    auto targetListEvent = JSON::Object::create();
    targetListEvent->setString("event"_s, "SetTargetList"_s);
    targetListEvent->setInteger("connectionID"_s, event.clientID);
    targetListEvent->setString("message"_s, event.message.value());
    sendWebInspectorEvent(m_clientConnection.value(), targetListEvent->toJSONString());
}

void RemoteInspectorServer::setupInspectorClient(const Event&)
{
    ASSERT(isMainThread());

    auto setupEvent = JSON::Object::create();
    setupEvent->setString("event"_s, "GetTargetList"_s);

    LockHolder lock(m_connectionsLock);
    for (auto connection : m_inspectorConnections)
        sendWebInspectorEvent(connection, setupEvent->toJSONString());
}

void RemoteInspectorServer::setup(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.targetID || !event.connectionID)
        return;

    m_inspectionTargets.add(std::make_pair(event.connectionID.value(), event.targetID.value()));

    auto setupEvent = JSON::Object::create();
    setupEvent->setString("event"_s, "Setup"_s);
    setupEvent->setInteger("targetID"_s, event.targetID.value());
    sendWebInspectorEvent(event.connectionID.value(), setupEvent->toJSONString());
}

void RemoteInspectorServer::sendCloseEvent(uint64_t connectionID, uint64_t targetID)
{
    ASSERT(isMainThread());

    auto closeEvent = JSON::Object::create();
    closeEvent->setString("event"_s, "FrontendDidClose"_s);
    closeEvent->setInteger("targetID"_s, targetID);
    sendWebInspectorEvent(connectionID, closeEvent->toJSONString());
}

void RemoteInspectorServer::close(const Event& event)
{
    ASSERT(isMainThread());

    sendCloseEvent(event.connectionID.value(), event.targetID.value());
    m_inspectionTargets.remove(std::make_pair(event.connectionID.value(), event.targetID.value()));
}

void RemoteInspectorServer::clientConnectionClosed()
{
    ASSERT(isMainThread());

    for (auto connectionTargetPair : m_inspectionTargets)
        sendCloseEvent(connectionTargetPair.first, connectionTargetPair.second);

    m_inspectionTargets.clear();
    m_clientConnection = WTF::nullopt;
}

void RemoteInspectorServer::connectionClosed(uint64_t clientID)
{
    ASSERT(isMainThread());

    LockHolder lock(m_connectionsLock);
    if (m_inspectorConnections.removeFirst(clientID) && m_clientConnection) {
        auto closedEvent = JSON::Object::create();
        closedEvent->setString("event"_s, "SetTargetList"_s);
        closedEvent->setInteger("connectionID"_s, clientID);
        auto targetList = JSON::Array::create();
        closedEvent->setString("message"_s, targetList->toJSONString());
        sendWebInspectorEvent(m_clientConnection.value(), closedEvent->toJSONString());
    }
}

void RemoteInspectorServer::sendMessageToBackend(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.connectionID || !event.targetID || !event.message)
        return;

    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_s, "SendMessageToTarget"_s);
    sendEvent->setInteger("targetID"_s, event.targetID.value());
    sendEvent->setString("message"_s, event.message.value());
    sendWebInspectorEvent(event.connectionID.value(), sendEvent->toJSONString());
}

void RemoteInspectorServer::sendMessageToFrontend(const Event& event)
{
    if (!m_clientConnection)
        return;

    ASSERT(isMainThread());

    if (!event.targetID || !event.message)
        return;

    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_s, "SendMessageToFrontend"_s);
    sendEvent->setInteger("targetID"_s, event.targetID.value());
    sendEvent->setInteger("connectionID"_s, event.clientID);
    sendEvent->setString("message"_s, event.message.value());
    sendWebInspectorEvent(m_clientConnection.value(), sendEvent->toJSONString());
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
