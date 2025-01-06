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
#include "RemoteCDMInstance.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteCDMInstanceMessages.h"
#include "RemoteCDMInstanceProxyMessages.h"
#include "RemoteCDMInstanceSession.h"
#include "RemoteCDMInstanceSessionIdentifier.h"
#include "WebProcess.h"
#include <WebCore/CDMKeySystemConfiguration.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteCDMInstance> RemoteCDMInstance::create(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceIdentifier&& identifier, RemoteCDMInstanceConfiguration&& configuration)
{
    return adoptRef(*new RemoteCDMInstance(WTFMove(factory), WTFMove(identifier), WTFMove(configuration)));
}

RemoteCDMInstance::RemoteCDMInstance(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceIdentifier&& identifier, RemoteCDMInstanceConfiguration&& configuration)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(identifier))
    , m_configuration(WTFMove(configuration))
{
    if (RefPtr factory = m_factory.get())
        factory->gpuProcessConnection().messageReceiverMap().addMessageReceiver(Messages::RemoteCDMInstance::messageReceiverName(), m_identifier.toUInt64(), *this);
}

RemoteCDMInstance::~RemoteCDMInstance()
{
    RefPtr factory = m_factory.get();
    if (!factory)
        return;

    factory->removeInstance(m_identifier);
    factory->gpuProcessConnection().messageReceiverMap().removeMessageReceiver(Messages::RemoteCDMInstance::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteCDMInstance::unrequestedInitializationDataReceived(const String& type, Ref<SharedBuffer>&& initData)
{
    if (RefPtr client = m_client.get())
        client->unrequestedInitializationDataReceived(type, WTFMove(initData));
}

void RemoteCDMInstance::initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers distinctiveIdentifiers, AllowPersistentState persistentState, SuccessCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(Failed);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceProxy::InitializeWithConfiguration(configuration, distinctiveIdentifiers, persistentState), WTFMove(callback), m_identifier);
}

void RemoteCDMInstance::setServerCertificate(Ref<WebCore::SharedBuffer>&& certificate, SuccessCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(Failed);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceProxy::SetServerCertificate(WTFMove(certificate)), WTFMove(callback), m_identifier);
}

void RemoteCDMInstance::setStorageDirectory(const String& directory)
{
    if (RefPtr factory = m_factory.get())
        factory->gpuProcessConnection().connection().send(Messages::RemoteCDMInstanceProxy::SetStorageDirectory(directory), m_identifier);
}

RefPtr<WebCore::CDMInstanceSession> RemoteCDMInstance::createSession()
{
    RefPtr factory = m_factory.get();
    if (!factory)
        return nullptr;

    uint64_t logIdentifier { 0 };
#if !RELEASE_LOG_DISABLED
    if (m_client)
        logIdentifier = reinterpret_cast<uint64_t>(m_client->logIdentifier());
#endif

    auto sendResult = factory->gpuProcessConnection().connection().sendSync(Messages::RemoteCDMInstanceProxy::CreateSession(logIdentifier), m_identifier);
    auto [identifier] = sendResult.takeReplyOr(std::nullopt);
    if (!identifier)
        return nullptr;
    auto session = RemoteCDMInstanceSession::create(factory.get(), WTFMove(*identifier));
    factory->addSession(session.copyRef());
    return session;
}

}

#endif
