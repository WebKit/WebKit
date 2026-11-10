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

    m_clientConnection = std::nullopt;

    RunLoop::currentSingleton().dispatch([this] {
        Locker locker { m_mutex };
        stopInternal(StopSource::API);
    });
}

void RemoteInspector::sendWebInspectorEvent(StringImpl* event)
{
    if (!m_clientConnection)
        return;

    send(m_clientConnection.value(), byteCast<uint8_t>(utf8(event).span()));
}

void RemoteInspector::start()
{
    Locker locker { m_mutex };

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

    if (m_client)
        m_client->closeAutomationSession();

    updateHasActiveDebugSession();

    m_automaticInspectionCandidates.clear();
}

TargetListing RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    if (!target.allowsInspectionByPolicy())
        return nullptr;

    // FIXME: Support remote debugging of a ServiceWorker.
    if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        return nullptr;

    TargetListing targetListing = JSON::Object::create();

    targetListing->setString("name"_si, target.name());
    targetListing->setString("url"_si, target.url());
    targetListing->setInteger("targetID"_si, target.targetIdentifier());
    targetListing->setBoolean("hasLocalDebugger"_si, target.hasLocalDebugger());
    if (target.type() == RemoteInspectionTarget::Type::WebPage || target.type() == RemoteInspectionTarget::Type::LegacyWebPage)
        targetListing->setString("type"_si, "web-page"_si);
    else if (target.type() == RemoteInspectionTarget::Type::JavaScript)
        targetListing->setString("type"_si, "javascript"_si);
    else if (target.type() == RemoteInspectionTarget::Type::ServiceWorker)
        targetListing->setString("type"_si, "service-worker"_si);
    else if (target.type() == RemoteInspectionTarget::Type::WasmDebugger)
        targetListing->setString("type"_si, "wasm-debugger"_si);

    return targetListing;
}

TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    TargetListing targetListing = JSON::Object::create();
    targetListing->setString("type"_si, "automation"_si);
    targetListing->setString("name"_si, target.name());
    targetListing->setInteger("targetID"_si, target.targetIdentifier());
    targetListing->setBoolean("isPaired"_si, target.isPaired());
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
    jsonEvent->setString("event"_si, "SetTargetList"_si);
    jsonEvent->setString("message"_si, targetListJSON->toJSONString());
    jsonEvent->setInteger("connectionID"_si, m_clientConnection.value());
    jsonEvent->setBoolean("remoteAutomationAllowed"_si, m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed);
    sendWebInspectorEvent(jsonEvent->toJSONString());
}

void RemoteInspector::pushListingsSoon()
{
    if (!isConnected())
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;

    RunLoop::currentSingleton().dispatch([this] {
        Locker locker { m_mutex };
        if (m_pushScheduled)
            pushListingsNow();
    });
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage(TargetID)
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    // FIXME: Implement automatic inspection.
}

void RemoteInspector::requestAutomationSession(RefPtr<StringImpl>&& sessionID, const Client::SessionCapabilities& capabilities)
{
    if (!m_client)
        return;

    if (!m_clientCapabilities || !m_clientCapabilities->remoteAutomationAllowed) {
        LOG_ERROR("Error: Remote automation is not allowed");
        return;
    }

    if (!sessionID) {
        LOG_ERROR("Client error: SESSION ID cannot be null");
        return;
    }

    m_client->requestAutomationSession(WTF::move(sessionID), capabilities);
    updateClientCapabilities();
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, StringImpl* message)
{
    if (!m_clientConnection)
        return;

    auto sendMessageEvent = JSON::Object::create();
    sendMessageEvent->setInteger("targetID"_si, targetIdentifier);
    sendMessageEvent->setString("event"_si, "SendMessageToFrontend"_si);
    sendMessageEvent->setInteger("connectionID"_si, m_clientConnection.value());
    sendMessageEvent->setString("message"_si, message);
    sendWebInspectorEvent(sendMessageEvent->toJSONString());
}

