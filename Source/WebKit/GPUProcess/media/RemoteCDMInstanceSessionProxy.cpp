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
#include "RemoteCDMInstanceSessionProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "RemoteCDMFactoryProxy.h"
#include "RemoteCDMInstanceSessionMessages.h"
#include "SharedBufferCopy.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteCDMInstanceSessionProxy> RemoteCDMInstanceSessionProxy::create(WeakPtr<RemoteCDMProxy>&& proxy, Ref<WebCore::CDMInstanceSession>&& session, RemoteCDMInstanceSessionIdentifier identifier)
{
    auto sessionProxy = std::unique_ptr<RemoteCDMInstanceSessionProxy>(new RemoteCDMInstanceSessionProxy(WTFMove(proxy), WTFMove(session), identifier));
    auto client = makeWeakPtr<WebCore::CDMInstanceSessionClient>(sessionProxy.get());
    sessionProxy->m_session->setClient(WTFMove(client));
    return sessionProxy;
}

RemoteCDMInstanceSessionProxy::RemoteCDMInstanceSessionProxy(WeakPtr<RemoteCDMProxy>&& cdm, Ref<WebCore::CDMInstanceSession>&& session, RemoteCDMInstanceSessionIdentifier identifier)
    : m_cdm(WTFMove(cdm))
    , m_session(WTFMove(session))
    , m_identifier(identifier)
{
}

RemoteCDMInstanceSessionProxy::~RemoteCDMInstanceSessionProxy()
{
}

void RemoteCDMInstanceSessionProxy::requestLicense(LicenseType type, AtomString initDataType, IPC::SharedBufferCopy&& initData, LicenseCallback&& completion)
{
    if (!initData.buffer()) {
        completion({ }, emptyString(), false, false);
        return;
    }

    // Implement the CDMPrivate::supportsInitData() check here:
    if (!m_cdm->supportsInitData(initDataType, *initData.buffer())) {
        completion({ }, emptyString(), false, false);
        return;
    }

    m_session->requestLicense(type, initDataType, initData.buffer().releaseNonNull(), [completion = WTFMove(completion)] (Ref<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, CDMInstanceSession::SuccessValue succeeded) mutable {
        completion(WTFMove(message), sessionId, needsIndividualization, succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::updateLicense(String sessionId, LicenseType type, IPC::SharedBufferCopy&& response, LicenseUpdateCallback&& completion)
{
    if (!response.buffer()) {
        completion(true, { }, WTF::nullopt, WTF::nullopt, false);
        return;
    }

    // Implement the CDMPrivate::sanitizeResponse() check here:
    auto sanitizedResponse = m_cdm->sanitizeResponse(*response.buffer());
    if (!sanitizedResponse) {
        completion(false, { }, WTF::nullopt, WTF::nullopt, false);
        return;
    }

    m_session->updateLicense(sessionId, type, sanitizedResponse.releaseNonNull(), [completion = WTFMove(completion)] (bool sessionClosed, Optional<CDMInstanceSession::KeyStatusVector>&& keyStatuses, Optional<double>&& expirationTime, Optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded) mutable {
        completion(sessionClosed, WTFMove(keyStatuses), WTFMove(expirationTime), WTFMove(message), succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::loadSession(LicenseType type, String sessionId, String origin, LoadSessionCallback&& completion)
{
    // Implement the CDMPrivate::sanitizeSessionId() check here:
    auto sanitizedSessionId = m_cdm->sanitizeSessionId(sessionId);
    if (!sanitizedSessionId) {
        completion(WTF::nullopt, WTF::nullopt, WTF::nullopt, false, CDMInstanceSession::SessionLoadFailure::MismatchedSessionType);
        return;
    }

    m_session->loadSession(type, *sanitizedSessionId, origin, [completion = WTFMove(completion)] (Optional<CDMInstanceSession::KeyStatusVector>&& keyStatuses, Optional<double>&& expirationTime, Optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded, CDMInstanceSession::SessionLoadFailure failure) mutable {
        completion(WTFMove(keyStatuses), WTFMove(expirationTime), WTFMove(message), succeeded == CDMInstanceSession::Succeeded, failure);
    });
}

void RemoteCDMInstanceSessionProxy::closeSession(const String& sessionId, CloseSessionCallback&& completion)
{
    m_session->closeSession(sessionId, [completion = WTFMove(completion)] () mutable { completion(); });
}

void RemoteCDMInstanceSessionProxy::removeSessionData(const String& sessionId, LicenseType type, RemoveSessionDataCallback&& completion)
{
    m_session->removeSessionData(sessionId, type, [completion = WTFMove(completion)] (CDMInstanceSession::KeyStatusVector&& keyStatuses, Optional<Ref<SharedBuffer>>&& expiredSessionsData, CDMInstanceSession::SuccessValue succeeded) mutable {
        Optional<IPC::SharedBufferCopy> expiredSessionDataReference;
        if (expiredSessionsData)
            expiredSessionDataReference = IPC::SharedBufferCopy(WTFMove(*expiredSessionsData));
        completion(WTFMove(keyStatuses), WTFMove(expiredSessionDataReference), succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::storeRecordOfKeyUsage(const String& sessionId)
{
    m_session->storeRecordOfKeyUsage(sessionId);
}

void RemoteCDMInstanceSessionProxy::displayIDChanged(PlatformDisplayID displayID)
{
    m_session->displayChanged(displayID);
}

void RemoteCDMInstanceSessionProxy::updateKeyStatuses(KeyStatusVector&& keyStatuses)
{
    if (!m_cdm)
        return;

    if (auto* factory = m_cdm->factory())
        factory->gpuConnectionToWebProcess().connection().send(Messages::RemoteCDMInstanceSession::UpdateKeyStatuses(WTFMove(keyStatuses)), m_identifier);
}

void RemoteCDMInstanceSessionProxy::sendMessage(CDMMessageType type, Ref<SharedBuffer>&& message)
{
    if (!m_cdm)
        return;

    if (auto* factory = m_cdm->factory())
        factory->gpuConnectionToWebProcess().connection().send(Messages::RemoteCDMInstanceSession::SendMessage(type, WTFMove(message)), m_identifier);
}

void RemoteCDMInstanceSessionProxy::sessionIdChanged(const String& sessionId)
{
    if (!m_cdm)
        return;

    if (auto* factory = m_cdm->factory())
        factory->gpuConnectionToWebProcess().connection().send(Messages::RemoteCDMInstanceSession::SessionIdChanged(sessionId), m_identifier);
}

}

#endif
