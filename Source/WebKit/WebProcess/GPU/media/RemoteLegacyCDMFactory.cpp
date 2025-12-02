/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "RemoteLegacyCDMSessionMessages.h"
#include "WebProcess.h"
#include <WebCore/LegacyCDM.h>
#include <WebCore/Settings.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLegacyCDMFactory);

RemoteLegacyCDMFactory::RemoteLegacyCDMFactory(WebProcess& webProcess)
    : m_webProcess(webProcess)
{
}

RemoteLegacyCDMFactory::~RemoteLegacyCDMFactory() = default;

void RemoteLegacyCDMFactory::ref() const
{
    m_webProcess->ref();
}

void RemoteLegacyCDMFactory::deref() const
{
    m_webProcess->deref();
}

void RemoteLegacyCDMFactory::registerFactory()
{
    LegacyCDM::clearFactories();
    LegacyCDM::registerCDMFactory(
        [protectedThis = Ref { *this }] (LegacyCDM& privateCDM) -> std::unique_ptr<WebCore::CDMPrivateInterface> {
            return protectedThis->createCDM(privateCDM);
        },
        [protectedThis = Ref { *this }] (const String& keySystem) {
            return protectedThis->supportsKeySystem(keySystem);
        },
        [protectedThis = Ref { *this }] (const String& keySystem, const String& mimeType) {
            return protectedThis->supportsKeySystemAndMimeType(keySystem, mimeType);
        }
    );
}

ASCIILiteral RemoteLegacyCDMFactory::supplementName()
{
    return "RemoteLegacyCDMFactory"_s;
}

GPUProcessConnection& RemoteLegacyCDMFactory::gpuProcessConnection()
{
    return WebProcess::singleton().ensureGPUProcessConnection();
}

bool RemoteLegacyCDMFactory::supportsKeySystem(const String& keySystem)
{
    auto foundInCache = m_supportsKeySystemCache.find(keySystem);
    if (foundInCache != m_supportsKeySystemCache.end())
        return foundInCache->value;

    auto sendResult = gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem(keySystem, std::nullopt), { });
    auto [supported] = sendResult.takeReplyOr(false);
    m_supportsKeySystemCache.set(keySystem, supported);
    return supported;
}

bool RemoteLegacyCDMFactory::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    auto key = std::make_pair(keySystem, mimeType);
    auto foundInCache = m_supportsKeySystemAndMimeTypeCache.find(key);
    if (foundInCache != m_supportsKeySystemAndMimeTypeCache.end())
        return foundInCache->value;

    auto sendResult = gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::SupportsKeySystem(keySystem, mimeType), { });
    auto [supported] = sendResult.takeReplyOr(false);
    m_supportsKeySystemAndMimeTypeCache.set(key, supported);
    return supported;
}

std::unique_ptr<CDMPrivateInterface> RemoteLegacyCDMFactory::createCDM(WebCore::LegacyCDM& cdm)
{
    std::optional<MediaPlayerIdentifier> playerId;
    Ref gpuProcessConnection = this->gpuProcessConnection();
    if (auto player = cdm.mediaPlayer())
        playerId = gpuProcessConnection->protectedMediaPlayerManager()->findRemotePlayerId(player->protectedPlayerPrivate().get());

    auto sendResult = gpuProcessConnection->connection().sendSync(Messages::RemoteLegacyCDMFactoryProxy::CreateCDM(cdm.keySystem(), WTFMove(playerId)), { });
    auto [identifier] = sendResult.takeReplyOr(std::nullopt);
    if (!identifier)
        return nullptr;
    auto remoteCDM = makeUniqueRefWithoutRefCountedCheck<RemoteLegacyCDM>(*this, *identifier);
    m_cdms.set(*identifier, Ref { remoteCDM.get() }.get());
    return remoteCDM.moveToUniquePtr();
}

void RemoteLegacyCDMFactory::addSession(RemoteLegacyCDMSessionIdentifier identifier, RemoteLegacyCDMSession& session)
{
    ASSERT(!m_sessions.contains(identifier));
    m_sessions.set(identifier, WeakPtr { session });

    gpuProcessConnection().messageReceiverMap().addMessageReceiver(Messages::RemoteLegacyCDMSession::messageReceiverName(), identifier.toUInt64(), session);
}

void RemoteLegacyCDMFactory::removeSession(RemoteLegacyCDMSessionIdentifier identifier)
{
    ASSERT(m_sessions.contains(identifier));
    RefPtr session = m_sessions.get(identifier);
    gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteLegacyCDMFactoryProxy::RemoveSession(identifier), [protectedThis = Ref { *this }, identifier, session = WTFMove(session)] {
        ASSERT(protectedThis->m_sessions.contains(identifier));
        protectedThis->m_sessions.remove(identifier);
        protectedThis->gpuProcessConnection().messageReceiverMap().removeMessageReceiver(Messages::RemoteLegacyCDMSession::messageReceiverName(), identifier.toUInt64());
        UNUSED_PARAM(session);
    }, { });
}

RemoteLegacyCDM* RemoteLegacyCDMFactory::findCDM(CDMPrivateInterface* privateInterface) const
{
    for (auto& cdm : m_cdms.values()) {
        if (privateInterface == cdm.get())
            return cdm.get();
    }
    return nullptr;
}

}

#endif
