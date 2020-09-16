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
#include "RemoteCDMProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "RemoteCDMConfiguration.h"
#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceProxy.h"
#include <WebCore/CDMKeySystemConfiguration.h>
#include <WebCore/CDMPrivate.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteCDMProxy> RemoteCDMProxy::create(WeakPtr<RemoteCDMFactoryProxy>&& factory, std::unique_ptr<WebCore::CDMPrivate>&& priv)
{
    if (!priv)
        return nullptr;

    auto configuration = makeUniqueRefWithoutFastMallocCheck<RemoteCDMConfiguration, RemoteCDMConfiguration&&>({
        priv->supportedInitDataTypes(),
        priv->supportedRobustnesses(),
        priv->supportsServerCertificates(),
        priv->supportsSessions()
    });
    // Use new() to access CDMPrivate's private constructor.
    return std::unique_ptr<RemoteCDMProxy>(new RemoteCDMProxy(WTFMove(factory), WTFMove(priv), WTFMove(configuration)));
}

RemoteCDMProxy::RemoteCDMProxy(WeakPtr<RemoteCDMFactoryProxy>&& factory, std::unique_ptr<CDMPrivate>&& priv, UniqueRef<RemoteCDMConfiguration>&& configuration)
    : m_factory(WTFMove(factory))
    , m_private(WTFMove(priv))
    , m_configuration(WTFMove(configuration))
{
}

RemoteCDMProxy::~RemoteCDMProxy() = default;

bool RemoteCDMProxy::supportsInitData(const AtomString& type, const SharedBuffer& data)
{
    return m_private->supportsInitData(type, data);
}

RefPtr<SharedBuffer> RemoteCDMProxy::sanitizeResponse(const SharedBuffer& response)
{
    return m_private->sanitizeResponse(response);
}

Optional<String> RemoteCDMProxy::sanitizeSessionId(const String& sessionId)
{
    return m_private->sanitizeSessionId(sessionId);
}

void RemoteCDMProxy::getSupportedConfiguration(WebCore::CDMKeySystemConfiguration&& configuration, WebCore::CDMPrivate::LocalStorageAccess access, WebCore::CDMPrivate::SupportedConfigurationCallback&& callback)
{
    m_private->getSupportedConfiguration(WTFMove(configuration), access, WTFMove(callback));
}

void RemoteCDMProxy::createInstance(CompletionHandler<void(RemoteCDMInstanceIdentifier, RemoteCDMInstanceConfiguration&&)>&& completion)
{
    auto privateInstance = m_private->createInstance();
    if (!privateInstance || !m_factory) {
        completion({ }, { });
        return;
    }
    auto instance = RemoteCDMInstanceProxy::create(makeWeakPtr(this), privateInstance.releaseNonNull());
    auto identifier = RemoteCDMInstanceIdentifier::generate();
    RemoteCDMInstanceConfiguration configuration = instance->configuration();
    m_factory->addInstance(identifier, WTFMove(instance));
    completion(identifier, WTFMove(configuration));
}

void RemoteCDMProxy::loadAndInitialize()
{
    m_private->loadAndInitialize();
}

}

#endif
