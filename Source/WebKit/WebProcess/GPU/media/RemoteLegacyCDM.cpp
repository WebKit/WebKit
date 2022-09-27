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
#include "RemoteLegacyCDM.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSession.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteLegacyCDM> RemoteLegacyCDM::create(WeakPtr<RemoteLegacyCDMFactory>&& factory, RemoteLegacyCDMIdentifier identifier)
{
    return std::unique_ptr<RemoteLegacyCDM>(new RemoteLegacyCDM(WTFMove(factory), WTFMove(identifier)));
}

RemoteLegacyCDM::RemoteLegacyCDM(WeakPtr<RemoteLegacyCDMFactory>&& factory, RemoteLegacyCDMIdentifier&& identifier)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(identifier))
{
}

RemoteLegacyCDM::~RemoteLegacyCDM() = default;

bool RemoteLegacyCDM::supportsMIMEType(const String& mimeType)
{
    if (!m_factory)
        return false;

    auto sendResult = m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::SupportsMIMEType(mimeType), m_identifier);
    auto [supported] = sendResult.takeReplyOr(false);
    return supported;
}

std::unique_ptr<WebCore::LegacyCDMSession> RemoteLegacyCDM::createSession(WebCore::LegacyCDMSessionClient& client)
{
    if (!m_factory)
        return nullptr;

    String storageDirectory = client.mediaKeysStorageDirectory();

    uint64_t logIdentifier { 0 };
#if !RELEASE_LOG_DISABLED
    logIdentifier = reinterpret_cast<uint64_t>(client.logIdentifier());
#endif

    auto sendResult = m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::CreateSession(storageDirectory, logIdentifier), m_identifier);
    auto [identifier] = sendResult.takeReplyOr(RemoteLegacyCDMSessionIdentifier { });
    if (!identifier)
        return nullptr;
    return RemoteLegacyCDMSession::create(m_factory, WTFMove(identifier), client);
}

void RemoteLegacyCDM::setPlayerId(MediaPlayerIdentifier identifier)
{
    if (!m_factory)
        return;

    std::optional<MediaPlayerIdentifier> optionalId;
    if (identifier)
        optionalId = identifier;
    m_factory->gpuProcessConnection().connection().send(Messages::RemoteLegacyCDMProxy::SetPlayerId(optionalId), m_identifier);
}

}

#endif
