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
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLegacyCDM);

RemoteLegacyCDM::RemoteLegacyCDM(RemoteLegacyCDMFactory& factory, RemoteLegacyCDMIdentifier identifier)
    : m_factory(factory)
    , m_identifier(identifier)
{
}

RemoteLegacyCDM::~RemoteLegacyCDM() = default;

Ref<RemoteLegacyCDMFactory> RemoteLegacyCDM::protectedFactory() const
{
    return m_factory.get();
}

bool RemoteLegacyCDM::supportsMIMEType(const String& mimeType) const
{
    auto sendResult = protectedFactory()->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::SupportsMIMEType(mimeType), m_identifier);
    auto [supported] = sendResult.takeReplyOr(false);
    return supported;
}

std::unique_ptr<WebCore::LegacyCDMSession> RemoteLegacyCDM::createSession(WebCore::LegacyCDMSessionClient& client)
{
    String storageDirectory = client.mediaKeysStorageDirectory();

    uint64_t logIdentifier { 0 };
#if !RELEASE_LOG_DISABLED
    logIdentifier = reinterpret_cast<uint64_t>(client.logIdentifier());
#endif

    Ref factory = m_factory.get();
    auto sendResult = factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMProxy::CreateSession(storageDirectory, logIdentifier), m_identifier);
    auto [identifier] = sendResult.takeReplyOr(std::nullopt);
    if (!identifier)
        return nullptr;
    return RemoteLegacyCDMSession::create(factory, WTFMove(*identifier), client);
}

void RemoteLegacyCDM::setPlayerId(std::optional<MediaPlayerIdentifier> identifier)
{
    protectedFactory()->gpuProcessConnection().connection().send(Messages::RemoteLegacyCDMProxy::SetPlayerId(identifier), m_identifier);
}

void RemoteLegacyCDM::ref() const
{
    m_factory->ref();
}

void RemoteLegacyCDM::deref() const
{
    m_factory->deref();
}

}

#endif
