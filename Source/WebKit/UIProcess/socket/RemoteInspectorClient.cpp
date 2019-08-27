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
#include "RemoteInspectorClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteWebInspectorProxy.h"
#include <wtf/MainThread.h>

namespace WebKit {

class RemoteInspectorProxy final : public RemoteWebInspectorProxyClient {
    WTF_MAKE_FAST_ALLOCATED();
public:
    RemoteInspectorProxy(RemoteInspectorClient& inspectorClient, ConnectionID connectionID, TargetID targetID, const String& type)
        : m_proxy(RemoteWebInspectorProxy::create())
        , m_inspectorClient(inspectorClient)
        , m_connectionID(connectionID)
        , m_targetID(targetID)
        , m_debuggableType(type)
    {
        m_proxy->setClient(this);
    }

    ~RemoteInspectorProxy()
    {
        m_proxy->setClient(nullptr);
        m_proxy->invalidate();
    }

    void load()
    {
        m_proxy->load(m_debuggableType, String());
    }

    void show()
    {
        m_proxy->show();
    }

    void sendMessageToFrontend(const String& message)
    {
        m_proxy->sendMessageToFrontend(message);
    }

    void sendMessageToBackend(const String& message) override
    {
        m_inspectorClient.sendMessageToBackend(m_connectionID, m_targetID, message);
    }

    void closeFromFrontend() override
    {
        m_inspectorClient.closeFromFrontend(m_connectionID, m_targetID);
    }

private:
    Ref<RemoteWebInspectorProxy> m_proxy;
    RemoteInspectorClient& m_inspectorClient;
    ConnectionID m_connectionID;
    TargetID m_targetID;
    String m_debuggableType;
};

RemoteInspectorClient::RemoteInspectorClient(const char* address, unsigned port, RemoteInspectorObserver& observer)
    : m_observer(observer)
{
    m_connectionID = connectInet(address, port);
    if (!m_connectionID) {
        LOG_ERROR("Inspector client could not connect to %s:%d", address, port);
        return;
    }

    startInitialCommunication();
}

RemoteInspectorClient::~RemoteInspectorClient()
{
}

void RemoteInspectorClient::sendWebInspectorEvent(const String& event)
{
    ASSERT(isMainThread());
    ASSERT(m_connectionID.hasValue());
    auto message = event.utf8();
    send(m_connectionID.value(), reinterpret_cast<const uint8_t*>(message.data()), message.length());
}

HashMap<String, Inspector::RemoteInspectorConnectionClient::CallHandler>& RemoteInspectorClient::dispatchMap()
{
    static NeverDestroyed<HashMap<String, CallHandler>> dispatchMap = HashMap<String, CallHandler>({
        { "SetTargetList"_s, static_cast<CallHandler>(&RemoteInspectorClient::setTargetList) },
        { "SendMessageToFrontend"_s, static_cast<CallHandler>(&RemoteInspectorClient::sendMessageToFrontend) },
    });

    return dispatchMap;
}

void RemoteInspectorClient::startInitialCommunication()
{
    auto event = JSON::Object::create();
    event->setString("event"_s, "SetupInspectorClient"_s);
    sendWebInspectorEvent(event->toJSONString());
}

void RemoteInspectorClient::connectionClosed()
{
    m_targets.clear();
    m_inspectorProxyMap.clear();
    m_observer.connectionClosed(*this);
}

void RemoteInspectorClient::didClose(ConnectionID)
{
}

void RemoteInspectorClient::inspect(ConnectionID connectionID, TargetID targetID, const String& type)
{
    auto addResult = m_inspectorProxyMap.ensure(std::make_pair(connectionID, targetID), [this, connectionID, targetID, &type] {
        return makeUnique<RemoteInspectorProxy>(*this, connectionID, targetID, type);
    });

    if (!addResult.isNewEntry) {
        addResult.iterator->value->show();
        return;
    }

    auto setupEvent = JSON::Object::create();
    setupEvent->setString("event"_s, "Setup"_s);
    setupEvent->setInteger("connectionID"_s, connectionID);
    setupEvent->setInteger("targetID"_s, targetID);
    sendWebInspectorEvent(setupEvent->toJSONString());

    addResult.iterator->value->load();
}

void RemoteInspectorClient::sendMessageToBackend(ConnectionID connectionID, TargetID targetID, const String& message)
{
    auto backendEvent = JSON::Object::create();
    backendEvent->setString("event"_s, "SendMessageToBackend"_s);
    backendEvent->setInteger("connectionID"_s, connectionID);
    backendEvent->setInteger("targetID"_s, targetID);
    backendEvent->setString("message"_s, message);
    sendWebInspectorEvent(backendEvent->toJSONString());
}

void RemoteInspectorClient::closeFromFrontend(ConnectionID connectionID, TargetID targetID)
{
    auto closedEvent = JSON::Object::create();
    closedEvent->setString("event"_s, "FrontendDidClose"_s);
    closedEvent->setInteger("connectionID"_s, connectionID);
    closedEvent->setInteger("targetID"_s, targetID);
    sendWebInspectorEvent(closedEvent->toJSONString());

    m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));
}

void RemoteInspectorClient::setTargetList(const Event& event)
{
    if (!event.connectionID || !event.message)
        return;

    RefPtr<JSON::Value> messageValue;
    if (!JSON::Value::parseJSON(event.message.value(), messageValue))
        return;

    RefPtr<JSON::Array> messageArray;
    if (!messageValue->asArray(messageArray))
        return;

    Vector<Target> targetList;
    for (auto& itemValue : *messageArray) {
        RefPtr<JSON::Object> itemObject;
        if (!itemValue->asObject(itemObject))
            continue;

        Target target;
        if (!itemObject->getInteger("targetID"_s, target.id)
            || !itemObject->getString("name"_s, target.name)
            || !itemObject->getString("url"_s, target.url)
            || !itemObject->getString("type"_s, target.type))
            continue;

        targetList.append(WTFMove(target));
    }

    auto connectionID = event.connectionID.value();

    // Find closed targets to remove them.
    Vector<TargetID, 4> targetsToRemove;
    for (auto& pair : m_inspectorProxyMap.keys()) {
        if (pair.first != connectionID)
            continue;

        bool found = false;
        for (auto& target : targetList) {
            if (target.id == pair.second) {
                found = true;
                break;
            }
        }

        if (!found)
            targetsToRemove.append(pair.second);
    }
    for (auto& targetID : targetsToRemove)
        m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));

    m_targets.set(connectionID, WTFMove(targetList));
    m_observer.targetListChanged(*this);
}

void RemoteInspectorClient::sendMessageToFrontend(const Event& event)
{
    if (!event.connectionID || !event.targetID || !event.message)
        return;

    auto proxy = m_inspectorProxyMap.get(std::make_pair(event.connectionID.value(), event.targetID.value()));
    if (!proxy) {
        ASSERT_NOT_REACHED();
        return;
    }

    proxy->sendMessageToFrontend(event.message.value());
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
