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
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionMessages.h"
#include "SharedBufferCopy.h"
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteLegacyCDMSessionProxy> RemoteLegacyCDMSessionProxy::create(WeakPtr<RemoteLegacyCDMFactoryProxy>&& factory, RemoteLegacyCDMSessionIdentifier identifier, WebCore::LegacyCDM& cdm)
{
    return std::unique_ptr<RemoteLegacyCDMSessionProxy>(new RemoteLegacyCDMSessionProxy(WTFMove(factory), identifier, cdm));
}

RemoteLegacyCDMSessionProxy::RemoteLegacyCDMSessionProxy(WeakPtr<RemoteLegacyCDMFactoryProxy>&& factory, RemoteLegacyCDMSessionIdentifier identifier, WebCore::LegacyCDM& cdm)
    : m_factory(WTFMove(factory))
    , m_identifier(identifier)
    , m_session(cdm.createSession(*this))
{
}

RemoteLegacyCDMSessionProxy::~RemoteLegacyCDMSessionProxy() = default;

static RefPtr<Uint8Array> convertToUint8Array(IPC::SharedBufferCopy&& buffer)
{
    if (!buffer.buffer())
        return nullptr;

    size_t sizeInBytes = buffer.size();
    auto arrayBuffer = ArrayBuffer::create(buffer.data(), sizeInBytes);
    return Uint8Array::create(WTFMove(arrayBuffer), 0, sizeInBytes);
}

template <typename T>
static Optional<IPC::SharedBufferCopy> convertToOptionalDataReference(T array)
{
    if (!array)
        return WTF::nullopt;
    return { SharedBuffer::create((const char*)array->data(), array->byteLength()) };
}

void RemoteLegacyCDMSessionProxy::generateKeyRequest(const String& mimeType, IPC::SharedBufferCopy&& initData, GenerateKeyCallback&& completion)
{
    auto initDataArray = convertToUint8Array(WTFMove(initData));
    if (!initDataArray) {
        completion({ }, emptyString(), 0, 0);
        return;
    }

    String destinationURL;
    unsigned short errorCode { 0 };
    uint32_t systemCode { 0 };

    auto keyRequest = m_session->generateKeyRequest(mimeType, initDataArray.get(), destinationURL, errorCode, systemCode);

    destinationURL = "this is a test string"_s;

    completion(convertToOptionalDataReference(keyRequest), destinationURL, errorCode, systemCode);
}

void RemoteLegacyCDMSessionProxy::releaseKeys()
{
    m_session->releaseKeys();
}

void RemoteLegacyCDMSessionProxy::update(IPC::SharedBufferCopy&& update, UpdateCallback&& completion)
{
    auto updateArray = convertToUint8Array(WTFMove(update));
    if (!updateArray) {
        completion(false, WTF::nullopt, 0, 0);
        return;
    }

    RefPtr<Uint8Array> nextMessage;
    unsigned short errorCode { 0 };
    uint32_t systemCode { 0 };

    bool succeeded = m_session->update(updateArray.get(), nextMessage, errorCode, systemCode);

    completion(succeeded, convertToOptionalDataReference(nextMessage), errorCode, systemCode);
}

RefPtr<ArrayBuffer> RemoteLegacyCDMSessionProxy::getCachedKeyForKeyId(const String& keyId)
{
    return m_session->cachedKeyForKeyID(keyId);
}

void RemoteLegacyCDMSessionProxy::cachedKeyForKeyID(String keyId, CachedKeyForKeyIDCallback&& completion)
{
    completion(convertToOptionalDataReference(getCachedKeyForKeyId(keyId)));
}

void RemoteLegacyCDMSessionProxy::sendMessage(Uint8Array* message, String destinationURL)
{
    if (!m_factory)
        return;

    m_factory->gpuConnectionToWebProcess().connection().send(Messages::RemoteLegacyCDMSession::SendMessage(convertToOptionalDataReference(message), destinationURL), m_identifier);
}

void RemoteLegacyCDMSessionProxy::sendError(MediaKeyErrorCode errorCode, uint32_t systemCode)
{
    if (m_factory)
        m_factory->gpuConnectionToWebProcess().connection().send(Messages::RemoteLegacyCDMSession::SendError(errorCode, systemCode), m_identifier);
}

String RemoteLegacyCDMSessionProxy::mediaKeysStorageDirectory() const
{
    if (m_factory)
        return m_factory->gpuConnectionToWebProcess().mediaKeysStorageDirectory();
    return emptyString();
}


}

#endif
