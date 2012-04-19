/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "NetworkResourcesData.h"

#include "DOMImplementation.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace {
// 10MB
static int maximumResourcesContentSize = 10 * 1000 * 1000;

// 1MB
static int maximumSingleResourceContentSize = 1000 * 1000;
}

namespace WebCore {


// ResourceData
NetworkResourcesData::ResourceData::ResourceData(const String& requestId, const String& loaderId)
    : m_requestId(requestId)
    , m_loaderId(loaderId)
    , m_isContentPurged(false)
    , m_type(InspectorPageAgent::OtherResource)
{
}

void NetworkResourcesData::ResourceData::setContent(const String& content)
{
    ASSERT(!hasData());
    ASSERT(!hasContent());
    m_content = content;
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
        result = 2 * m_content.length();
        m_content = String();
    }
    return result;
}

unsigned NetworkResourcesData::ResourceData::purgeContent()
{
    m_isContentPurged = true;
    return removeContent();
}

int NetworkResourcesData::ResourceData::dataLength() const
{
    return m_dataBuffer ? m_dataBuffer->size() : 0;
}

void NetworkResourcesData::ResourceData::appendData(const char* data, int dataLength)
{
    ASSERT(!hasContent());
    if (!m_dataBuffer)
        m_dataBuffer = SharedBuffer::create(data, dataLength);
    else
        m_dataBuffer->append(data, dataLength);
}

int NetworkResourcesData::ResourceData::decodeDataToContent()
{
    ASSERT(!hasContent());
    int dataLength = m_dataBuffer->size();
    m_content = m_decoder->decode(m_dataBuffer->data(), m_dataBuffer->size());
    m_content += m_decoder->flush();
    m_dataBuffer = nullptr;
    return 2 * m_content.length() - dataLength;
}

// NetworkResourcesData
NetworkResourcesData::NetworkResourcesData()
    : m_contentSize(0)
    , m_maximumResourcesContentSize(maximumResourcesContentSize)
    , m_maximumSingleResourceContentSize(maximumSingleResourceContentSize)
{
}

NetworkResourcesData::~NetworkResourcesData()
{
    clear();
}

void NetworkResourcesData::resourceCreated(const String& requestId, const String& loaderId)
{
    ensureNoDataForRequestId(requestId);
    m_requestIdToResourceDataMap.set(requestId, new ResourceData(requestId, loaderId));
}

static PassRefPtr<TextResourceDecoder> createOtherResourceTextDecoder(const String& mimeType, const String& textEncodingName)
{
    RefPtr<TextResourceDecoder> decoder;
    if (!textEncodingName.isEmpty())
        decoder = TextResourceDecoder::create("text/plain", textEncodingName);
    else if (DOMImplementation::isXMLMIMEType(mimeType.lower())) {
        decoder = TextResourceDecoder::create("application/xml");
        decoder->useLenientXMLDecoding();
    } else if (equalIgnoringCase(mimeType, "text/html"))
        decoder = TextResourceDecoder::create("text/html", "UTF-8");
    else if (mimeType == "text/plain")
        decoder = TextResourceDecoder::create("text/plain", "ISO-8859-1");
    return decoder;
}

void NetworkResourcesData::responseReceived(const String& requestId, const String& frameId, const ResourceResponse& response)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    resourceData->setFrameId(frameId);
    resourceData->setUrl(response.url());
    resourceData->setDecoder(createOtherResourceTextDecoder(response.mimeType(), response.textEncodingName()));
    resourceData->setHTTPStatusCode(response.httpStatusCode());
}

void NetworkResourcesData::setResourceType(const String& requestId, InspectorPageAgent::ResourceType type)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    resourceData->setType(type);
}

InspectorPageAgent::ResourceType NetworkResourcesData::resourceType(const String& requestId)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return InspectorPageAgent::OtherResource;
    return resourceData->type();
}

void NetworkResourcesData::setResourceContent(const String& requestId, const String& content)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    int dataLength = 2 * content.length();
    if (dataLength > m_maximumSingleResourceContentSize)
        return;
    if (resourceData->isContentPurged())
        return;
    if (ensureFreeSpace(dataLength) && !resourceData->isContentPurged()) {
        // We can not be sure that we didn't try to save this request data while it was loading, so remove it, if any.
        if (resourceData->hasContent())
            m_contentSize -= resourceData->removeContent();
        m_requestIdsDeque.append(requestId);
        resourceData->setContent(content);
        m_contentSize += dataLength;
    }
}

void NetworkResourcesData::maybeAddResourceData(const String& requestId, const char* data, int dataLength)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    if (!resourceData->decoder())
        return;
    if (resourceData->dataLength() + dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->purgeContent();
    if (resourceData->isContentPurged())
        return;
    if (ensureFreeSpace(dataLength) && !resourceData->isContentPurged()) {
        m_requestIdsDeque.append(requestId);
        resourceData->appendData(data, dataLength);
        m_contentSize += dataLength;
    }
}

void NetworkResourcesData::maybeDecodeDataToContent(const String& requestId)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    if (!resourceData->hasData())
        return;
    m_contentSize += resourceData->decodeDataToContent();
    int dataLength = 2 * resourceData->content().length();
    if (dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->purgeContent();
}

void NetworkResourcesData::addCachedResource(const String& requestId, CachedResource* cachedResource)
{
    if (!m_requestIdToResourceDataMap.contains(requestId))
        return;
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);

    resourceData->setCachedResource(cachedResource);
}

void NetworkResourcesData::addResourceSharedBuffer(const String& requestId, PassRefPtr<SharedBuffer> buffer, const String& textEncodingName)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (!resourceData)
        return;
    resourceData->setBuffer(buffer);
    resourceData->setTextEncodingName(textEncodingName);
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::data(const String& requestId)
{
    return m_requestIdToResourceDataMap.get(requestId);
}

void NetworkResourcesData::clear(const String& preservedLoaderId)
{
    m_requestIdsDeque.clear();
    m_contentSize = 0;

    ResourceDataMap preservedMap;

    ResourceDataMap::iterator it;
    ResourceDataMap::iterator end = m_requestIdToResourceDataMap.end();
    for (it = m_requestIdToResourceDataMap.begin(); it != end; ++it) {
        ResourceData* resourceData = it->second;
        if (!preservedLoaderId.isNull() && resourceData->loaderId() == preservedLoaderId)
            preservedMap.set(it->first, it->second);
        else
            delete resourceData;
    }
    m_requestIdToResourceDataMap.swap(preservedMap);
}

void NetworkResourcesData::setResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize)
{
    clear();
    m_maximumResourcesContentSize = maximumResourcesContentSize;
    m_maximumSingleResourceContentSize = maximumSingleResourceContentSize;
}


void NetworkResourcesData::ensureNoDataForRequestId(const String& requestId)
{
    ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
    if (resourceData) {
        if (resourceData->hasContent() || resourceData->hasData())
            m_contentSize -= resourceData->purgeContent();
        delete resourceData;
        m_requestIdToResourceDataMap.remove(requestId);
    }
}

bool NetworkResourcesData::ensureFreeSpace(int size)
{
    if (size > m_maximumResourcesContentSize)
        return false;

    while (size > m_maximumResourcesContentSize - m_contentSize) {
        String requestId = m_requestIdsDeque.takeFirst();
        ResourceData* resourceData = m_requestIdToResourceDataMap.get(requestId);
        if (resourceData)
            m_contentSize -= resourceData->purgeContent();
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