void RemoteInspector::setup(TargetID targetIdentifier)
{
    RemoteControllableTarget* target;
    {
        Locker locker { m_mutex };
        if (m_targetConnectionMap.contains(targetIdentifier))
            return;

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

    Locker locker { m_mutex };
    m_targetConnectionMap.set(targetIdentifier, WTF::move(connectionToTarget));

    updateHasActiveDebugSession();
}

void RemoteInspector::sendMessageToTarget(TargetID targetIdentifier, const char* message)
{
    if (auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier))
        connectionToTarget->sendMessageToTarget(StringImpl::fromUTF8(message));
}

RefPtr<StringImpl> RemoteInspector::backendCommands() const
{
    if (isEmpty(m_backendCommandsPath))
        return nullptr;

    auto contents = FileSystem::readEntireFile(m_backendCommandsPath);

    return contents ? StringImpl::createOrNull(byteCast<Latin1Character>(contents->span())) : emptyString();
}

// RemoteInspectorConnectionClient handlers

HashMap<RefPtr<StringImpl>, RemoteInspectorConnectionClient::CallHandler>& RemoteInspector::dispatchMap()
{
    static NeverDestroyed<HashMap<RefPtr<StringImpl>, CallHandler>> methods = HashMap<RefPtr<StringImpl>, CallHandler>({
        { "SetupInspectorClient"_si, static_cast<CallHandler>(&RemoteInspector::setupInspectorClient) },
        { "Setup"_si, static_cast<CallHandler>(&RemoteInspector::setupTarget) },
        { "FrontendDidClose"_si, static_cast<CallHandler>(&RemoteInspector::frontendDidClose) },
        { "SendMessageToBackend"_si, static_cast<CallHandler>(&RemoteInspector::sendMessageToBackend) },
        { "StartAutomationSession"_si, static_cast<CallHandler>(&RemoteInspector::startAutomationSession) },
    });

    return methods;
}

void RemoteInspector::setupInspectorClient(const Event&)
{
    ASSERT(isMainThread());

    auto backendCommandsEvent = JSON::Object::create();
    backendCommandsEvent->setString("event"_si, "BackendCommands"_si);
    backendCommandsEvent->setString("message"_si, backendCommands());
    sendWebInspectorEvent(backendCommandsEvent->toJSONString());

    m_readyToPushListings = true;

    Locker locker { m_mutex };
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
        Locker locker { m_mutex };
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
        Locker locker { m_mutex };
        connectionToTarget = m_targetConnectionMap.get(event.targetID.value());
        if (!connectionToTarget)
            return;
    }

    connectionToTarget->sendMessageToTarget(RefPtr<StringImpl> { event.message.value() });

}

void RemoteInspector::startAutomationSession(const Event& event)
{
    ASSERT(isMainThread());

    if (!m_clientConnection)
        return;

    if (!event.message)
        return;

    auto parsedMessageValue = JSON::Value::parseJSON(*event.message);
    if (!parsedMessageValue)
        return;

    auto parsedMessageObject = parsedMessageValue->asObject();
    auto sessionID = parsedMessageObject->getString("sessionID"_si);
    if (!sessionID)
        return;

    RemoteInspector::Client::SessionCapabilities capabilities { };
    auto capabilitiesObject = parsedMessageObject->getObject("capabilities"_si);
    if (capabilitiesObject)
        capabilities.acceptInsecureCertificates = capabilitiesObject->getBoolean("acceptInsecureCerts"_si).value_or(false);

    requestAutomationSession(WTF::move(sessionID), capabilities);

    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_si, "StartAutomationSession_Return"_si);

    auto capability = clientCapabilities();

    auto message = JSON::Object::create();
    message->setString("browserName"_si, capability ? capability->browserName : emptyString());
    message->setString("browserVersion"_si, capability ? capability->browserVersion : emptyString());
    sendEvent->setString("message"_si, message->toJSONString());
    sendWebInspectorEvent(sendEvent->toJSONString());

    m_readyToPushListings = true;

    Locker locker { m_mutex };
    pushListingsNow();
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
