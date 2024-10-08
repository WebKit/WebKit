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
#include "RemoteCDMInstanceProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceMessages.h"
#include "RemoteCDMInstanceSessionProxy.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CDMInstance.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteCDMInstanceProxy> RemoteCDMInstanceProxy::create(RemoteCDMProxy& cdm, Ref<CDMInstance>&& priv, RemoteCDMInstanceIdentifier identifier)
{
    auto configuration = makeUniqueRefWithoutFastMallocCheck<RemoteCDMInstanceConfiguration, RemoteCDMInstanceConfiguration&&>({
        priv->keySystem(),
    });
    return std::unique_ptr<RemoteCDMInstanceProxy>(new RemoteCDMInstanceProxy(cdm, WTFMove(priv), WTFMove(configuration), identifier));
}

RemoteCDMInstanceProxy::RemoteCDMInstanceProxy(RemoteCDMProxy& cdm, Ref<CDMInstance>&& priv, UniqueRef<RemoteCDMInstanceConfiguration>&& configuration, RemoteCDMInstanceIdentifier identifier)
    : m_cdm(cdm)
    , m_instance(WTFMove(priv))
    , m_configuration(WTFMove(configuration))
    , m_identifier(identifier)
#if !RELEASE_LOG_DISABLED
    , m_logger(cdm.logger())
    , m_logIdentifier(cdm.logIdentifier())
#endif
{
    protectedInstance()->setClient(*this);
}

RemoteCDMInstanceProxy::~RemoteCDMInstanceProxy()
{
    protectedInstance()->clearClient();
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

    gpuConnectionToWebProcess->protectedConnection()->send(Messages::RemoteCDMInstance::UnrequestedInitializationDataReceived(type, WTFMove(initData)), m_identifier);
}

void RemoteCDMInstanceProxy::initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers allowDistinctiveIdentifiers, AllowPersistentState allowPersistentState, CompletionHandler<void(SuccessValue)>&& completion)
{
    protectedInstance()->initializeWithConfiguration(configuration, allowDistinctiveIdentifiers, allowPersistentState, WTFMove(completion));
}

void RemoteCDMInstanceProxy::setServerCertificate(Ref<SharedBuffer>&& certificate, CompletionHandler<void(SuccessValue)>&& completion)
{
    protectedInstance()->setServerCertificate(WTFMove(certificate), WTFMove(completion));
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
        protectedInstance()->setStorageDirectory(directory);
}

void RemoteCDMInstanceProxy::createSession(uint64_t logIdentifier, CompletionHandler<void(std::optional<RemoteCDMInstanceSessionIdentifier>)>&& completion)
{
    auto privSession = protectedInstance()->createSession();
    if (!privSession || !m_cdm || !m_cdm->factory()) {
        completion(std::nullopt);
        return;
    }

#if !RELEASE_LOG_DISABLED
    privSession->setLogIdentifier(m_logIdentifier);
#endif

    auto identifier = RemoteCDMInstanceSessionIdentifier::generate();
    auto session = RemoteCDMInstanceSessionProxy::create(m_cdm.get(), privSession.releaseNonNull(), logIdentifier, identifier);
    protectedCdm()->protectedFactory()->addSession(identifier, WTFMove(session));
    completion(identifier);
}

Ref<WebCore::CDMInstance> RemoteCDMInstanceProxy::protectedInstance() const
{
    return m_instance;
}

std::optional<SharedPreferencesForWebProcess> RemoteCDMInstanceProxy::sharedPreferencesForWebProcess() const
{
    if (!m_cdm)
        return std::nullopt;

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG return m_cdm->sharedPreferencesForWebProcess();
}

RefPtr<RemoteCDMProxy> RemoteCDMInstanceProxy::protectedCdm() const
{
    return m_cdm.get();
}

}

#endif
