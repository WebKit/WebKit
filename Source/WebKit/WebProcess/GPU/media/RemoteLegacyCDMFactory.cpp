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
#include "RemoteLegacyCDMFactory.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "MediaPlayerPrivateRemote.h"
#include "RemoteLegacyCDM.h"
#include "RemoteLegacyCDMFactoryProxyMessages.h"
#include "RemoteLegacyCDMSession.h"
#include "WebProcess.h"
#include <WebCore/LegacyCDM.h>
#include <WebCore/Settings.h>

namespace WebKit {

using namespace WebCore;

RemoteLegacyCDMFactory::RemoteLegacyCDMFactory(WebProcess& process)
    : m_process(process)
{
}

RemoteLegacyCDMFactory::~RemoteLegacyCDMFactory() = default;

void RemoteLegacyCDMFactory::registerFactory()
{
    LegacyCDM::clearFactories();
    LegacyCDM::registerCDMFactory(
        [weakThis = makeWeakPtr(this)] (LegacyCDM* privateCDM) -> std::unique_ptr<WebCore::CDMPrivateInterface> {
            if (weakThis)
                return weakThis->createCDM(privateCDM);
            return nullptr;
        },
        [weakThis = makeWeakPtr(this)] (const String& keySystem) {
            return weakThis ? weakThis->supportsKeySystem(keySystem) : false;
        },
        [weakThis = makeWeakPtr(this)] (const String& keySystem, const String& mimeType) {
            return weakThis ? weakThis->supportsKeySystemAndMimeType(keySystem, mimeType) : false;
        }
    );
}

const char* RemoteLegacyCDMFactory::supplementName()
{
    return "RemoteLegacyCDMFactory";
}

GPUProcessConnection& RemoteLegacyCDMFactory::gpuProcessConnection()
{
    return m_process.ensureGPUProcessConnection();
}

bool RemoteLegacyCDMFactory::supportsKeySystem(const String& keySystem)
{
    auto foundInCache = m_supportsKeySystemCache.find(keySystem);
    if (foundInCache != m_supportsKeySystemCache.end())
        return foundInCache->value;

    bool supported = false;
    gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem(keySystem, std::nullopt), Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem::Reply(supported), { });
    m_supportsKeySystemCache.set(keySystem, supported);
    return supported;
}

bool RemoteLegacyCDMFactory::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    auto key = std::make_pair(keySystem, mimeType);
    auto foundInCache = m_supportsKeySystemAndMimeTypeCache.find(key);
    if (foundInCache != m_supportsKeySystemAndMimeTypeCache.end())
        return foundInCache->value;

    bool supported = false;
    gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem(keySystem, mimeType), Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem::Reply(supported), { });
    m_supportsKeySystemAndMimeTypeCache.set(key, supported);
    return supported;
}

std::unique_ptr<CDMPrivateInterface> RemoteLegacyCDMFactory::createCDM(WebCore::LegacyCDM* cdm)
{
    if (!cdm) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    std::optional<MediaPlayerIdentifier> playerId;
    if (auto player = cdm->mediaPlayer())
        playerId = gpuProcessConnection().mediaPlayerManager().findRemotePlayerId(player->playerPrivate());

    RemoteLegacyCDMIdentifier identifier;
    gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::CreateCDM(cdm->keySystem(), WTFMove(playerId)), Messages::RemoteLegacyCDMFactoryProxy::CreateCDM::Reply(identifier), { });
    if (!identifier)
        return nullptr;
    auto remoteCDM = RemoteLegacyCDM::create(makeWeakPtr(this), identifier);
    m_cdms.set(identifier, makeWeakPtr(remoteCDM.get()));
    return remoteCDM;
}

void RemoteLegacyCDMFactory::addSession(RemoteLegacyCDMSessionIdentifier identifier, std::unique_ptr<RemoteLegacyCDMSession>&& session)
{
    ASSERT(!m_sessions.contains(identifier));
    m_sessions.set(identifier, WTFMove(session));
}

void RemoteLegacyCDMFactory::removeSession(RemoteLegacyCDMSessionIdentifier identifier)
{
    ASSERT(m_sessions.contains(identifier));
    m_sessions.remove(identifier);
}

RemoteLegacyCDM* RemoteLegacyCDMFactory::findCDM(CDMPrivateInterface* privateInterface) const
{
    for (auto& cdm : m_cdms.values()) {
        if (privateInterface == cdm.get())
            return cdm.get();
    }
    return nullptr;
}

void RemoteLegacyCDMFactory::didReceiveSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* session = m_sessions.get(makeObjectIdentifier<RemoteLegacyCDMSessionIdentifierType>(decoder.destinationID())))
        session->didReceiveMessage(connection, decoder);
}

}

#endif
