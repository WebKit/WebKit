/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebsiteData.h"

#include "ArgumentCoders.h"
#include "WebsiteDataType.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

void WebsiteData::Entry::encode(IPC::Encoder& encoder) const
{
    encoder << origin;
    encoder << type;
    encoder << size;
}

auto WebsiteData::Entry::decode(IPC::Decoder& decoder) -> std::optional<Entry>
{
    Entry result;

    std::optional<WebCore::SecurityOriginData> securityOriginData;
    decoder >> securityOriginData;
    if (!securityOriginData)
        return std::nullopt;
    result.origin = WTFMove(*securityOriginData);

    if (!decoder.decode(result.type))
        return std::nullopt;

    if (!decoder.decode(result.size))
        return std::nullopt;

    return result;
}

void WebsiteData::encode(IPC::Encoder& encoder) const
{
    encoder << entries;
    encoder << hostNamesWithCookies;
    encoder << hostNamesWithHSTSCache;
#if ENABLE(TRACKING_PREVENTION)
    encoder << registrableDomainsWithResourceLoadStatistics;
#endif
}

bool WebsiteData::decode(IPC::Decoder& decoder, WebsiteData& result)
{
    if (!decoder.decode(result.entries))
        return false;
    if (!decoder.decode(result.hostNamesWithCookies))
        return false;
    if (!decoder.decode(result.hostNamesWithHSTSCache))
        return false;
#if ENABLE(TRACKING_PREVENTION)
    if (!decoder.decode(result.registrableDomainsWithResourceLoadStatistics))
        return false;
#endif
    return true;
}

WebsiteDataProcessType WebsiteData::ownerProcess(WebsiteDataType dataType)
{
    switch (dataType) {
    case WebsiteDataType::Cookies:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::DiskCache:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::MemoryCache:
        return WebsiteDataProcessType::Web;
    case WebsiteDataType::OfflineWebApplicationCache:
        return WebsiteDataProcessType::UI;
    case WebsiteDataType::SessionStorage:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::LocalStorage:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::WebSQLDatabases:
        return WebsiteDataProcessType::UI;
    case WebsiteDataType::IndexedDBDatabases:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::MediaKeys:
        return WebsiteDataProcessType::UI;
    case WebsiteDataType::HSTSCache:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::SearchFieldRecentSearches:
        return WebsiteDataProcessType::UI;
    case WebsiteDataType::ResourceLoadStatistics:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::Credentials:
        return WebsiteDataProcessType::Network;
#if ENABLE(SERVICE_WORKER)
    case WebsiteDataType::ServiceWorkerRegistrations:
        return WebsiteDataProcessType::Network;
#endif
    case WebsiteDataType::DOMCache:
        return WebsiteDataProcessType::Network;
    case WebsiteDataType::DeviceIdHashSalt:
        return WebsiteDataProcessType::UI;
    case WebsiteDataType::PrivateClickMeasurements:
        return WebsiteDataProcessType::Network;
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    case WebsiteDataType::AlternativeServices:
        return WebsiteDataProcessType::Network;
#endif
    case WebsiteDataType::FileSystem:
        return WebsiteDataProcessType::Network;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

OptionSet<WebsiteDataType> WebsiteData::filter(OptionSet<WebsiteDataType> unfilteredWebsiteDataTypes, WebsiteDataProcessType WebsiteDataProcessType)
{
    OptionSet<WebsiteDataType> filtered;
    for (auto dataType : unfilteredWebsiteDataTypes) {
        if (ownerProcess(dataType) == WebsiteDataProcessType)
            filtered.add(dataType);
    }
    
    return filtered;
}

WebsiteData WebsiteData::isolatedCopy() const &
{
    return WebsiteData {
        crossThreadCopy(entries),
        crossThreadCopy(hostNamesWithCookies),
        crossThreadCopy(hostNamesWithHSTSCache),
#if ENABLE(TRACKING_PREVENTION)
        crossThreadCopy(registrableDomainsWithResourceLoadStatistics),
#endif
    };
}

WebsiteData WebsiteData::isolatedCopy() &&
{
    return WebsiteData {
        crossThreadCopy(WTFMove(entries)),
        crossThreadCopy(WTFMove(hostNamesWithCookies)),
        crossThreadCopy(WTFMove(hostNamesWithHSTSCache)),
#if ENABLE(TRACKING_PREVENTION)
        crossThreadCopy(WTFMove(registrableDomainsWithResourceLoadStatistics)),
#endif
    };
}

auto WebsiteData::Entry::isolatedCopy() const & -> Entry
{
    return { crossThreadCopy(origin), type, size };
}

auto WebsiteData::Entry::isolatedCopy() && -> Entry
{
    return { crossThreadCopy(WTFMove(origin)), type, size };
}

}
