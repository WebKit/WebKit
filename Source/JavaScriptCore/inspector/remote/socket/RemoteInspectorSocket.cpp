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
#include <wtf/FileSystem.h>
#include <wtf/JSONValues.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace Inspector {

RemoteInspector& RemoteInspector::singleton()
{
    static LazyNeverDestroyed<RemoteInspector> shared;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        shared.construct();
    });
    return shared;
}

RemoteInspector::RemoteInspector()
{
    Socket::init();
    start();
}

void RemoteInspector::connect(ConnectionID id)
{
    ASSERT(!isConnected());

    m_clientConnection = id;
    start();
}

void RemoteInspector::didClose(RemoteInspectorSocketEndpoint&, ConnectionID)
{
    ASSERT(isConnected());

    m_clientConnection = WTF::nullopt;

    RunLoop::current().dispatch([=] {
        LockHolder lock(m_mutex);
        stopInternal(StopSource::API);
    });
}

void RemoteInspector::sendWebInspectorEvent(const String& event)
{
    if (!m_clientConnection)
        return;

    const CString message = event.utf8();
    send(m_clientConnection.value(), reinterpret_cast<const uint8_t*>(message.data()), message.length());
}

void RemoteInspector::start()
{
    LockHolder lock(m_mutex);

    if (m_enabled)
        return;

    m_enabled = true;
}

void RemoteInspector::stopInternal(StopSource)
{
    if (!m_enabled)
        return;

    m_enabled = false;
    m_pushScheduled = false;
    m_readyToPushListings = false;

    for (auto targetConnection : m_targetConnectionMap.values())
        targetConnection->close();
    m_targetConnectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;
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
    targetListing->setString("url"_s, target.url());
    targetListing->setInteger("targetID"_s, target.targetIdentifier());
    targetListing->setBoolean("hasLocalDebugger"_s, target.hasLocalDebugger());
    if (target.type() == RemoteInspectionTarget::Type::WebPage)
        targetListing->setString("type"_s, "web-page"_s);
    else if (target.type() == RemoteInspectionTarget::Type::Page)
        targetListing->setString("type"_s, "page"_s);
    else if (target.type() == RemoteInspectionTarget::Type::JavaScript)
        targetListing->setString("type"_s, "javascript"_s);
    else if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        targetListing->setString("type"_s, "service-worker"_s);

    return targetListing;
}

TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    TargetListing targetListing = JSON::Object::create();
    targetListing->setString("type"_s, "automation"_s);
    targetListing->setString("name"_s, target.name());
    targetListing->setInteger("targetID"_s, target.targetIdentifier());
    targetListing->setBoolean("isPaired"_s, target.isPaired());
    return targetListing;
}

void RemoteInspector::pushListingsNow()
{
    if (!isConnected() || !m_readyToPushListings)
        return;

    m_pushScheduled = false;

    auto targetListJSON = JSON::Array::create();
    for (auto listing : m_targetListingMap.values())
        targetListJSON->pushObject(*listing);

    auto jsonEvent = JSON::Object::create();
    jsonEvent->setString("event"_s, "SetTargetList"_s);
    jsonEvent->setString("message"_s, targetListJSON->toJSONString());
    jsonEvent->setInteger("connectionID"_s, m_clientConnection.value());
    jsonEvent->setBoolean("remoteAutomationAllowed"_s, m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed);
    sendWebInspectorEvent(jsonEvent->toJSONString());
}

void RemoteInspector::pushListingsSoon()
{
    if (!isConnected())
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
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_automaticInspectionPaused);
    ASSERT(m_automaticInspectionCandidateTargetIdentifier);
    // FIXME: Implement automatic inspection.
}

void RemoteInspector::requestAutomationSession(const String& sessionID, const Client::SessionCapabilities& capabilities)
{
    if (!m_client)
        return;

    if (!m_clientCapabilities || !m_clientCapabilities->remoteAutomationAllowed) {
        LOG_ERROR("Error: Remote automation is not allowed");
        return;
    }

    if (sessionID.isNull()) {
        LOG_ERROR("Client error: SESSION ID cannot be null");
        return;
    }

    m_client->requestAutomationSession(sessionID, capabilities);
    updateClientCapabilities();
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, const String& message)
{
    if (!m_clientConnection)
        return;

    auto sendMessageEvent = JSON::Object::create();
    sendMessageEvent->setInteger("targetID"_s, targetIdentifier);
    sendMessageEvent->setString("event"_s, "SendMessageToFrontend"_s);
    sendMessageEvent->setInteger("connectionID"_s, m_clientConnection.value());
    sendMessageEvent->setString("message"_s, message);
    sendWebInspectorEvent(sendMessageEvent->toJSONString());
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

String RemoteInspector::backendCommands() const
{
    if (m_backendCommandsPath.isEmpty())
        return { };

    auto handle = FileSystem::openFile(m_backendCommandsPath, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(handle))
        return { };

    String result;
    long long size;
    if (FileSystem::getFileSize(handle, size)) {
        Vector<LChar> buffer(size);
        if (FileSystem::readFromFile(handle, reinterpret_cast<char*>(buffer.data()), size) == size)
            result = String::adopt(WTFMove(buffer));
    }
    FileSystem::closeFile(handle);
    return result;
}

// RemoteInspectorConnectionClient handlers

HashMap<String, RemoteInspectorConnectionClient::CallHandler>& RemoteInspector::dispatchMap()
{
    static NeverDestroyed<HashMap<String, CallHandler>> methods = HashMap<String, CallHandler>({
        { "SetupInspectorClient"_s, static_cast<CallHandler>(&RemoteInspector::setupInspectorClient) },
        { "Setup"_s, static_cast<CallHandler>(&RemoteInspector::setupTarget) },
        { "FrontendDidClose"_s, static_cast<CallHandler>(&RemoteInspector::frontendDidClose) },
        { "SendMessageToBackend"_s, static_cast<CallHandler>(&RemoteInspector::sendMessageToBackend) },
        { "StartAutomationSession"_s, static_cast<CallHandler>(&RemoteInspector::startAutomationSession) },
    });

    return methods;
}

void RemoteInspector::setupInspectorClient(const Event&)
{
    ASSERT(isMainThread());

    auto backendCommandsEvent = JSON::Object::create();
    backendCommandsEvent->setString("event"_s, "BackendCommands"_s);
    backendCommandsEvent->setString("message"_s, backendCommands());
    sendWebInspectorEvent(backendCommandsEvent->toJSONString());

    m_readyToPushListings = true;

    LockHolder lock(m_mutex);
    pushListingsNow();
}

void RemoteInspector::setupTarget(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.targetID || !event.connectionID)
        return;

    setup(event.targetID.value());
}

void RemoteInspector::frontendDidClose(const Event& event)
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

void RemoteInspector::sendMessageToBackend(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.connectionID || !event.targetID || !event.message)
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

void RemoteInspector::startAutomationSession(const Event& event)
{
    ASSERT(isMainThread());

    if (!event.message)
        return;

    requestAutomationSession(event.message.value(), { });

    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_s, "SetCapabilities"_s);

    auto capability = clientCapabilities();

    auto message = JSON::Object::create();
    message->setString("browserName"_s, capability ? capability->browserName : "");
    message->setString("browserVersion"_s, capability ? capability->browserVersion : "");
    sendEvent->setString("message"_s, message->toJSONString());
    sendWebInspectorEvent(sendEvent->toJSONString());

    m_readyToPushListings = true;

    LockHolder lock(m_mutex);
    pushListingsNow();
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
