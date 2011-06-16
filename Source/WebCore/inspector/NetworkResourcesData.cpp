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
#include "NetworkResourcesData.h"

#if ENABLE(INSPECTOR)

namespace {
// 10MB
static int maximumResourcesContentSize = 10 * 1000 * 1000;
}

namespace WebCore {

// ResourceData
NetworkResourcesData::ResourceData::ResourceData(unsigned long identifier, const String& loaderId)
    : m_identifier(identifier)
    , m_loaderId(loaderId)
    , m_hasContent(false)
    , m_isXHR(false)
    , m_isContentPurged(false)
{
}

String NetworkResourcesData::ResourceData::content()
{
    return m_hasContent ? m_contentBuilder.toString() : String();
}

void NetworkResourcesData::ResourceData::appendContent(const String& content)
{
    m_contentBuilder.append(content);
    m_hasContent = true;
}

unsigned NetworkResourcesData::ResourceData::purgeContent()
{
    unsigned length = m_contentBuilder.toStringPreserveCapacity().length();
    m_contentBuilder.clear();
    m_isContentPurged = true;
    m_hasContent = false;
    return length;
}

// NetworkResourcesData
NetworkResourcesData::NetworkResourcesData()
    : m_contentSize(0)
{
}

NetworkResourcesData::~NetworkResourcesData()
{
    clear();
}

void NetworkResourcesData::resourceCreated(unsigned long identifier, const String& loaderId)
{
    ensureNoDataForIdentifier(identifier);
    m_identifierToResourceDataMap.set(identifier, new ResourceData(identifier, loaderId));
}

void NetworkResourcesData::responseReceived(unsigned long identifier, const String& frameId, const String& url)
{
    ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
    if (!resourceData)
        return;
    resourceData->setFrameId(frameId);
    resourceData->setUrl(url);
}

void NetworkResourcesData::didReceiveXHRResponse(unsigned long identifier)
{
    ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
    if (!resourceData)
        return;
    resourceData->setIsXHR(true);
}

void NetworkResourcesData::addResourceContent(unsigned long identifier, const String& content)
{
    ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
    if (!resourceData)
        return;
    if (resourceData->isContentPurged())
        return;
    if (ensureFreeSpace(content.length()) && !resourceData->isContentPurged()) {
        if (!resourceData->hasContent())
            m_identifiersDeque.append(identifier);
        resourceData->appendContent(content);
        m_contentSize += content.length();
    }
}

bool NetworkResourcesData::isXHR(unsigned long identifier)
{
    ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
    if (!resourceData)
        return false;
    return resourceData->isXHR();
}

NetworkResourcesData::ResourceData* NetworkResourcesData::data(unsigned long identifier)
{
    return m_identifierToResourceDataMap.get(identifier);
}

void NetworkResourcesData::clear(const String& preservedLoaderId)
{
    m_identifiersDeque.clear();

    ResourceDataMap preservedMap;

    ResourceDataMap::iterator it;
    ResourceDataMap::iterator end = m_identifierToResourceDataMap.end();
    for (it = m_identifierToResourceDataMap.begin(); it != end; ++it) {
        ResourceData* resourceData = it->second;
        if (!preservedLoaderId.isNull() && resourceData->loaderId() == preservedLoaderId)
            preservedMap.set(it->first, it->second);
        else
            delete resourceData;
    }
    m_identifierToResourceDataMap.swap(preservedMap);
}

void NetworkResourcesData::ensureNoDataForIdentifier(unsigned long identifier)
{
    ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
    if (resourceData) {
        if (resourceData->hasContent())
            m_contentSize -= resourceData->purgeContent();
        delete resourceData;
        m_identifierToResourceDataMap.remove(identifier);
    }
}

bool NetworkResourcesData::ensureFreeSpace(int size)
{
    if (size > maximumResourcesContentSize)
        return false;

    while (size > maximumResourcesContentSize - m_contentSize) {
        unsigned long identifier = m_identifiersDeque.takeFirst();
        ResourceData* resourceData = m_identifierToResourceDataMap.get(identifier);
        if (resourceData)
            m_contentSize -= resourceData->purgeContent();
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
