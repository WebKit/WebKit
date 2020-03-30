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

std::unique_ptr<RemoteLegacyCDM> RemoteLegacyCDM::create(WeakPtr<RemoteLegacyCDMFactory>&& factory, RemoteLegacyCDMIdentifier id)
{
    return std::unique_ptr<RemoteLegacyCDM>(new RemoteLegacyCDM(WTFMove(factory), WTFMove(id)));
}

RemoteLegacyCDM::RemoteLegacyCDM(WeakPtr<RemoteLegacyCDMFactory>&& factory, RemoteLegacyCDMIdentifier&& id)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(id))
{
}

RemoteLegacyCDM::~RemoteLegacyCDM() = default;

bool RemoteLegacyCDM::supportsMIMEType(const String& mimeType)
{
    if (!m_factory)
        return false;

    bool supported = false;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::SupportsMIMEType(mimeType), Messages::RemoteLegacyCDMProxy::SupportsMIMEType::Reply(supported), m_identifier);
    return supported;
}

std::unique_ptr<WebCore::LegacyCDMSession> RemoteLegacyCDM::createSession(WebCore::LegacyCDMSessionClient* client)
{
    if (!m_factory)
        return nullptr;

    String storageDirectory = client ? client->mediaKeysStorageDirectory() : emptyString();

    RemoteLegacyCDMSessionIdentifier id;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::CreateSession(storageDirectory), Messages::RemoteLegacyCDMProxy::CreateSession::Reply(id), m_identifier);
    if (!id)
        return nullptr;
    return RemoteLegacyCDMSession::create(m_factory, WTFMove(id));
}

void RemoteLegacyCDM::setPlayerId(MediaPlayerPrivateRemoteIdentifier id)
{
    if (!m_factory)
        return;

    Optional<MediaPlayerPrivateRemoteIdentifier> optionalId;
    if (id)
        optionalId = id;
    m_factory->gpuProcessConnection().connection().send(Messages::RemoteLegacyCDMProxy::SetPlayerId(optionalId), m_identifier);
}

}

#endif
