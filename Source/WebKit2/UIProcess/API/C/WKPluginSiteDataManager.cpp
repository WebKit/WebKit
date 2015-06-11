/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "WKPluginSiteDataManager.h"

#include "APIObject.h"
#include "WKAPICast.h"
#include "WebProcessPool.h"
#include "WebsiteDataRecord.h"

using namespace WebKit;

WKTypeID WKPluginSiteDataManagerGetTypeID()
{
    return toAPI(API::WebsiteDataStore::APIType);
}

void WKPluginSiteDataManagerGetSitesWithData(WKPluginSiteDataManagerRef manager, void* context, WKPluginSiteDataManagerGetSitesWithDataFunction callback)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(manager))->websiteDataStore();
    websiteDataStore.fetchData(WebsiteDataTypes::WebsiteDataTypePlugInData, [context, callback](Vector<WebsiteDataRecord> dataRecords) {
        Vector<String> hostNames;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& hostName : dataRecord.pluginDataHostNames)
                hostNames.append(hostName);
        }

        callback(toAPI(API::Array::createStringArray(hostNames).ptr()), nullptr, context);
    });
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKPluginSiteDataManagerClearSiteData(WKPluginSiteDataManagerRef manager, WKArrayRef sites, WKClearSiteDataFlags flags, uint64_t maxAgeInSeconds, void* context, WKPluginSiteDataManagerClearSiteDataFunction callback)
{
    // These are the only parameters supported.
    ASSERT_UNUSED(flags, flags == kWKClearSiteDataFlagsClearAll);
    ASSERT_UNUSED(maxAgeInSeconds, maxAgeInSeconds == std::numeric_limits<uint64_t>::max());

#if ENABLE(NETSCAPE_PLUGIN_API)
    WebsiteDataRecord dataRecord;
    for (const auto& string : toImpl(sites)->elementsOfType<API::String>())
        dataRecord.pluginDataHostNames.add(string->string());

    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(manager))->websiteDataStore();
    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypePlugInData, { dataRecord }, [context, callback] {
        callback(nullptr, context);
    });
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(sites);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(maxAgeInSeconds);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKPluginSiteDataManagerClearAllSiteData(WKPluginSiteDataManagerRef manager, void* context, WKPluginSiteDataManagerClearSiteDataFunction callback)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(manager))->websiteDataStore();
    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypePlugInData, std::chrono::system_clock::time_point::min(), [context, callback] {
        callback(nullptr, context);
    });
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}
