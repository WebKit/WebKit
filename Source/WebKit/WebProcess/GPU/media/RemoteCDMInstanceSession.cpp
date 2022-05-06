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
#include "RemoteCDMInstanceSession.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteCDMInstanceSessionProxyMessages.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Ref.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteCDMInstanceSession> RemoteCDMInstanceSession::create(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceSessionIdentifier&& identifier)
{
    return adoptRef(*new RemoteCDMInstanceSession(WTFMove(factory), WTFMove(identifier)));
}

RemoteCDMInstanceSession::RemoteCDMInstanceSession(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceSessionIdentifier&& identifier)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(identifier))
{
}

void RemoteCDMInstanceSession::requestLicense(LicenseType type, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    if (!m_factory) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::RequestLicense(type, initDataType, WTFMove(initData)), [callback = WTFMove(callback)] (RefPtr<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, bool succeeded) mutable {
        if (!message) {
            callback(SharedBuffer::create(), emptyString(), false, Failed);
            return;
        }
        callback(message.releaseNonNull(), sessionId, needsIndividualization, succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::updateLicense(const String& sessionId, LicenseType type, Ref<SharedBuffer>&& response, LicenseUpdateCallback&& callback)
{
    if (!m_factory) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::UpdateLicense(sessionId, type, WTFMove(response)), [callback = WTFMove(callback)] (bool sessionWasClosed, std::optional<KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<Message>&& message, bool succeeded) mutable {
        callback(sessionWasClosed, WTFMove(changedKeys), WTFMove(changedExpiration), WTFMove(message), succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::loadSession(LicenseType type, const String& sessionId, const String& origin, LoadSessionCallback&& callback)
{
    if (!m_factory) {
        callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::Other);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::LoadSession(type, sessionId, origin), [callback = WTFMove(callback)] (std::optional<KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<Message>&& message, bool succeeded, SessionLoadFailure loadFailure) mutable {
        callback(WTFMove(changedKeys), WTFMove(changedExpiration), WTFMove(message), succeeded ? Succeeded : Failed, loadFailure);
    }, m_identifier);
}

void RemoteCDMInstanceSession::closeSession(const String& sessionId, CloseSessionCallback&& callback)
{
    if (!m_factory) {
        callback();
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::CloseSession(sessionId), [callback = WTFMove(callback)] () mutable {
        callback();
    }, m_identifier);
}

void RemoteCDMInstanceSession::removeSessionData(const String& sessionId, LicenseType type, RemoveSessionDataCallback&& callback)
{
    if (!m_factory) {
        callback({ }, nullptr, Failed);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::RemoveSessionData(sessionId, type), [callback = WTFMove(callback)] (KeyStatusVector&& changedKeys, RefPtr<SharedBuffer>&& message, bool succeeded) mutable {
        callback(WTFMove(changedKeys), WTFMove(message), succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::storeRecordOfKeyUsage(const String& sessionId)
{
    if (!m_factory)
        return;

    m_factory->gpuProcessConnection().connection().send(Messages::RemoteCDMInstanceSessionProxy::StoreRecordOfKeyUsage(sessionId), m_identifier);
}

void RemoteCDMInstanceSession::updateKeyStatuses(KeyStatusVector&& keyStatuses)
{
    if (m_client)
        m_client->updateKeyStatuses(WTFMove(keyStatuses));
}

void RemoteCDMInstanceSession::sendMessage(WebCore::CDMMessageType type, RefPtr<SharedBuffer>&& message)
{
    if (m_client && message)
        m_client->sendMessage(type, message.releaseNonNull());
}

void RemoteCDMInstanceSession::sessionIdChanged(const String& sessionId)
{
    if (m_client)
        m_client->sessionIdChanged(sessionId);
}

}

#endif
