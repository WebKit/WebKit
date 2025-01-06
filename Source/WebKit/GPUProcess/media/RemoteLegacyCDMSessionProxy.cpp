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
#include "RemoteLegacyCDMSessionProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionMessages.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayAdaptors.h>
#include <WebCore/LegacyCDM.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/LoggerHelper.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteLegacyCDMSessionProxy> RemoteLegacyCDMSessionProxy::create(RemoteLegacyCDMFactoryProxy& factory, uint64_t logIdentifier, RemoteLegacyCDMSessionIdentifier sessionIdentifier, WebCore::LegacyCDM& cdm)
{
    return adoptRef(*new RemoteLegacyCDMSessionProxy(factory, logIdentifier, sessionIdentifier, cdm));
}

RemoteLegacyCDMSessionProxy::RemoteLegacyCDMSessionProxy(RemoteLegacyCDMFactoryProxy& factory, uint64_t parentLogIdentifier, RemoteLegacyCDMSessionIdentifier sessionIdentifier, WebCore::LegacyCDM& cdm)
    : m_factory(factory)
#if !RELEASE_LOG_DISABLED
    , m_logger(factory.logger())
    , m_logIdentifier(parentLogIdentifier)
#endif
    , m_identifier(sessionIdentifier)
    , m_session(cdm.createSession(*this))
{
    if (!m_session)
        ERROR_LOG(LOGIDENTIFIER, "could not create CDM session.");
}

RemoteLegacyCDMSessionProxy::~RemoteLegacyCDMSessionProxy() = default;

void RemoteLegacyCDMSessionProxy::invalidate()
{
    m_factory = nullptr;
}

static RefPtr<Uint8Array> convertToUint8Array(RefPtr<SharedBuffer>&& buffer)
{
    if (!buffer)
        return nullptr;

    auto arrayBuffer = buffer->tryCreateArrayBuffer();
    if (!arrayBuffer)
        return nullptr;
    return Uint8Array::create(arrayBuffer.releaseNonNull(), 0, buffer->size());
}

template <typename T>
static RefPtr<WebCore::SharedBuffer> convertToOptionalSharedBuffer(T array)
{
    if (!array)
        return nullptr;
    return SharedBuffer::create(array->span());
}

void RemoteLegacyCDMSessionProxy::setPlayer(WeakPtr<RemoteMediaPlayerProxy> player)
{
    m_player = WTFMove(player);
}

void RemoteLegacyCDMSessionProxy::generateKeyRequest(const String& mimeType, RefPtr<SharedBuffer>&& initData, GenerateKeyCallback&& completion)
{
    RefPtr session = m_session;
    if (!session) {
        completion({ }, emptyString(), 0, 0);
        return;
    }
    
    auto initDataArray = convertToUint8Array(WTFMove(initData));
    if (!initDataArray) {
        completion({ }, emptyString(), 0, 0);
        return;
    }

    String destinationURL;
    unsigned short errorCode { 0 };
    uint32_t systemCode { 0 };

    auto keyRequest = session->generateKeyRequest(mimeType, initDataArray.get(), destinationURL, errorCode, systemCode);

    destinationURL = "this is a test string"_s;

    completion(convertToOptionalSharedBuffer(keyRequest), destinationURL, errorCode, systemCode);
}

void RemoteLegacyCDMSessionProxy::releaseKeys()
{
    if (RefPtr session = m_session)
        session->releaseKeys();
}

void RemoteLegacyCDMSessionProxy::update(RefPtr<SharedBuffer>&& update, UpdateCallback&& completion)
{
    RefPtr session = m_session;
    if (!session) {
        completion(false, nullptr, 0, 0);
        return;
    }
    
    auto updateArray = convertToUint8Array(WTFMove(update));
    if (!updateArray) {
        completion(false, nullptr, 0, 0);
        return;
    }

    RefPtr<Uint8Array> nextMessage;
    unsigned short errorCode { 0 };
    uint32_t systemCode { 0 };

    bool succeeded = session->update(updateArray.get(), nextMessage, errorCode, systemCode);

    completion(succeeded, convertToOptionalSharedBuffer(nextMessage), errorCode, systemCode);
}

RefPtr<ArrayBuffer> RemoteLegacyCDMSessionProxy::getCachedKeyForKeyId(const String& keyId)
{
    RefPtr session = m_session;
    if (!session)
        return nullptr;
    
    return session->cachedKeyForKeyID(keyId);
}

void RemoteLegacyCDMSessionProxy::cachedKeyForKeyID(String keyId, CachedKeyForKeyIDCallback&& completion)
{
    completion(convertToOptionalSharedBuffer(getCachedKeyForKeyId(keyId)));
}

void RemoteLegacyCDMSessionProxy::sendMessage(Uint8Array* message, String destinationURL)
{
    RefPtr factory = m_factory.get();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->protectedConnection()->send(Messages::RemoteLegacyCDMSession::SendMessage(convertToOptionalSharedBuffer(message), destinationURL), m_identifier);
}

void RemoteLegacyCDMSessionProxy::sendError(MediaKeyErrorCode errorCode, uint32_t systemCode)
{
    RefPtr factory = m_factory.get();
    if (!factory)
        return;

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return;

    gpuConnectionToWebProcess->protectedConnection()->send(Messages::RemoteLegacyCDMSession::SendError(errorCode, systemCode), m_identifier);
}

String RemoteLegacyCDMSessionProxy::mediaKeysStorageDirectory() const
{
    RefPtr factory = m_factory.get();
    if (!factory)
        return emptyString();

    RefPtr gpuConnectionToWebProcess = factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return emptyString();

    return gpuConnectionToWebProcess->mediaKeysStorageDirectory();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemoteLegacyCDMSessionProxy::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, EME);
}
#endif

std::optional<SharedPreferencesForWebProcess> RemoteLegacyCDMSessionProxy::sharedPreferencesForWebProcess() const
{
    if (!m_factory)
        return std::nullopt;

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG return m_factory->sharedPreferencesForWebProcess();
}

RefPtr<WebCore::LegacyCDMSession> RemoteLegacyCDMSessionProxy::protectedSession() const
{
    return m_session;
}

} // namespace WebKit

#endif
