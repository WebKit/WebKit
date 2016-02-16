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
#include "ResourceLoadObserver.h"

#include "Document.h"
#include "KeyedCoding.h"
#include "Logging.h"
#include "NetworkStorageSession.h"
#include "PlatformStrategies.h"
#include "ResourceLoadStatistics.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#define LOG_STATISTICS_TO_FILE 0

namespace WebCore {

ResourceLoadObserver& ResourceLoadObserver::sharedObserver()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}
    
void ResourceLoadObserver::logFrameNavigation(bool isRedirect, const URL& sourceURL, const URL& targetURL, bool isMainFrame, const URL& mainFrameURL)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    if (!targetURL.isValid() || !mainFrameURL.isValid())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || targetPrimaryDomain == sourcePrimaryDomain)
        return;
    
    auto targetOrigin = SecurityOrigin::create(targetURL);
    auto& targetStatistics = resourceStatisticsForPrimaryDomain(targetPrimaryDomain);
    
    if (isMainFrame)
        targetStatistics.topFrameHasBeenNavigatedToBefore = true;
    else {
        targetStatistics.subframeHasBeenLoadedBefore = true;

        auto mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
        targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain);
    }
    
    if (isRedirect) {
        auto& redirectingOriginResourceStatistics = resourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        
        if (isPrevalentResource(targetPrimaryDomain))
            redirectingOriginResourceStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);
        
        if (isMainFrame) {
            ++targetStatistics.topFrameHasBeenRedirectedTo;
            ++redirectingOriginResourceStatistics.topFrameHasBeenRedirectedFrom;
        } else {
            ++targetStatistics.subframeHasBeenRedirectedTo;
            ++redirectingOriginResourceStatistics.subframeHasBeenRedirectedFrom;
            redirectingOriginResourceStatistics.subframeUniqueRedirectsTo.add(targetPrimaryDomain);
            
            ++targetStatistics.subframeSubResourceCount;
        }
    } else {
        if (sourcePrimaryDomain.isNull() || sourcePrimaryDomain.isEmpty() || sourcePrimaryDomain == "nullOrigin") {
            if (isMainFrame)
                ++targetStatistics.topFrameInitialLoadCount;
            else
                ++targetStatistics.subframeSubResourceCount;
        } else {
            auto& sourceOriginResourceStatistics = resourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

            if (isMainFrame) {
                ++sourceOriginResourceStatistics.topFrameHasBeenNavigatedFrom;
                ++targetStatistics.topFrameHasBeenNavigatedTo;
            } else {
                ++sourceOriginResourceStatistics.subframeHasBeenNavigatedFrom;
                ++targetStatistics.subframeHasBeenNavigatedTo;
            }
        }
    }

    targetStatistics.checkAndSetAsPrevalentResourceIfNecessary(m_resourceStatisticsMap.size());
}
    
void ResourceLoadObserver::logSubresourceLoading(bool isRedirect, const URL& sourceURL, const URL& targetURL, const URL& mainFrameURL)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || targetPrimaryDomain == sourcePrimaryDomain)
        return;

    auto& targetStatistics = resourceStatisticsForPrimaryDomain(targetPrimaryDomain);

    auto mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
    targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);

    if (isRedirect) {
        auto& redirectingOriginStatistics = resourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        
        if (isPrevalentResource(targetPrimaryDomain))
            redirectingOriginStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);
        
        ++redirectingOriginStatistics.subresourceHasBeenRedirectedFrom;
        ++targetStatistics.subresourceHasBeenRedirectedTo;

        redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain);

        ++targetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
        
        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    } else {
        ++targetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
        
        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    }
    
    targetStatistics.checkAndSetAsPrevalentResourceIfNecessary(m_resourceStatisticsMap.size());
}
    
void ResourceLoadObserver::logUserInteraction(const Document& document)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    auto& statistics = resourceStatisticsForPrimaryDomain(primaryDomain(document.url()));
    statistics.hadUserInteraction = true;
}
    
bool ResourceLoadObserver::isPrevalentResource(const String& primaryDomain) const
{
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;

    return mapEntry->value.isPrevalentResource;
}
    
ResourceLoadStatistics& ResourceLoadObserver::resourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    if (!m_resourceStatisticsMap.contains(primaryDomain))
        m_resourceStatisticsMap.set(primaryDomain, ResourceLoadStatistics());
    
    return m_resourceStatisticsMap.find(primaryDomain)->value;
}

