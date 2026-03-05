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
#include "RemoteCDMInstanceSessionProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "RemoteCDMFactoryProxy.h"
#include "RemoteCDMInstanceSessionMessages.h"
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteCDMInstanceSessionProxy> RemoteCDMInstanceSessionProxy::create(WeakPtr<RemoteCDMProxy>&& proxy, Ref<WebCore::CDMInstanceSession>&& session, uint64_t logIdentifier, RemoteCDMInstanceSessionIdentifier identifier)
{
    Ref sessionProxy = adoptRef(*new RemoteCDMInstanceSessionProxy(WTF::move(proxy), WTF::move(session), logIdentifier, identifier));
    WeakPtr<WebCore::CDMInstanceSessionClient> client = sessionProxy.get();
    protect(sessionProxy->m_session)->setClient(WTF::move(client));
    return sessionProxy;
}

RemoteCDMInstanceSessionProxy::RemoteCDMInstanceSessionProxy(WeakPtr<RemoteCDMProxy>&& cdm, Ref<WebCore::CDMInstanceSession>&& session, uint64_t logIdentifier, RemoteCDMInstanceSessionIdentifier identifier)
    : m_cdm(WTF::move(cdm))
    , m_session(WTF::move(session))
    , m_identifier(identifier)
{
}

RemoteCDMInstanceSessionProxy::~RemoteCDMInstanceSessionProxy()
{
}

void RemoteCDMInstanceSessionProxy::setLogIdentifier(uint64_t logIdentifier)
{
#if !RELEASE_LOG_DISABLED
    protect(m_session)->setLogIdentifier(logIdentifier);
#else
    UNUSED_PARAM(logIdentifier);
#endif
}

void RemoteCDMInstanceSessionProxy::requestLicense(LicenseType type, KeyGroupingStrategy keyGroupingStrategy, String initDataType, RefPtr<WebCore::SharedBuffer>&& initData, LicenseCallback&& completion)
{
    if (!initData) {
        completion({ }, emptyString(), false, false);
        return;
    }

    // Implement the CDMPrivate::supportsInitData() check here:
    if (!protect(m_cdm)->supportsInitData(initDataType, *initData)) {
        completion({ }, emptyString(), false, false);
        return;
    }

    protect(m_session)->requestLicense(type, keyGroupingStrategy, initDataType, initData.releaseNonNull(), [completion = WTF::move(completion)] (Ref<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, CDMInstanceSession::SuccessValue succeeded) mutable {
        completion(WTF::move(message), sessionId, needsIndividualization, succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::updateLicense(String sessionId, LicenseType type, RefPtr<SharedBuffer>&& response, LicenseUpdateCallback&& completion)
{
    if (!response) {
        completion(true, { }, std::nullopt, std::nullopt, false);
        return;
    }

    // Implement the CDMPrivate::sanitizeResponse() check here:
    auto sanitizedResponse = protect(m_cdm)->sanitizeResponse(*response);
    if (!sanitizedResponse) {
        completion(false, { }, std::nullopt, std::nullopt, false);
        return;
    }

    protect(m_session)->updateLicense(sessionId, type, sanitizedResponse.releaseNonNull(), [completion = WTF::move(completion)] (bool sessionClosed, std::optional<CDMInstanceSession::KeyStatusVector>&& keyStatuses, std::optional<double>&& expirationTime, std::optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded) mutable {
        completion(sessionClosed, WTF::move(keyStatuses), WTF::move(expirationTime), WTF::move(message), succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::loadSession(LicenseType type, String sessionId, String origin, LoadSessionCallback&& completion)
{
    // Implement the CDMPrivate::sanitizeSessionId() check here:
    auto sanitizedSessionId = protect(m_cdm)->sanitizeSessionId(sessionId);
    if (!sanitizedSessionId) {
        completion(std::nullopt, std::nullopt, std::nullopt, false, CDMInstanceSession::SessionLoadFailure::MismatchedSessionType);
        return;
    }

    protect(m_session)->loadSession(type, *sanitizedSessionId, origin, [completion = WTF::move(completion)] (std::optional<CDMInstanceSession::KeyStatusVector>&& keyStatuses, std::optional<double>&& expirationTime, std::optional<CDMInstanceSession::Message>&& message, CDMInstanceSession::SuccessValue succeeded, CDMInstanceSession::SessionLoadFailure failure) mutable {
        completion(WTF::move(keyStatuses), WTF::move(expirationTime), WTF::move(message), succeeded == CDMInstanceSession::Succeeded, failure);
    });
}

void RemoteCDMInstanceSessionProxy::closeSession(const String& sessionId, CloseSessionCallback&& completion)
{
    protect(m_session)->closeSession(sessionId, [completion = WTF::move(completion)] () mutable {
        completion();
    });
}

void RemoteCDMInstanceSessionProxy::removeSessionData(const String& sessionId, LicenseType type, RemoveSessionDataCallback&& completion)
{
    protect(m_session)->removeSessionData(sessionId, type, [completion = WTF::move(completion)] (CDMInstanceSession::KeyStatusVector&& keyStatuses, RefPtr<SharedBuffer>&& expiredSessionsData, CDMInstanceSession::SuccessValue succeeded) mutable {
        completion(WTF::move(keyStatuses), WTF::move(expiredSessionsData), succeeded == CDMInstanceSession::Succeeded);
    });
}

void RemoteCDMInstanceSessionProxy::storeRecordOfKeyUsage(const String& sessionId)
{
    protect(m_session)->storeRecordOfKeyUsage(sessionId);
}

void RemoteCDMInstanceSessionProxy::updateKeyStatuses(KeyStatusVector&& keyStatuses)
{
    if (!m_cdm)
        return;

    RefPtr factory = m_cdm->factory();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->connection().send(Messages::RemoteCDMInstanceSession::UpdateKeyStatuses(WTF::move(keyStatuses)), m_identifier);
}

void RemoteCDMInstanceSessionProxy::sendMessage(CDMMessageType type, Ref<SharedBuffer>&& message)
{
    if (!m_cdm)
        return;

    RefPtr factory = m_cdm->factory();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->connection().send(Messages::RemoteCDMInstanceSession::SendMessage(type, WTF::move(message)), m_identifier);
}

void RemoteCDMInstanceSessionProxy::sessionIdChanged(const String& sessionId)
{
    if (!m_cdm)
        return;

    RefPtr factory = m_cdm->factory();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->connection().send(Messages::RemoteCDMInstanceSession::SessionIdChanged(sessionId), m_identifier);
}

std::optional<SharedPreferencesForWebProcess> RemoteCDMInstanceSessionProxy::sharedPreferencesForWebProcess() const
{
    if (!m_cdm)
        return std::nullopt;

    return protect(m_cdm)->sharedPreferencesForWebProcess();
}

}

#endif
