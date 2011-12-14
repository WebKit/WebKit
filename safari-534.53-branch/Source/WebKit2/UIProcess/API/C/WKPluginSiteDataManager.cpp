/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "WKAPICast.h"
#include "WebPluginSiteDataManager.h"
#include <WebCore/npapi.h>

using namespace WebKit;
using namespace std;

WKTypeID WKPluginSiteDataManagerGetTypeID()
{
    return toAPI(WebPluginSiteDataManager::APIType);
}

void WKPluginSiteDataManagerGetSitesWithData(WKPluginSiteDataManagerRef managerRef, void* context, WKPluginSiteDataManagerGetSitesWithDataFunction callback)
{
    toImpl(managerRef)->getSitesWithData(ArrayCallback::create(context, callback));
}

static uint64_t toNPClearSiteDataFlags(WKClearSiteDataFlags flags)
{
    if (flags == kWKClearSiteDataFlagsClearAll)
        return NP_CLEAR_ALL;

    uint64_t result = 0;
    if (flags & kWKClearSiteDataFlagsClearCache)
        result |= NP_CLEAR_CACHE;
    return result;
}

void WKPluginSiteDataManagerClearSiteData(WKPluginSiteDataManagerRef managerRef, WKArrayRef sitesRef, WKClearSiteDataFlags flags, uint64_t maxAgeInSeconds, void* context, WKPluginSiteDataManagerClearSiteDataFunction function)
{
    toImpl(managerRef)->clearSiteData(toImpl(sitesRef), toNPClearSiteDataFlags(flags), maxAgeInSeconds, VoidCallback::create(context, function));
}

void WKPluginSiteDataManagerClearAllSiteData(WKPluginSiteDataManagerRef managerRef, void* context, WKPluginSiteDataManagerClearSiteDataFunction function)
{
    toImpl(managerRef)->clearSiteData(0, NP_CLEAR_ALL, numeric_limits<uint64_t>::max(), VoidCallback::create(context, function));
}
