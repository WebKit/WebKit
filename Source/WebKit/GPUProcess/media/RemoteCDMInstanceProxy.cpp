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
#include "RemoteCDMInstanceProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceMessages.h"
#include "RemoteCDMInstanceSessionProxy.h"
#include <WebCore/CDMInstance.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteCDMInstanceProxy> RemoteCDMInstanceProxy::create(RemoteCDMProxy& cdm, Ref<CDMInstance>&& priv, RemoteCDMInstanceIdentifier identifier)
{
    auto configuration = makeUniqueRefWithoutFastMallocCheck<RemoteCDMInstanceConfiguration, RemoteCDMInstanceConfiguration&&>({
        priv->keySystem(),
    });
    return adoptRef(*new RemoteCDMInstanceProxy(cdm, WTF::move(priv), WTF::move(configuration), identifier));
}

RemoteCDMInstanceProxy::RemoteCDMInstanceProxy(RemoteCDMProxy& cdm, Ref<CDMInstance>&& priv, UniqueRef<RemoteCDMInstanceConfiguration>&& configuration, RemoteCDMInstanceIdentifier identifier)
    : m_cdm(cdm)
    , m_instance(WTF::move(priv))
    , m_configuration(WTF::move(configuration))
    , m_identifier(identifier)
#if !RELEASE_LOG_DISABLED
    , m_logger(cdm.logger())
    , m_logIdentifier(cdm.logIdentifier())
#endif
{
    m_instance->setClient(*this);
}

RemoteCDMInstanceProxy::~RemoteCDMInstanceProxy()
{
    m_instance->clearClient();
}

void RemoteCDMInstanceProxy::unrequestedInitializationDataReceived(const String& type, Ref<SharedBuffer>&& initData)
{
    if (!m_cdm)
        return;

    RefPtr factory = m_cdm->factory();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->connection().send(Messages::RemoteCDMInstance::UnrequestedInitializationDataReceived(type, WTF::move(initData)), m_identifier);
}

void RemoteCDMInstanceProxy::initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers allowDistinctiveIdentifiers, AllowPersistentState allowPersistentState, CompletionHandler<void(SuccessValue)>&& completion)
{
    m_instance->initializeWithConfiguration(configuration, allowDistinctiveIdentifiers, allowPersistentState, WTF::move(completion));
}

void RemoteCDMInstanceProxy::setServerCertificate(Ref<SharedBuffer>&& certificate, CompletionHandler<void(SuccessValue)>&& completion)
{
    m_instance->setServerCertificate(WTF::move(certificate), WTF::move(completion));
}

void RemoteCDMInstanceProxy::setStorageDirectory(const String& directory)
{
    if (!m_cdm)
        return;

    RefPtr factory = m_cdm->factory();
    if (!factory)
        return;

    auto mediaKeysStorageDirectory = factory->mediaKeysStorageDirectory();
    if (mediaKeysStorageDirectory.isEmpty())
        return;

    if (directory.startsWith(mediaKeysStorageDirectory))
        m_instance->setStorageDirectory(directory);
}

void RemoteCDMInstanceProxy::createSession(uint64_t logIdentifier, CompletionHandler<void(std::optional<RemoteCDMInstanceSessionIdentifier>)>&& completion)
{
    auto privSession = m_instance->createSession();
    if (!privSession || !m_cdm || !m_cdm->factory()) {
        completion(std::nullopt);
        return;
    }

#if !RELEASE_LOG_DISABLED
    privSession->setLogIdentifier(m_logIdentifier);
#endif

    auto identifier = RemoteCDMInstanceSessionIdentifier::generate();
    auto session = RemoteCDMInstanceSessionProxy::create(m_cdm.get(), privSession.releaseNonNull(), logIdentifier, identifier);
    protect(protectedCdm()->factory())->addSession(identifier, WTF::move(session));
    completion(identifier);
}

std::optional<SharedPreferencesForWebProcess> RemoteCDMInstanceProxy::sharedPreferencesForWebProcess() const
{
    if (!m_cdm)
        return std::nullopt;

    return protectedCdm()->sharedPreferencesForWebProcess();
}

RefPtr<RemoteCDMProxy> RemoteCDMInstanceProxy::protectedCdm() const
{
    return m_cdm.get();
}

}

#endif
