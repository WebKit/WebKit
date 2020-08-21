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

#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceSessionProxy.h"
#include "SharedBufferCopy.h"
#include <WebCore/CDMInstance.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteCDMInstanceProxy> RemoteCDMInstanceProxy::create(WeakPtr<RemoteCDMProxy>&& cdm, Ref<CDMInstance>&& priv)
{
    auto configuration = makeUniqueRefWithoutFastMallocCheck<RemoteCDMInstanceConfiguration, RemoteCDMInstanceConfiguration&&>({
        priv->keySystem(),
    });
    return std::unique_ptr<RemoteCDMInstanceProxy>(new RemoteCDMInstanceProxy(WTFMove(cdm), WTFMove(priv), WTFMove(configuration)));
}

RemoteCDMInstanceProxy::RemoteCDMInstanceProxy(WeakPtr<RemoteCDMProxy>&& cdm, Ref<CDMInstance>&& priv, UniqueRef<RemoteCDMInstanceConfiguration>&& configuration)
    : m_cdm(WTFMove(cdm))
    , m_instance(WTFMove(priv))
    , m_configuration(WTFMove(configuration))
{
}

RemoteCDMInstanceProxy::~RemoteCDMInstanceProxy() = default;

void RemoteCDMInstanceProxy::initializeWithConfiguration(const WebCore::CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers allowDistinctiveIdentifiers, AllowPersistentState allowPersistentState, CompletionHandler<void(SuccessValue)>&& completion)
{
    m_instance->initializeWithConfiguration(configuration, allowDistinctiveIdentifiers, allowPersistentState, WTFMove(completion));
}

void RemoteCDMInstanceProxy::setServerCertificate(IPC::SharedBufferCopy&& certificate, CompletionHandler<void(SuccessValue)>&& completion)
{
    if (!certificate.buffer()) {
        completion(CDMInstance::Failed);
        return;
    }

    m_instance->setServerCertificate(certificate.buffer().releaseNonNull(), WTFMove(completion));
}

void RemoteCDMInstanceProxy::setStorageDirectory(const String& directory)
{
    m_instance->setStorageDirectory(directory);
}

void RemoteCDMInstanceProxy::createSession(CompletionHandler<void(const RemoteCDMInstanceSessionIdentifier&)>&& completion)
{
    auto privSession = m_instance->createSession();
    if (!privSession || !m_cdm || !m_cdm->factory()) {
        completion({ });
        return;
    }
    auto identifier = RemoteCDMInstanceSessionIdentifier::generate();
    auto session = RemoteCDMInstanceSessionProxy::create(makeWeakPtr(m_cdm.get()), privSession.releaseNonNull(), identifier);
    m_cdm->factory()->addSession(identifier, WTFMove(session));
    completion(identifier);
}

}

#endif
