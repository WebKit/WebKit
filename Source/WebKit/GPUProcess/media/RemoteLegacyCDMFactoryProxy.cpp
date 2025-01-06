/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteLegacyCDMFactoryProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteLegacyCDMProxy.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#include <WebCore/LegacyCDM.h>
#include <wtf/Algorithms.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLegacyCDMFactoryProxy);

RemoteLegacyCDMFactoryProxy::RemoteLegacyCDMFactoryProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteLegacyCDMFactoryProxy::~RemoteLegacyCDMFactoryProxy()
{
    clear();
}

void RemoteLegacyCDMFactoryProxy::clear()
{
    auto proxies = std::exchange(m_proxies, { });
    auto sessions = std::exchange(m_sessions, { });

    // FIXME: Make ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref/deref work inside a destructor.
    SUPPRESS_UNCOUNTED_LOCAL auto* connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;

    for (auto const& session : sessions)
        SUPPRESS_UNCOUNTED_ARG connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), session.key.toUInt64());

    for (auto const& proxy : proxies)
        SUPPRESS_UNCOUNTED_ARG connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), proxy.key.toUInt64());
}

void RemoteLegacyCDMFactoryProxy::createCDM(const String& keySystem, std::optional<MediaPlayerIdentifier>&& playerId, CompletionHandler<void(std::optional<RemoteLegacyCDMIdentifier>&&)>&& completion)
{
    RefPtr privateCDM = LegacyCDM::create(keySystem);
    if (!privateCDM) {
        completion(std::nullopt);
        return;
    }

    auto proxy = RemoteLegacyCDMProxy::create(*this, playerId, privateCDM.releaseNonNull());
    auto identifier = RemoteLegacyCDMIdentifier::generate();
    addProxy(identifier, WTFMove(proxy));
    completion(WTFMove(identifier));
}

void RemoteLegacyCDMFactoryProxy::supportsKeySystem(const String& keySystem, std::optional<String> mimeType, CompletionHandler<void(bool)>&& completion)
{
    if (mimeType)
        completion(LegacyCDM::keySystemSupportsMimeType(keySystem, *mimeType));
    else
        completion(LegacyCDM::supportsKeySystem(keySystem));
}

void RemoteLegacyCDMFactoryProxy::didReceiveCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (ObjectIdentifier<RemoteLegacyCDMIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr proxy = m_proxies.get(ObjectIdentifier<RemoteLegacyCDMIdentifierType>(decoder.destinationID())))
            proxy->didReceiveMessage(connection, decoder);
    }
}

void RemoteLegacyCDMFactoryProxy::didReceiveCDMSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (ObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr session = m_sessions.get(ObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>(decoder.destinationID())))
            session->didReceiveMessage(connection, decoder);
    }
}

bool RemoteLegacyCDMFactoryProxy::didReceiveSyncCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (ObjectIdentifier<RemoteLegacyCDMIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr proxy = m_proxies.get(ObjectIdentifier<RemoteLegacyCDMIdentifierType>(decoder.destinationID())))
            return proxy->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

bool RemoteLegacyCDMFactoryProxy::didReceiveSyncCDMSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (ObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr session = m_sessions.get(ObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>(decoder.destinationID())))
            return session->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

void RemoteLegacyCDMFactoryProxy::addProxy(RemoteLegacyCDMIdentifier identifier, Ref<RemoteLegacyCDMProxy>&& proxy)
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;

    connection->messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), identifier.toUInt64(), proxy.get());

    ASSERT(!m_proxies.contains(identifier));
    m_proxies.set(identifier, WTFMove(proxy));
}

void RemoteLegacyCDMFactoryProxy::removeProxy(RemoteLegacyCDMIdentifier identifier)
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;

    connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), identifier.toUInt64());

    ASSERT(m_proxies.contains(identifier));
    m_proxies.remove(identifier);
}

void RemoteLegacyCDMFactoryProxy::addSession(RemoteLegacyCDMSessionIdentifier identifier, Ref<RemoteLegacyCDMSessionProxy>&& session)
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;

    connection->messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), identifier.toUInt64(), session.get());

    ASSERT(!m_sessions.contains(identifier));
    m_sessions.set(identifier, WTFMove(session));
}

void RemoteLegacyCDMFactoryProxy::removeSession(RemoteLegacyCDMSessionIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection) {
        completionHandler();
        return;
    }

    connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), identifier.toUInt64());

    ASSERT(m_sessions.contains(identifier));
    if (RefPtr session = m_sessions.take(identifier))
        session->invalidate();

    if (connection && allowsExitUnderMemoryPressure())
        connection->protectedGPUProcess()->tryExitIfUnusedAndUnderMemoryPressure();

    completionHandler();
}

RemoteLegacyCDMSessionProxy* RemoteLegacyCDMFactoryProxy::getSession(const RemoteLegacyCDMSessionIdentifier& identifier) const
{
    auto results = m_sessions.find(identifier);
    if (results != m_sessions.end())
        return results->value.ptr();
    return nullptr;
}

bool RemoteLegacyCDMFactoryProxy::allowsExitUnderMemoryPressure() const
{
    return m_sessions.isEmpty();
}

#if !RELEASE_LOG_DISABLED
const Logger& RemoteLegacyCDMFactoryProxy::logger() const
{
    if (!m_logger) {
        Ref logger = Logger::create(this);
        m_logger = logger.ptr();
        RefPtr connection { m_gpuConnectionToWebProcess.get() };
        logger->setEnabled(this, connection && connection->isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}
#endif

std::optional<SharedPreferencesForWebProcess> RemoteLegacyCDMFactoryProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}

#endif
