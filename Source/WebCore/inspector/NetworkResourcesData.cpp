/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkResourcesData.h"

#include "CachedResource.h"
#include "InspectorNetworkAgent.h"
#include "ResourceResponse.h"
#include "TextResourceDecoder.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkResourcesData);
WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkResourcesData::ResourceData);

using namespace Inspector;

NetworkResourcesData::ResourceData::ResourceData(const String& requestId, const String& loaderId)
    : m_requestId(requestId)
    , m_loaderId(loaderId)
{
}

void NetworkResourcesData::ResourceData::setContent(const String& content, bool base64Encoded)
{
    ASSERT(!hasData());
    ASSERT(!hasContent());
    m_content = content;
    m_base64Encoded = base64Encoded;
}

unsigned NetworkResourcesData::ResourceData::removeContent()
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

unsigned NetworkResourcesData::ResourceData::evictContent()
{
    m_isContentEvicted = true;
    setDecoder(nullptr);
    return removeContent();
}

bool NetworkResourcesData::ResourceData::hasData() const
{
    return !!m_dataBuffer;
}

size_t NetworkResourcesData::ResourceData::dataLength() const
{
    return m_dataBuffer.size();
}

void NetworkResourcesData::ResourceData::appendData(const SharedBuffer& data)
{
    ASSERT(!hasContent());
    m_dataBuffer.append(data);
}

void NetworkResourcesData::ResourceData::decodeDataToContent()
{
    ASSERT(!hasContent());

    auto buffer = m_dataBuffer.takeAsContiguous();

    if (m_decoder) {
        m_base64Encoded = false;
        m_content = m_decoder->decodeAndFlush(buffer->span());
    } else {
        m_base64Encoded = true;
        m_content = base64EncodeToString(buffer->span());
    }
}

NetworkResourcesData::NetworkResourcesData(const Settings& settings)
    : m_settings(settings)
{
}

NetworkResourcesData::~NetworkResourcesData()
{
    clear();
}

void NetworkResourcesData::resourceCreated(const String& requestId, const String& loaderId, InspectorPageAgent::ResourceType type)
{
    ensureNoDataForRequestId(requestId);

    auto resourceData = makeUnique<ResourceData>(requestId, loaderId);
    resourceData->setType(type);
    m_requestIdToResourceDataMap.set(requestId, WTFMove(resourceData));
}

void NetworkResourcesData::resourceCreated(const String& requestId, const String& loaderId, CachedResource& cachedResource)
{
    ensureNoDataForRequestId(requestId);

    auto resourceData = makeUnique<ResourceData>(requestId, loaderId);
    resourceData->setCachedResource(&cachedResource);
    m_requestIdToResourceDataMap.set(requestId, WTFMove(resourceData));
}

void NetworkResourcesData::responseReceived(const String& requestId, const String& frameId, const ResourceResponse& response, InspectorPageAgent::ResourceType type, bool forceBufferData)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;

    resourceData->setFrameId(frameId);
    resourceData->setURL(response.url().string());
    resourceData->setHTTPStatusCode(response.httpStatusCode());
    resourceData->setHTTPStatusText(response.httpStatusText());
    resourceData->setType(type);
    resourceData->setForceBufferData(forceBufferData);
    resourceData->setMIMEType(response.mimeType());
    resourceData->setResponseTimestamp(WallTime::now());

    if (InspectorNetworkAgent::shouldTreatAsText(response.mimeType()))
        resourceData->setDecoder(InspectorNetworkAgent::createTextDecoder(response.mimeType(), response.textEncodingName()));

    if (m_settings.supportsShowingCertificate) {
        if (auto& certificateInfo = response.certificateInfo())
            resourceData->setCertificateInfo(certificateInfo);
    }
}

void NetworkResourcesData::setResourceType(const String& requestId, InspectorPageAgent::ResourceType type)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setType(type);
}

InspectorPageAgent::ResourceType NetworkResourcesData::resourceType(const String& requestId)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return InspectorPageAgent::OtherResource;
    return resourceData->type();
}

void NetworkResourcesData::setResourceContent(const String& requestId, const String& content, bool base64Encoded)
{
    if (content.isNull())
        return;

    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;

    size_t dataLength = content.sizeInBytes();
    if (dataLength > m_settings.maximumSingleResourceContentSize)
        return;
    if (resourceData->isContentEvicted())
        return;

    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        // We can not be sure that we didn't try to save this request data while it was loading, so remove it, if any.
        if (resourceData->hasContent() || resourceData->hasData())
            m_contentSize -= resourceData->removeContent();
        m_requestIdsDeque.appendOrMoveToLast(requestId);
        resourceData->setContent(content, base64Encoded);
        m_contentSize += dataLength;
    }
}

