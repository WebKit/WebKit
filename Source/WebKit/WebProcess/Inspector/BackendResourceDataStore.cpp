/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BackendResourceDataStore.h"

#include <WebCore/CertificateInfo.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BackendResourceDataStore);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BackendResourceDataStore::ResourceData);

// --- ResourceData ---

BackendResourceDataStore::ResourceData::ResourceData(ResourceID resourceID, Inspector::ResourceType type)
    : m_resourceID(resourceID)
    , m_type(type)
{
}

BackendResourceDataStore::ResourceData::~ResourceData() = default;

void BackendResourceDataStore::ResourceData::setContent(const String& content, bool base64Encoded)
{
    ASSERT(!hasData());
    ASSERT(!hasContent());
    m_content = content;
    m_base64Encoded = base64Encoded;
}

unsigned BackendResourceDataStore::ResourceData::removeContent()
{
    unsigned result = 0;
    if (hasData()) {
        ASSERT(!hasContent());
        result = m_dataBuffer.size();
        m_dataBuffer.reset();
    }

    if (hasContent()) {
        ASSERT(!hasData());
        result = m_content.sizeInBytes();
        m_content = String();
    }
    return result;
}

unsigned BackendResourceDataStore::ResourceData::evictContent()
{
    m_isContentEvicted = true;
    setDecoder(nullptr);
    return removeContent();
}

bool BackendResourceDataStore::ResourceData::hasData() const
{
    return !!m_dataBuffer;
}

size_t BackendResourceDataStore::ResourceData::dataLength() const
{
    return m_dataBuffer.size();
}

void BackendResourceDataStore::ResourceData::appendData(const SharedBuffer& data)
{
    ASSERT(!hasContent());
    m_dataBuffer.append(data);
}

void BackendResourceDataStore::ResourceData::decodeDataToContent()
{
    ASSERT(!hasContent());

    auto buffer = m_dataBuffer.takeBufferAsContiguous();

    if (RefPtr decoder = m_decoder) {
        m_base64Encoded = false;
        m_content = decoder->decodeAndFlush(buffer->span());
    } else {
        m_base64Encoded = true;
        m_content = base64EncodeToString(buffer->span());
    }
}

void BackendResourceDataStore::ResourceData::setCertificateInfo(std::unique_ptr<CertificateInfo>&& info)
{
    m_certificateInfo = WTF::move(info);
}

// --- BackendResourceDataStore ---

BackendResourceDataStore::BackendResourceDataStore(const Settings& settings)
    : m_settings(settings)
{
}

BackendResourceDataStore::~BackendResourceDataStore()
{
    clear();
}

BackendResourceDataStore::ResourceData* BackendResourceDataStore::resourceDataForId(ResourceLoaderIdentifier resourceID)
{
    auto it = m_resourceDataMap.find(resourceID);
    if (it == m_resourceDataMap.end())
        return nullptr;
    return &it->value.get();
}

void BackendResourceDataStore::ensureNoDataForId(ResourceLoaderIdentifier resourceID)
{
    auto it = m_resourceDataMap.find(resourceID);
    if (it == m_resourceDataMap.end())
        return;

    ResourceData& resourceData = it->value.get();
    if (resourceData.hasContent() || resourceData.hasData())
        m_contentSize -= resourceData.evictContent();
    m_resourceDataMap.remove(it);
}

bool BackendResourceDataStore::ensureFreeSpace(size_t size)
{
    if (size > m_settings.maximumResourcesContentSize)
        return false;

    while (m_contentSize > m_settings.maximumResourcesContentSize - size) {
        auto resourceID = m_resourceIdsDeque.takeFirst();
        ResourceData* resourceData = resourceDataForId(resourceID);
        if (resourceData)
            m_contentSize -= resourceData->evictContent();
    }
    return true;
}

void BackendResourceDataStore::resourceCreated(ResourceLoaderIdentifier resourceID, Inspector::ResourceType type)
{
    ensureNoDataForId(resourceID);
    m_resourceDataMap.set(resourceID, makeUniqueRef<ResourceData>(resourceID, type));
}

void BackendResourceDataStore::responseReceived(ResourceLoaderIdentifier resourceID, const ResourceResponse& response, Inspector::ResourceType type)
{
    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return;

    resourceData->setURL(response.url().string());
    resourceData->setHTTPStatusCode(response.httpStatusCode());
    resourceData->setHTTPStatusText(response.httpStatusText());
    resourceData->setType(type);
    resourceData->setMIMEType(response.mimeType());
    resourceData->m_responseTimestamp = WallTime::now();

    if (ResourceUtilities::shouldTreatAsText(response.mimeType()))
        resourceData->setDecoder(ResourceUtilities::createTextDecoder(response.mimeType(), response.textEncodingName()));

    if (m_settings.supportsShowingCertificate) {
        if (auto& certificateInfo = response.certificateInfo())
            resourceData->setCertificateInfo(makeUniqueWithoutFastMallocCheck<CertificateInfo>(certificateInfo.value()));
    }
}

