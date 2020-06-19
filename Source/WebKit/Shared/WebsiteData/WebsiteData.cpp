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
#include <wtf/text/StringHash.h>

namespace WebKit {

void WebsiteData::Entry::encode(IPC::Encoder& encoder) const
{
    encoder << origin;
    encoder << type;
    encoder << size;
}

auto WebsiteData::Entry::decode(IPC::Decoder& decoder) -> Optional<Entry>
{
    Entry result;

    Optional<WebCore::SecurityOriginData> securityOriginData;
    decoder >> securityOriginData;
    if (!securityOriginData)
        return WTF::nullopt;
    result.origin = WTFMove(*securityOriginData);

    if (!decoder.decode(result.type))
        return WTF::nullopt;

    if (!decoder.decode(result.size))
        return WTF::nullopt;

    return result;
}

void WebsiteData::encode(IPC::Encoder& encoder) const
{
    encoder << entries;
    encoder << hostNamesWithCookies;
#if ENABLE(NETSCAPE_PLUGIN_API)
    encoder << hostNamesWithPluginData;
#endif
    encoder << hostNamesWithHSTSCache;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    encoder << registrableDomainsWithResourceLoadStatistics;
#endif
}

bool WebsiteData::decode(IPC::Decoder& decoder, WebsiteData& result)
{
    if (!decoder.decode(result.entries))
        return false;
    if (!decoder.decode(result.hostNamesWithCookies))
        return false;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (!decoder.decode(result.hostNamesWithPluginData))
        return false;
#endif
    if (!decoder.decode(result.hostNamesWithHSTSCache))
        return false;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
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
#if ENABLE(NETSCAPE_PLUGIN_API)
    case WebsiteDataType::PlugInData:
        return WebsiteDataProcessType::UI;
#endif
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
    case WebsiteDataType::AdClickAttributions:
        return WebsiteDataProcessType::Network;
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    case WebsiteDataType::AlternativeServices:
        return WebsiteDataProcessType::Network;
#endif
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

}
