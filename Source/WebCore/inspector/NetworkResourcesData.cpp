/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace Inspector;

static const size_t maximumResourcesContentSize = 200 * 1000 * 1000; // 200MB
static const size_t maximumSingleResourceContentSize = 50 * 1000 * 1000; // 50MB

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
        result = m_dataBuffer->size();
        m_dataBuffer = nullptr;
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
    return removeContent();
}

size_t NetworkResourcesData::ResourceData::dataLength() const
{
    return m_dataBuffer ? m_dataBuffer->size() : 0;
}

void NetworkResourcesData::ResourceData::appendData(const char* data, size_t dataLength)
{
    ASSERT(!hasContent());
    if (!m_dataBuffer)
        m_dataBuffer = SharedBuffer::create(data, dataLength);
    else
        m_dataBuffer->append(data, dataLength);
}

unsigned NetworkResourcesData::ResourceData::decodeDataToContent()
{
    ASSERT(!hasContent());

    size_t dataLength = m_dataBuffer->size();

    if (m_decoder) {
        m_base64Encoded = false;
        m_content = m_decoder->decodeAndFlush(m_dataBuffer->data(), dataLength);
    } else {
        m_base64Encoded = true;
        m_content = base64Encode(m_dataBuffer->data(), dataLength);
    }

    m_dataBuffer = nullptr;

    return m_content.sizeInBytes() - dataLength;
}

NetworkResourcesData::NetworkResourcesData()
    : m_maximumResourcesContentSize(maximumResourcesContentSize)
    , m_maximumSingleResourceContentSize(maximumSingleResourceContentSize)
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

    if (auto& certificateInfo = response.certificateInfo())
        resourceData->setCertificateInfo(certificateInfo);
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
    if (dataLength > m_maximumSingleResourceContentSize)
        return;
    if (resourceData->isContentEvicted())
        return;

    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        // We can not be sure that we didn't try to save this request data while it was loading, so remove it, if any.
        if (resourceData->hasContent() || resourceData->hasData())
            m_contentSize -= resourceData->removeContent();
        m_requestIdsDeque.append(requestId);
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

NetworkResourcesData::ResourceData const* NetworkResourcesData::maybeAddResourceData(const String& requestId, const char* data, size_t dataLength)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return nullptr;

    if (!shouldBufferResourceData(*resourceData))
        return resourceData;

    if (resourceData->dataLength() + dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
    if (resourceData->isContentEvicted())
        return resourceData;

    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        m_requestIdsDeque.append(requestId);
        resourceData->appendData(data, dataLength);
        m_contentSize += dataLength;
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

    m_contentSize += resourceData->decodeDataToContent();
    size_t dataLength = resourceData->content().sizeInBytes();
    if (dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
}

void NetworkResourcesData::addCachedResource(const String& requestId, CachedResource* cachedResource)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setCachedResource(cachedResource);
}

void NetworkResourcesData::addResourceSharedBuffer(const String& requestId, RefPtr<SharedBuffer>&& buffer, const String& textEncodingName)
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

void NetworkResourcesData::clear(Optional<String> preservedLoaderId)
{
    m_requestIdsDeque.clear();
    m_contentSize = 0;

    if (!preservedLoaderId)
        m_requestIdToResourceDataMap.clear();
    else {
        m_requestIdToResourceDataMap.removeIf([loaderId = *preservedLoaderId] (auto& entry) {
            return entry.value->loaderId() != loaderId;
        });
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
    if (size > m_maximumResourcesContentSize)
        return false;

    while (size > m_maximumResourcesContentSize - m_contentSize) {
        String requestId = m_requestIdsDeque.takeFirst();
        ResourceData* resourceData = resourceDataForRequestId(requestId);
        if (resourceData)
            m_contentSize -= resourceData->evictContent();
    }
    return true;
}

} // namespace WebCore
