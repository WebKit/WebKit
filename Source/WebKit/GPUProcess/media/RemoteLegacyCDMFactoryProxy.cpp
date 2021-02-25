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
#include "RemoteLegacyCDMProxy.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#include <WebCore/LegacyCDM.h>
#include <wtf/Algorithms.h>

namespace WebKit {

using namespace WebCore;

RemoteLegacyCDMFactoryProxy::RemoteLegacyCDMFactoryProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(makeWeakPtr(connection))
{
}

RemoteLegacyCDMFactoryProxy::~RemoteLegacyCDMFactoryProxy()
{
    if (!m_gpuConnectionToWebProcess)
        return;

    for (auto const& session : m_sessions)
        m_gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), session.key.toUInt64());

    for (auto const& proxy : m_proxies)
        m_gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), proxy.key.toUInt64());
}

void RemoteLegacyCDMFactoryProxy::createCDM(const String& keySystem, Optional<MediaPlayerIdentifier>&& optionalPlayerId, CompletionHandler<void(RemoteLegacyCDMIdentifier&&)>&& completion)
{
    auto privateCDM = LegacyCDM::create(keySystem);
    if (!privateCDM) {
        completion({ });
        return;
    }

    MediaPlayerIdentifier playerId;
    if (optionalPlayerId)
        playerId = WTFMove(optionalPlayerId.value());

    auto proxy = RemoteLegacyCDMProxy::create(makeWeakPtr(this), WTFMove(playerId), WTFMove(privateCDM));
    auto identifier = RemoteLegacyCDMIdentifier::generate();
    addProxy(identifier, WTFMove(proxy));
    completion(WTFMove(identifier));
}

void RemoteLegacyCDMFactoryProxy::supportsKeySystem(const String& keySystem, Optional<String> mimeType, CompletionHandler<void(bool)>&& completion)
{
    if (mimeType)
        completion(LegacyCDM::keySystemSupportsMimeType(keySystem, *mimeType));
    else
        completion(LegacyCDM::supportsKeySystem(keySystem));
}

void RemoteLegacyCDMFactoryProxy::didReceiveCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* proxy = m_proxies.get(makeObjectIdentifier<RemoteLegacyCDMIdentifierType>(decoder.destinationID())))
        proxy->didReceiveMessage(connection, decoder);
}

void RemoteLegacyCDMFactoryProxy::didReceiveCDMSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* session = m_sessions.get(makeObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>(decoder.destinationID())))
        session->didReceiveMessage(connection, decoder);
}

void RemoteLegacyCDMFactoryProxy::didReceiveSyncCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& encoder)
{
    if (auto* proxy = m_proxies.get(makeObjectIdentifier<RemoteLegacyCDMIdentifierType>(decoder.destinationID())))
        proxy->didReceiveSyncMessage(connection, decoder, encoder);
}

void RemoteLegacyCDMFactoryProxy::didReceiveSyncCDMSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& encoder)
{
    if (auto* session = m_sessions.get(makeObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>(decoder.destinationID())))
        session->didReceiveSyncMessage(connection, decoder, encoder);
}

void RemoteLegacyCDMFactoryProxy::addProxy(RemoteLegacyCDMIdentifier identifier, std::unique_ptr<RemoteLegacyCDMProxy>&& proxy)
{
    if (!m_gpuConnectionToWebProcess)
        return;

    m_gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), identifier.toUInt64(), *proxy);

    ASSERT(!m_proxies.contains(identifier));
    m_proxies.set(identifier, WTFMove(proxy));
}

void RemoteLegacyCDMFactoryProxy::removeProxy(RemoteLegacyCDMIdentifier identifier)
{
    if (!m_gpuConnectionToWebProcess)
        return;

    m_gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), identifier.toUInt64());

    ASSERT(m_proxies.contains(identifier));
    m_proxies.remove(identifier);
}

void RemoteLegacyCDMFactoryProxy::addSession(RemoteLegacyCDMSessionIdentifier identifier, std::unique_ptr<RemoteLegacyCDMSessionProxy>&& session)
{
    if (!m_gpuConnectionToWebProcess)
        return;

    m_gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), identifier.toUInt64(), *session);

    ASSERT(!m_sessions.contains(identifier));
    m_sessions.set(identifier, WTFMove(session));
}

void RemoteLegacyCDMFactoryProxy::removeSession(RemoteLegacyCDMSessionIdentifier identifier)
{
    if (!m_gpuConnectionToWebProcess)
        return;

    m_gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), identifier.toUInt64());

    ASSERT(m_sessions.contains(identifier));
    m_sessions.remove(identifier);
}

RemoteLegacyCDMSessionProxy* RemoteLegacyCDMFactoryProxy::getSession(const RemoteLegacyCDMSessionIdentifier& identifier) const
{
    auto results = m_sessions.find(identifier);
    if (results != m_sessions.end())
        return results->value.get();
    return nullptr;
}

}

#endif
