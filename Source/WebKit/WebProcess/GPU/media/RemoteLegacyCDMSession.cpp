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
#include "RemoteLegacyCDMSession.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

static RefPtr<ArrayBuffer> convertToArrayBuffer(RefPtr<const SharedBuffer>&& buffer)
{
    if (buffer)
        return buffer->tryCreateArrayBuffer();
    return nullptr;
}

static RefPtr<Uint8Array> convertToUint8Array(RefPtr<const SharedBuffer>&& buffer)
{
    auto arrayBuffer = convertToArrayBuffer(WTFMove(buffer));
    if (!arrayBuffer)
        return nullptr;

    size_t sizeInBytes = arrayBuffer->byteLength();
    return Uint8Array::create(WTFMove(arrayBuffer), 0, sizeInBytes);
}

template <typename T>
static RefPtr<SharedBuffer> convertToSharedBuffer(T array)
{
    if (!array)
        return nullptr;
    return SharedBuffer::create(array->data(), array->byteLength());
}

std::unique_ptr<RemoteLegacyCDMSession> RemoteLegacyCDMSession::create(WeakPtr<RemoteLegacyCDMFactory> factory, RemoteLegacyCDMSessionIdentifier&& identifier)
{
    return std::unique_ptr<RemoteLegacyCDMSession>(new RemoteLegacyCDMSession(WTFMove(factory), WTFMove(identifier)));
}

RemoteLegacyCDMSession::RemoteLegacyCDMSession(WeakPtr<RemoteLegacyCDMFactory> factory, RemoteLegacyCDMSessionIdentifier&& identifier)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(identifier))
{
}

RemoteLegacyCDMSession::~RemoteLegacyCDMSession() = default;

RefPtr<Uint8Array> RemoteLegacyCDMSession::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    if (!m_factory || !initData)
        return nullptr;

    auto ipcInitData = convertToSharedBuffer(initData);
    RefPtr<SharedBuffer> ipcNextMessage;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMSessionProxy::GenerateKeyRequest(mimeType, ipcInitData), Messages::RemoteLegacyCDMSessionProxy::GenerateKeyRequest::Reply(ipcNextMessage, destinationURL, errorCode, systemCode), m_identifier);

    if (!ipcNextMessage)
        return nullptr;

    return convertToUint8Array(WTFMove(ipcNextMessage));
}

void RemoteLegacyCDMSession::releaseKeys()
{
    if (!m_factory)
        return;

    m_factory->gpuProcessConnection().connection().send(Messages::RemoteLegacyCDMSessionProxy::ReleaseKeys(), m_identifier);
    m_cachedKeyCache.clear();
}

bool RemoteLegacyCDMSession::update(Uint8Array* keyData, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    if (!m_factory || !keyData)
        return false;

    auto ipcKeyData = convertToSharedBuffer(keyData);
    bool succeeded { false };
    RefPtr<SharedBuffer> ipcNextMessage;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMSessionProxy::Update(ipcKeyData), Messages::RemoteLegacyCDMSessionProxy::Update::Reply(succeeded, ipcNextMessage, errorCode, systemCode), m_identifier);

    if (ipcNextMessage)
        nextMessage = convertToUint8Array(WTFMove(ipcNextMessage));

    return succeeded;
}

RefPtr<ArrayBuffer> RemoteLegacyCDMSession::cachedKeyForKeyID(const String& keyId) const
{
    if (!m_factory)
        return nullptr;

    auto foundInCache = m_cachedKeyCache.find(keyId);
    if (foundInCache != m_cachedKeyCache.end())
        return foundInCache->value;

    RefPtr<SharedBuffer> ipcKey;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteLegacyCDMSessionProxy::CachedKeyForKeyID(keyId), Messages::RemoteLegacyCDMSessionProxy::CachedKeyForKeyID::Reply(ipcKey), m_identifier);

    if (!ipcKey)
        return nullptr;

    auto ipcKeyBuffer = convertToArrayBuffer(WTFMove(ipcKey));
    m_cachedKeyCache.set(keyId, ipcKeyBuffer);
    return ipcKeyBuffer;
}

void RemoteLegacyCDMSession::sendMessage(RefPtr<SharedBuffer>&& message, const String& destinationURL)
{
    if (!m_client)
        return;

    if (!message) {
        m_client->sendMessage(nullptr, destinationURL);
        return;
    }

    m_client->sendMessage(convertToUint8Array(WTFMove(message)).get(), destinationURL);
}

void RemoteLegacyCDMSession::sendError(WebCore::LegacyCDMSessionClient::MediaKeyErrorCode errorCode, uint32_t systemCode)
{
    if (m_client)
        m_client->sendError(errorCode, systemCode);
}

}

#endif