void BackendResourceDataStore::setResourceType(ResourceLoaderIdentifier resourceID, Inspector::ResourceType type)
{
    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return;
    resourceData->setType(type);
}

void BackendResourceDataStore::setResourceContent(ResourceLoaderIdentifier resourceID, const String& content, bool base64Encoded)
{
    if (content.isNull())
        return;

    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return;

    size_t dataLength = content.sizeInBytes();
    if (dataLength > m_settings.maximumSingleResourceContentSize)
        return;
    if (resourceData->isContentEvicted())
        return;

    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        if (resourceData->hasContent() || resourceData->hasData())
            m_contentSize -= resourceData->removeContent();
        m_resourceIdsDeque.appendOrMoveToLast(resourceID);
        resourceData->setContent(content, base64Encoded);
        m_contentSize += dataLength;
    }
}

// Always buffer data — we don't hold CachedResource references, so this store
// is the only place response content is preserved for Inspector.
BackendResourceDataStore::ResourceData const* BackendResourceDataStore::maybeAddResourceData(ResourceLoaderIdentifier resourceID, const SharedBuffer& data)
{
    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return nullptr;

    if (resourceData->dataLength() + data.size() > m_settings.maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
    if (resourceData->isContentEvicted())
        return resourceData;

    if (ensureFreeSpace(data.size()) && !resourceData->isContentEvicted()) {
        m_resourceIdsDeque.appendOrMoveToLast(resourceID);
        resourceData->appendData(data);
        m_contentSize += data.size();
    }

    return resourceData;
}

void BackendResourceDataStore::maybeDecodeDataToContent(ResourceLoaderIdentifier resourceID)
{
    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return;

    if (!resourceData->hasData())
        return;

    auto rawByteCount = resourceData->dataLength();
    m_contentSize -= rawByteCount;

    resourceData->decodeDataToContent();
    auto decodedByteCount = resourceData->content().sizeInBytes();
    if (decodedByteCount > m_settings.maximumSingleResourceContentSize) {
        resourceData->evictContent();
        return;
    }

    // Add the decoded size to m_contentSize before calling ensureFreeSpace so that
    // if ensureFreeSpace evicts THIS resource, the accounting remains consistent.
    // ensureFreeSpace(0) triggers eviction when m_contentSize exceeds the limit.
    m_contentSize += decodedByteCount;
    ensureFreeSpace(0);
    // If we were evicted by ensureFreeSpace, m_contentSize was already adjusted.
    if (resourceData->isContentEvicted())
        m_contentSize -= decodedByteCount;
    else
        m_resourceIdsDeque.appendOrMoveToLast(resourceID);
}

void BackendResourceDataStore::addResourceSharedBuffer(ResourceLoaderIdentifier resourceID, RefPtr<FragmentedSharedBuffer>&& buffer, const String& textEncodingName)
{
    ResourceData* resourceData = resourceDataForId(resourceID);
    if (!resourceData)
        return;
    resourceData->setBuffer(WTF::move(buffer));
    resourceData->setTextEncodingName(textEncodingName);
}

BackendResourceDataStore::ResourceData const* BackendResourceDataStore::data(ResourceLoaderIdentifier resourceID)
{
    return resourceDataForId(resourceID);
}

void BackendResourceDataStore::clear()
{
    m_resourceDataMap.clear();
    m_resourceIdsDeque.clear();
    m_contentSize = 0;
}

Expected<std::tuple<String, bool>, String> BackendResourceDataStore::getResponseBody(ResourceLoaderIdentifier resourceID)
{
    ResourceData const* resourceData = data(resourceID);
    if (!resourceData)
        return makeUnexpected("Missing resource for given requestId"_s);

    if (resourceData->hasContent())
        return std::tuple<String, bool> { resourceData->content(), resourceData->base64Encoded() };

    if (resourceData->isContentEvicted())
        return makeUnexpected("Resource content was evicted from inspector cache"_s);

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        String body;
        if (ResourceUtilities::sharedBufferContent(resourceData->buffer(), resourceData->textEncodingName(), false, &body))
            return std::tuple<String, bool> { body, false };
    }

    return makeUnexpected("Missing content of resource for given requestId"_s);
}

} // namespace WebKit
