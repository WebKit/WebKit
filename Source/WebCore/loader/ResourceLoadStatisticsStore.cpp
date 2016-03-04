/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsStore.h"

#include "KeyedCoding.h"
#include "Logging.h"
#include "NetworkStorageSession.h"
#include "PlatformStrategies.h"
#include "ResourceLoadStatistics.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#define LOG_STATISTICS_TO_FILE 0

namespace WebCore {

Ref<ResourceLoadStatisticsStore> ResourceLoadStatisticsStore::create(const String& resourceLoadStatisticsDirectory)
{
    return adoptRef(*new ResourceLoadStatisticsStore(resourceLoadStatisticsDirectory));
}

Ref<ResourceLoadStatisticsStore> ResourceLoadStatisticsStore::create()
{
    return adoptRef(*new ResourceLoadStatisticsStore());
}
    
ResourceLoadStatisticsStore::ResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory)
    : m_storagePath(resourceLoadStatisticsDirectory)
{
}
    
bool ResourceLoadStatisticsStore::isPrevalentResource(const String& primaryDomain) const
{
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;

    return mapEntry->value.isPrevalentResource;
}
    
ResourceLoadStatistics& ResourceLoadStatisticsStore::resourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    auto addResult = m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    });

    return addResult.iterator->value;
}

typedef HashMap<String, ResourceLoadStatistics>::KeyValuePairType StatisticsValue;

void ResourceLoadStatisticsStore::writeDataToDisk()
{
    auto encoder = KeyedEncoder::encoder();
    
    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [this](KeyedEncoder& encoderInner, const StatisticsValue& origin) {
        origin.value.encode(encoderInner);
    });
    
    writeEncoderToDisk(*encoder.get(), "full_browsing_session");
}

void ResourceLoadStatisticsStore::setStatisticsStorageDirectory(const String& path)
{
    m_storagePath = path;
    readDataFromDiskIfNeeded();
}

String ResourceLoadStatisticsStore::persistentStoragePath(const String& label) const
{
    if (m_storagePath.isEmpty())
        return emptyString();

    // TODO Decide what to call this file
    return pathByAppendingComponent(m_storagePath, label + "_resourceLog.plist");
}

void ResourceLoadStatisticsStore::readDataFromDiskIfNeeded()
{
    if (m_resourceStatisticsMap.size())
        return;

    auto decoder = createDecoderFromDisk("full_browsing_session");
    if (!decoder)
        return;

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder->decodeObjects("browsingStatistics", loadedStatistics, [this](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner);
    });

    if (!succeeded)
        return;

    for (auto& statistics : loadedStatistics)
        m_resourceStatisticsMap.set(statistics.highLevelDomain, statistics);
}

std::unique_ptr<KeyedDecoder> ResourceLoadStatisticsStore::createDecoderFromDisk(const String& label) const
{
    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return nullptr;
    
    RefPtr<SharedBuffer> rawData = SharedBuffer::createWithContentsOfFile(resourceLog);
    if (!rawData)
        return nullptr;
    
    return KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(rawData->data()), rawData->size());
}
    
void ResourceLoadStatisticsStore::writeEncoderToDisk(KeyedEncoder& encoder, const String& label) const
{
#if LOG_STATISTICS_TO_FILE
    RefPtr<SharedBuffer> rawData = encoder.finishEncoding();
    if (!rawData)
        return;
    
    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return;

    if (!m_storagePath.isEmpty())
        makeAllDirectories(m_storagePath);

    auto handle = openFile(resourceLog, OpenForWrite);
    if (!handle)
        return;
    
    int64_t writtenBytes = writeToFile(handle, rawData->data(), rawData->size());
    closeFile(handle);
    
    if (writtenBytes != static_cast<int64_t>(rawData->size()))
        WTFLogAlways("ResourceLoadStatistics: We only wrote %lld out of %d bytes to disk", writtenBytes, rawData->size());
#else
    UNUSED_PARAM(encoder);
    UNUSED_PARAM(label);
#endif
}

String ResourceLoadStatisticsStore::statisticsForOrigin(const String& origin)
{
    auto iter = m_resourceStatisticsMap.find(origin);
    if (iter == m_resourceStatisticsMap.end())
        return emptyString();
    
    return "Statistics for " + origin + ":\n" + iter->value.toString();
}

Vector<ResourceLoadStatistics> ResourceLoadStatisticsStore::takeStatistics()
{
    Vector<ResourceLoadStatistics> statistics;
    statistics.reserveInitialCapacity(m_resourceStatisticsMap.size());
    for (auto& statistic : m_resourceStatisticsMap.values())
        statistics.uncheckedAppend(WTFMove(statistic));

    m_resourceStatisticsMap.clear();

    return statistics;
}

void ResourceLoadStatisticsStore::mergeStatistics(const Vector<ResourceLoadStatistics>& statistics)
{
    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return ResourceLoadStatistics(statistic.highLevelDomain);
        });
        
        result.iterator->value.merge(statistic);
    }
}

void ResourceLoadStatisticsStore::setNotificationCallback(std::function<void()> handler)
{
    m_dataAddedHandler = WTFMove(handler);
}

void ResourceLoadStatisticsStore::fireDataModificationHandler()
{
    m_dataAddedHandler();
}

}
