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
#include "RemoteInspector.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteAutomationTarget.h"
#include "RemoteConnectionToTarget.h"
#include "RemoteInspectionTarget.h"
#include <wtf/JSONValues.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace Inspector {

PlatformSocketType RemoteInspector::s_connectionIdentifier = INVALID_SOCKET_VALUE;
uint16_t RemoteInspector::s_serverPort = 0;

RemoteInspector& RemoteInspector::singleton()
{
    static NeverDestroyed<RemoteInspector> shared;
    return shared;
}

RemoteInspector::RemoteInspector()
{
    Socket::init();
}

void RemoteInspector::didClose(ConnectionID id)
{
    if (id != m_clientID.value())
        return;

    RunLoop::current().dispatch([=] {
        LockHolder lock(m_mutex);
        stopInternal(StopSource::API);
    });
}

HashMap<String, RemoteInspector::CallHandler>& RemoteInspector::dispatchMap()
{
    static NeverDestroyed<HashMap<String, CallHandler>> methods = HashMap<String, CallHandler>({
        { "GetTargetList"_s, static_cast<CallHandler>(&RemoteInspector::receivedGetTargetListMessage) },
        { "Setup"_s, static_cast<CallHandler>(&RemoteInspector::receivedSetupMessage) },
        { "SendMessageToTarget"_s, static_cast<CallHandler>(&RemoteInspector::receivedDataMessage) },
        { "FrontendDidClose"_s, static_cast<CallHandler>(&RemoteInspector::receivedCloseMessage) },
    });

    return methods;
}

void RemoteInspector::sendWebInspectorEvent(const String& event)
{
    if (!m_clientID)
        return;

    const CString message = event.utf8();
    send(m_clientID.value(), reinterpret_cast<const uint8_t*>(message.data()), message.length());
}

void RemoteInspector::start()
{
    LockHolder lock(m_mutex);

    if (m_enabled || (s_connectionIdentifier == INVALID_SOCKET_VALUE && !s_serverPort))
        return;

    m_enabled = true;

    if (s_connectionIdentifier) {
        m_clientID = createClient(s_connectionIdentifier);
        s_connectionIdentifier = INVALID_SOCKET_VALUE;
    } else
        m_clientID = connectInet("127.0.0.1", s_serverPort);

    if (!m_targetMap.isEmpty())
        pushListingsSoon();
}

void RemoteInspector::stopInternal(StopSource)
{
    if (!m_enabled)
        return;

    m_enabled = false;
    m_pushScheduled = false;

    for (auto targetConnection : m_targetConnectionMap.values())
        targetConnection->close();
    m_targetConnectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;
    m_clientID = WTF::nullopt;
}

TargetListing RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    if (!target.remoteDebuggingAllowed())
        return nullptr;

    // FIXME: Support remote debugging of a ServiceWorker.
    if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        return nullptr;

    TargetListing targetListing = JSON::Object::create();

    targetListing->setString("name"_s, target.name());
    targetListing->setString("url"_s, target.name());
    targetListing->setInteger("targetID"_s, target.targetIdentifier());
    targetListing->setBoolean("hasLocalDebugger"_s, target.hasLocalDebugger());
    if (target.type() == RemoteInspectionTarget::Type::Web)
        targetListing->setString("type"_s, "web"_s);
    else if (target.type() == RemoteInspectionTarget::Type::JavaScript)
        targetListing->setString("type"_s, "javascript"_s);
    else if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        targetListing->setString("type"_s, "service-worker"_s);

    return targetListing;
}

TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget&) const
{
    return nullptr;
}

void RemoteInspector::pushListingsNow()
{
    if (!m_clientID)
        return;

    m_pushScheduled = false;

    auto targetListJSON = JSON::Array::create();
    for (auto listing : m_targetListingMap.values())
        targetListJSON->pushObject(listing);

    auto jsonEvent = JSON::Object::create();
    jsonEvent->setString("event"_s, "SetTargetList"_s);
    jsonEvent->setString("message"_s, targetListJSON->toJSONString());
    sendWebInspectorEvent(jsonEvent->toJSONString());
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_clientID)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;

    RunLoop::current().dispatch([=] {
        LockHolder lock(m_mutex);
        if (m_pushScheduled)
            pushListingsNow();
    });
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage()
{
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, const String& message)
{
    LockHolder lock(m_mutex);
    if (!m_clientID)
        return;

    auto sendMessageEvent = JSON::Object::create();
    sendMessageEvent->setInteger("targetID"_s, targetIdentifier);
    sendMessageEvent->setString("event"_s, "SendMessageToFrontend"_s);
    sendMessageEvent->setString("message"_s, message);
    sendWebInspectorEvent(sendMessageEvent->toJSONString());
}

void RemoteInspector::receivedGetTargetListMessage(const Event&)
{
    ASSERT(isMainThread());

    LockHolder lock(m_mutex);
    pushListingsNow();
}

void RemoteInspector::receivedSetupMessage(const Event& event)
{
    ASSERT(isMainThread());

    if (event.targetID)
        setup(event.targetID.value());
}

void RemoteInspector::receivedDataMessage(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.targetID || !event.message)
        return;

    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        LockHolder lock(m_mutex);
        connectionToTarget = m_targetConnectionMap.get(event.targetID.value());
        if (!connectionToTarget)
            return;
    }

    connectionToTarget->sendMessageToTarget(event.message.value());
}

void RemoteInspector::receivedCloseMessage(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.targetID)
        return;

    RefPtr<RemoteConnectionToTarget> connectionToTarget;
    {
        LockHolder lock(m_mutex);
        RemoteControllableTarget* target = m_targetMap.get(event.targetID.value());
        if (!target)
            return;

        connectionToTarget = m_targetConnectionMap.take(event.targetID.value());
        updateHasActiveDebugSession();
    }

    if (connectionToTarget)
        connectionToTarget->close();
}

void RemoteInspector::setup(TargetID targetIdentifier)
{
    RemoteControllableTarget* target;
    {
        LockHolder lock(m_mutex);
        target = m_targetMap.get(targetIdentifier);
        if (!target)
            return;
    }

    auto connectionToTarget = adoptRef(*new RemoteConnectionToTarget(*target));
    ASSERT(is<RemoteInspectionTarget>(target) || is<RemoteAutomationTarget>(target));
    if (!connectionToTarget->setup()) {
        connectionToTarget->close();
        return;
    }

    LockHolder lock(m_mutex);
    m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));

    updateHasActiveDebugSession();
}

void RemoteInspector::sendMessageToTarget(TargetID targetIdentifier, const char* message)
{
    if (auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier))
        connectionToTarget->sendMessageToTarget(String::fromUTF8(message));
}

void RemoteInspector::setConnectionIdentifier(PlatformSocketType connectionIdentifier)
{
    RemoteInspector::s_connectionIdentifier = connectionIdentifier;
}

void RemoteInspector::setServerPort(uint16_t port)
{
    RemoteInspector::s_serverPort = port;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
