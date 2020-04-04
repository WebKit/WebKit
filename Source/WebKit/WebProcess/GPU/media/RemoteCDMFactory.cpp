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
#include "RemoteCDMFactory.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteCDM.h"
#include "RemoteCDMFactoryProxyMessages.h"
#include "RemoteCDMInstanceSession.h"
#include "WebProcess.h"
#include <WebCore/Settings.h>

namespace WebKit {

using namespace WebCore;

RemoteCDMFactory::RemoteCDMFactory(WebProcess& process)
    : m_process(process)
{
}

RemoteCDMFactory::~RemoteCDMFactory() = default;

void RemoteCDMFactory::registerFactory(Vector<CDMFactory*>& factories)
{
    factories.append(this);
}

const char* RemoteCDMFactory::supplementName()
{
    return "RemoteCDMFactory";
}

GPUProcessConnection& RemoteCDMFactory::gpuProcessConnection()
{
    return m_process.ensureGPUProcessConnection();
}

bool RemoteCDMFactory::supportsKeySystem(const String& keySystem)
{
    bool supported = false;
    gpuProcessConnection().connection().sendSync(Messages::RemoteCDMFactoryProxy::SupportsKeySystem(keySystem), Messages::RemoteCDMFactoryProxy::SupportsKeySystem::Reply(supported), { });
    return supported;
}

std::unique_ptr<CDMPrivate> RemoteCDMFactory::createCDM(const String& keySystem)
{
    RemoteCDMIdentifier id;
    RemoteCDMConfiguration configuration;
    gpuProcessConnection().connection().sendSync(Messages::RemoteCDMFactoryProxy::CreateCDM(keySystem), Messages::RemoteCDMFactoryProxy::CreateCDM::Reply(id, configuration), { });
    if (!id)
        return nullptr;
    return RemoteCDM::create(makeWeakPtr(this), WTFMove(id), WTFMove(configuration));
}

void RemoteCDMFactory::addSession(Ref<RemoteCDMInstanceSession>&& session)
{
    ASSERT(!m_sessions.contains(session->identifier()));
    m_sessions.set(session->identifier(), WTFMove(session));
}

void RemoteCDMFactory::removeSession(RemoteCDMInstanceSessionIdentifier id)
{
    ASSERT(m_sessions.contains(id));
    m_sessions.remove(id);
}

void RemoteCDMFactory::didReceiveSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* session = m_sessions.get(makeObjectIdentifier<RemoteCDMInstanceSessionIdentifierType>(decoder.destinationID())))
        session->didReceiveMessage(connection, decoder);
}

}

#endif