String ResourceLoadObserver::primaryDomain(const URL& url)
{
    String host = url.host();
    Vector<String> hostSplitOnDot;
    
    host.split('.', false, hostSplitOnDot);

    String primaryDomain;
    if (host.isNull())
        primaryDomain = "nullOrigin";
    else if (hostSplitOnDot.size() < 3)
        primaryDomain = host;
    else {
        // Skip TLD and then up to two domains smaller than 4 characters
        int primaryDomainCutOffIndex = hostSplitOnDot.size() - 2;

        // Start with TLD as a given part
        size_t numberOfParts = 1;
        for (; primaryDomainCutOffIndex >= 0; --primaryDomainCutOffIndex) {
            ++numberOfParts;

            // We have either a domain part that's 4 chars or longer, or 3 domain parts including TLD
            if (hostSplitOnDot.at(primaryDomainCutOffIndex).length() >= 4 || numberOfParts >= 3)
                break;
        }

        if (primaryDomainCutOffIndex < 0)
            primaryDomain = host;
        else {
            StringBuilder builder;
            builder.append(hostSplitOnDot.at(primaryDomainCutOffIndex));
            for (size_t j = primaryDomainCutOffIndex + 1; j < hostSplitOnDot.size(); ++j) {
                builder.append('.');
                builder.append(hostSplitOnDot[j]);
            }
            primaryDomain = builder.toString();
        }
    }

    return primaryDomain;
}

typedef HashMap<String, ResourceLoadStatistics>::KeyValuePairType StatisticsValue;

void ResourceLoadObserver::writeDataToDisk()
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;
    
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("originsVisited", m_resourceStatisticsMap.size());
    
    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [this](KeyedEncoder& encoderInner, const StatisticsValue& origin) {
        origin.value.encode(encoderInner, origin.key);
    });
    
    writeEncoderToDisk(*encoder.get(), "full_browsing_session");
}

void ResourceLoadObserver::writeDataToDisk(const String& origin, const ResourceLoadStatistics& resourceStatistics) const
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("originsVisited", 1);
    
    encoder->encodeObject(origin, resourceStatistics, [this, &origin](KeyedEncoder& encoder, const ResourceLoadStatistics& resourceStatistics) {
        resourceStatistics.encode(encoder, origin);
    });
    
    String label = origin;
    label.replace('/', '_');
    label.replace(':', '+');
    writeEncoderToDisk(*encoder.get(), label);
}

void ResourceLoadObserver::setStatisticsStorageDirectory(const String& path)
{
    m_storagePath = path;
}

String ResourceLoadObserver::persistentStoragePath(const String& label) const
{
    if (m_storagePath.isEmpty())
        return emptyString();

    // TODO Decide what to call this file
    return pathByAppendingComponent(m_storagePath, label + "_resourceLog.plist");
}

void ResourceLoadObserver::readDataFromDiskIfNeeded()
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    if (m_resourceStatisticsMap.size())
        return;

    auto decoder = createDecoderFromDisk("full_browsing_session");
    if (!decoder)
        return;

    unsigned visitedOrigins = 0;
    decoder->decodeUInt32("originsVisited", visitedOrigins);
        
    Vector<String> loadedOrigins;
    decoder->decodeObjects("browsingStatistics", loadedOrigins, [this](KeyedDecoder& decoderInner, String& origin) {
        if (!decoderInner.decodeString("PrevalentResourceOrigin", origin))
            return false;

        ResourceLoadStatistics statistics;
        if (!statistics.decode(decoderInner, origin))
            return false;

        m_resourceStatisticsMap.set(origin, statistics);
            
        return true;
    });
        
    ASSERT(visitedOrigins == static_cast<size_t>(loadedOrigins.size()));
}

std::unique_ptr<KeyedDecoder> ResourceLoadObserver::createDecoderFromDisk(const String& label) const
{
    String resourceLog = persistentStoragePath(label);
    if (resourceLog.isEmpty())
        return nullptr;
    
    RefPtr<SharedBuffer> rawData = SharedBuffer::createWithContentsOfFile(resourceLog);
    if (!rawData)
        return nullptr;
    
    return KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(rawData->data()), rawData->size());
}
    
void ResourceLoadObserver::writeEncoderToDisk(KeyedEncoder& encoder, const String& label) const
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

String ResourceLoadObserver::statisticsForOrigin(const String& origin)
{
    if (!m_resourceStatisticsMap.contains(origin))
        return emptyString();
    
    auto& statistics = m_resourceStatisticsMap.find(origin)->value;
    return "Statistics for " + origin + ":\n" + statistics.toString();
}
    
}
