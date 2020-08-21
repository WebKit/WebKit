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
#include "RemoteCDMInstanceProxyMessages.h"
#include "RemoteCDMInstanceSession.h"
#include "RemoteCDMInstanceSessionIdentifier.h"
#include "SharedBufferCopy.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CDMKeySystemConfiguration.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteCDMInstance> RemoteCDMInstance::create(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceIdentifier&& id, RemoteCDMInstanceConfiguration&& configuration)
{
    return adoptRef(*new RemoteCDMInstance(WTFMove(factory), WTFMove(id), WTFMove(configuration)));
}

RemoteCDMInstance::RemoteCDMInstance(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceIdentifier&& id, RemoteCDMInstanceConfiguration&& configuration)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(id))
    , m_configuration(WTFMove(configuration))
{
}

void RemoteCDMInstance::initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers distinctiveIdentifiers, AllowPersistentState persistentState, SuccessCallback&& callback)
{
    if (!m_factory) {
        callback(Failed);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceProxy::InitializeWithConfiguration(configuration, distinctiveIdentifiers, persistentState), WTFMove(callback), m_identifier);
}

void RemoteCDMInstance::setServerCertificate(Ref<WebCore::SharedBuffer>&& certificate, SuccessCallback&& callback)
{
    if (!m_factory) {
        callback(Failed);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceProxy::SetServerCertificate(certificate.get()), WTFMove(callback), m_identifier);
}

void RemoteCDMInstance::setStorageDirectory(const String& directory)
{
    if (!m_factory)
        return;

    m_factory->gpuProcessConnection().connection().send(Messages::RemoteCDMInstanceProxy::SetStorageDirectory(directory), m_identifier);
}

RefPtr<WebCore::CDMInstanceSession> RemoteCDMInstance::createSession()
{
    if (!m_factory)
        return nullptr;

    RemoteCDMInstanceSessionIdentifier id;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteCDMInstanceProxy::CreateSession(), Messages::RemoteCDMInstanceProxy::CreateSession::Reply(id), m_identifier);
    if (!id)
        return nullptr;
    auto session = RemoteCDMInstanceSession::create(makeWeakPtr(m_factory.get()), WTFMove(id));
    m_factory->addSession(session.copyRef());
    return session;
}

}

#endif