static bool shouldBufferResourceData(const NetworkResourcesData::ResourceData& resourceData)
{
    if (resourceData.forceBufferData())
        return true;

    if (resourceData.decoder())
        return true;

    // Buffer data for Web Inspector when the rest of the system would not normally buffer.
    if (resourceData.cachedResource() && resourceData.cachedResource()->dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData)
        return true;

    return false;
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::maybeAddResourceData(const String& requestId, const SharedBuffer& data)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return nullptr;

    if (!shouldBufferResourceData(*resourceData))
        return resourceData;

    if (resourceData->dataLength() + data.size() > m_settings.maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
    if (resourceData->isContentEvicted())
        return resourceData;

    if (ensureFreeSpace(data.size()) && !resourceData->isContentEvicted()) {
        m_requestIdsDeque.appendOrMoveToLast(requestId);
        resourceData->appendData(data);
        m_contentSize += data.size();
    }

    return resourceData;
}

void NetworkResourcesData::maybeDecodeDataToContent(const String& requestId)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;

    if (!resourceData->hasData())
        return;

    auto byteCount = resourceData->dataLength();
    m_contentSize -= byteCount;

    resourceData->decodeDataToContent();
    byteCount = resourceData->content().sizeInBytes();
    if (byteCount > m_settings.maximumSingleResourceContentSize) {
        resourceData->evictContent();
        return;
    }

    if (ensureFreeSpace(byteCount) && !resourceData->isContentEvicted())
        m_contentSize += byteCount;
}

void NetworkResourcesData::addCachedResource(const String& requestId, CachedResource* cachedResource)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setCachedResource(cachedResource);
}

void NetworkResourcesData::addResourceSharedBuffer(const String& requestId, RefPtr<FragmentedSharedBuffer>&& buffer, const String& textEncodingName)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setBuffer(WTFMove(buffer));
    resourceData->setTextEncodingName(textEncodingName);
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::data(const String& requestId)
{
    return resourceDataForRequestId(requestId);
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::dataForURL(const String& url)
{
    if (url.isNull())
        return nullptr;
    
    NetworkResourcesData::ResourceData* mostRecentResourceData = nullptr;
    
    for (auto* resourceData : resources()) {
        // responseTimestamp is checked so that we only grab the most recent response for the URL, instead of potentionally getting a more stale response.
        if (resourceData->url() == url && resourceData->httpStatusCode() != 304 && (!mostRecentResourceData || (resourceData->responseTimestamp() > mostRecentResourceData->responseTimestamp())))
            mostRecentResourceData = resourceData;
    }
    
    return mostRecentResourceData;
}

Vector<String> NetworkResourcesData::removeCachedResource(CachedResource* cachedResource)
{
    Vector<String> result;
    for (auto& entry : m_requestIdToResourceDataMap) {
        ResourceData* resourceData = entry.value.get();
        if (resourceData->cachedResource() == cachedResource) {
            resourceData->setCachedResource(nullptr);
            result.append(entry.key);
        }
    }

    return result;
}

void NetworkResourcesData::clear(std::optional<String> preservedLoaderId)
{
    if (!preservedLoaderId) {
        m_requestIdToResourceDataMap.clear();
        m_requestIdsDeque.clear();
        m_contentSize = 0;
        return;
    }

    for (auto&& requestId : std::exchange(m_requestIdsDeque, { })) {
        auto resourceData = resourceDataForRequestId(requestId);
        if (!resourceData)
            continue;
        if (resourceData->loaderId() == *preservedLoaderId)
            m_requestIdsDeque.add(requestId);
        else {
            m_contentSize -= resourceData->evictContent();
            m_requestIdToResourceDataMap.remove(requestId);
        }
    }
}

Vector<NetworkResourcesData::ResourceData*> NetworkResourcesData::resources()
{
    return WTF::map(m_requestIdToResourceDataMap.values(), [] (const auto& v) { return v.get(); });
}

NetworkResourcesData::ResourceData* NetworkResourcesData::resourceDataForRequestId(const String& requestId)
{
    if (requestId.isNull())
        return nullptr;
    return m_requestIdToResourceDataMap.get(requestId);
}

void NetworkResourcesData::ensureNoDataForRequestId(const String& requestId)
{
    auto result = m_requestIdToResourceDataMap.take(requestId);
    if (!result)
        return;

    ResourceData* resourceData = result.get();
    if (resourceData->hasContent() || resourceData->hasData())
        m_contentSize -= resourceData->evictContent();
}

bool NetworkResourcesData::ensureFreeSpace(size_t size)
{
    if (size > m_settings.maximumResourcesContentSize)
        return false;

    ASSERT(m_settings.maximumResourcesContentSize >= m_contentSize);
    while (size > m_settings.maximumResourcesContentSize - m_contentSize) {
        String requestId = m_requestIdsDeque.takeFirst();
        ResourceData* resourceData = resourceDataForRequestId(requestId);
        if (resourceData)
            m_contentSize -= resourceData->evictContent();
    }
    return true;
}

} // namespace WebCore
