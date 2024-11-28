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
#include "RemoteLegacyCDMProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"

namespace WebKit {

using namespace WebCore;

Ref<RemoteLegacyCDMProxy> RemoteLegacyCDMProxy::create(WeakPtr<RemoteLegacyCDMFactoryProxy> factory, std::optional<MediaPlayerIdentifier> playerId, Ref<WebCore::LegacyCDM>&& cdm)
{
    return adoptRef(*new RemoteLegacyCDMProxy(WTFMove(factory), playerId, WTFMove(cdm)));
}

RemoteLegacyCDMProxy::RemoteLegacyCDMProxy(WeakPtr<RemoteLegacyCDMFactoryProxy>&& factory, std::optional<MediaPlayerIdentifier> playerId, Ref<WebCore::LegacyCDM>&& cdm)
    : m_factory(WTFMove(factory))
    , m_playerId(playerId)
    , m_cdm(WTFMove(cdm))
{
    m_cdm->setClient(this);
}

RemoteLegacyCDMProxy::~RemoteLegacyCDMProxy()
{
    m_cdm->setClient(nullptr);
}

void RemoteLegacyCDMProxy::supportsMIMEType(const String& mimeType, SupportsMIMETypeCallback&& callback)
{
    callback(protectedCDM()->supportsMIMEType(mimeType));
}

void RemoteLegacyCDMProxy::createSession(const String& keySystem, uint64_t logIdentifier, CreateSessionCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(std::nullopt);
        return;
    }

    auto sessionIdentifier = RemoteLegacyCDMSessionIdentifier::generate();
    Ref session = RemoteLegacyCDMSessionProxy::create(*factory, logIdentifier, sessionIdentifier, protectedCDM());
    factory->addSession(sessionIdentifier, WTFMove(session));
    callback(WTFMove(sessionIdentifier));
}

RefPtr<MediaPlayer> RemoteLegacyCDMProxy::cdmMediaPlayer(const LegacyCDM*) const
{
    RefPtr factory = m_factory.get();
    if (!m_playerId || !factory)
        return nullptr;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return nullptr;

    return gpuConnectionToWebProcess->protectedRemoteMediaPlayerManagerProxy()->mediaPlayer(*m_playerId);
}

std::optional<SharedPreferencesForWebProcess> RemoteLegacyCDMProxy::sharedPreferencesForWebProcess() const
{
    if (!m_factory)
        return std::nullopt;

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG return m_factory->sharedPreferencesForWebProcess();
}

}

#endif
