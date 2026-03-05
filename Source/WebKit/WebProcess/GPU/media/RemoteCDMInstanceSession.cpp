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
    return adoptRef(*new RemoteCDMInstanceSession(WTF::move(factory), WTF::move(identifier)));
}

RemoteCDMInstanceSession::RemoteCDMInstanceSession(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMInstanceSessionIdentifier&& identifier)
    : m_factory(WTF::move(factory))
    , m_identifier(WTF::move(identifier))
{
}

RemoteCDMInstanceSession::~RemoteCDMInstanceSession()
{
    protect(m_factory.get())->removeSession(m_identifier);
}

#if !RELEASE_LOG_DISABLED
void RemoteCDMInstanceSession::setLogIdentifier(uint64_t logIdentifier)
{
    protect(m_factory.get())->gpuProcessConnection().connection().send(Messages::RemoteCDMInstanceSessionProxy::SetLogIdentifier(reinterpret_cast<uint64_t>(logIdentifier)), m_identifier);
}
#endif

void RemoteCDMInstanceSession::requestLicense(LicenseType type, KeyGroupingStrategy keyGroupingStrategy, const String& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::RequestLicense(type, keyGroupingStrategy, initDataType, WTF::move(initData)), [callback = WTF::move(callback)] (RefPtr<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, bool succeeded) mutable {
        if (!message) {
            callback(SharedBuffer::create(), emptyString(), false, Failed);
            return;
        }
        callback(message.releaseNonNull(), sessionId, needsIndividualization, succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::updateLicense(const String& sessionId, LicenseType type, Ref<SharedBuffer>&& response, LicenseUpdateCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::UpdateLicense(sessionId, type, WTF::move(response)), [callback = WTF::move(callback)] (bool sessionWasClosed, std::optional<KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<Message>&& message, bool succeeded) mutable {
        callback(sessionWasClosed, WTF::move(changedKeys), WTF::move(changedExpiration), WTF::move(message), succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::loadSession(LicenseType type, const String& sessionId, const String& origin, LoadSessionCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::Other);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::LoadSession(type, sessionId, origin), [callback = WTF::move(callback)] (std::optional<KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<Message>&& message, bool succeeded, SessionLoadFailure loadFailure) mutable {
        callback(WTF::move(changedKeys), WTF::move(changedExpiration), WTF::move(message), succeeded ? Succeeded : Failed, loadFailure);
    }, m_identifier);
}

void RemoteCDMInstanceSession::closeSession(const String& sessionId, CloseSessionCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback();
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::CloseSession(sessionId), [callback = WTF::move(callback)] () mutable {
        callback();
    }, m_identifier);
}

void RemoteCDMInstanceSession::removeSessionData(const String& sessionId, LicenseType type, RemoveSessionDataCallback&& callback)
{
    RefPtr factory = m_factory.get();
    if (!factory) {
        callback({ }, nullptr, Failed);
        return;
    }

    factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMInstanceSessionProxy::RemoveSessionData(sessionId, type), [callback = WTF::move(callback)] (KeyStatusVector&& changedKeys, RefPtr<SharedBuffer>&& message, bool succeeded) mutable {
        callback(WTF::move(changedKeys), WTF::move(message), succeeded ? Succeeded : Failed);
    }, m_identifier);
}

void RemoteCDMInstanceSession::storeRecordOfKeyUsage(const String& sessionId)
{
    if (RefPtr factory = m_factory.get())
        factory->gpuProcessConnection().connection().send(Messages::RemoteCDMInstanceSessionProxy::StoreRecordOfKeyUsage(sessionId), m_identifier);
}

void RemoteCDMInstanceSession::updateKeyStatuses(KeyStatusVector&& keyStatuses)
{
    if (RefPtr client = m_client.get())
        client->updateKeyStatuses(WTF::move(keyStatuses));
}

void RemoteCDMInstanceSession::sendMessage(WebCore::CDMMessageType type, RefPtr<SharedBuffer>&& message)
{
    if (!message)
        return;

    if (RefPtr client = m_client.get())
        client->sendMessage(type, message.releaseNonNull());
}

void RemoteCDMInstanceSession::sessionIdChanged(const String& sessionId)
{
    if (RefPtr client = m_client.get())
        client->sessionIdChanged(sessionId);
}

}

#endif
