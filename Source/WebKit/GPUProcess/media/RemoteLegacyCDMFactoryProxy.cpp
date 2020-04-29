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

#include "RemoteLegacyCDMProxy.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#include <WebCore/LegacyCDM.h>
#include <wtf/Algorithms.h>

namespace WebKit {

using namespace WebCore;

RemoteLegacyCDMFactoryProxy::RemoteLegacyCDMFactoryProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteLegacyCDMFactoryProxy::~RemoteLegacyCDMFactoryProxy() = default;

void RemoteLegacyCDMFactoryProxy::createCDM(const String& keySystem, Optional<MediaPlayerPrivateRemoteIdentifier>&& optionalPlayerId, CompletionHandler<void(RemoteLegacyCDMIdentifier&&)>&& completion)
{
    auto privateCDM = LegacyCDM::create(keySystem);
    if (!privateCDM) {
        completion({ });
        return;
    }

    MediaPlayerPrivateRemoteIdentifier playerId;
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

void RemoteLegacyCDMFactoryProxy::addProxy(RemoteLegacyCDMIdentifier id, std::unique_ptr<RemoteLegacyCDMProxy>&& proxy)
{
    gpuConnectionToWebProcess().messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), id.toUInt64(), *proxy);

    ASSERT(!m_proxies.contains(id));
    m_proxies.set(id, WTFMove(proxy));
}

void RemoteLegacyCDMFactoryProxy::removeProxy(RemoteLegacyCDMIdentifier id)
{
    gpuConnectionToWebProcess().messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMProxy::messageReceiverName(), id.toUInt64());

    ASSERT(m_proxies.contains(id));
    m_proxies.remove(id);
}

void RemoteLegacyCDMFactoryProxy::addSession(RemoteLegacyCDMSessionIdentifier id, std::unique_ptr<RemoteLegacyCDMSessionProxy>&& session)
{
    gpuConnectionToWebProcess().messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), id.toUInt64(), *session);

    ASSERT(!m_sessions.contains(id));
    m_sessions.set(id, WTFMove(session));
}

void RemoteLegacyCDMFactoryProxy::removeSession(RemoteLegacyCDMSessionIdentifier id)
{
    gpuConnectionToWebProcess().messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSessionProxy::messageReceiverName(), id.toUInt64());

    ASSERT(m_sessions.contains(id));
    m_sessions.remove(id);
}

RemoteLegacyCDMSessionProxy* RemoteLegacyCDMFactoryProxy::getSession(const RemoteLegacyCDMSessionIdentifier& id) const
{
    auto results = m_sessions.find(id);
    if (results != m_sessions.end())
        return results->value.get();
    return nullptr;
}


}

#endif
