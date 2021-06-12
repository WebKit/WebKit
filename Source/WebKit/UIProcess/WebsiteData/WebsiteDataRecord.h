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

#pragma once

#include "WebsiteDataType.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

struct WebsiteDataRecord {
    static String displayNameForCookieHostName(const String& hostName);
    static String displayNameForHostName(const String& hostName);

    static String displayNameForOrigin(const WebCore::SecurityOriginData&);

    void add(WebsiteDataType, const WebCore::SecurityOriginData&);
    void addCookieHostName(const String& hostName);
#if ENABLE(NETSCAPE_PLUGIN_API)
    void addPluginDataHostName(const String& hostName);
#endif
    void addHSTSCacheHostname(const String& hostName);
    void addAlternativeServicesHostname(const String& hostName);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void addResourceLoadStatisticsRegistrableDomain(const WebCore::RegistrableDomain&);
#endif

    bool matches(const WebCore::RegistrableDomain&) const;
    String topPrivatelyControlledDomain();

    WebsiteDataRecord isolatedCopy() const;

    String displayName;
    OptionSet<WebsiteDataType> types;

    struct Size {
        uint64_t totalSize;
        HashMap<unsigned, uint64_t> typeSizes;
    };
    std::optional<Size> size;

    HashSet<WebCore::SecurityOriginData> origins;
    HashSet<String> cookieHostNames;
#if ENABLE(NETSCAPE_PLUGIN_API)
    HashSet<String> pluginDataHostNames;
#endif
    HashSet<String> HSTSCacheHostNames;
    HashSet<String> alternativeServicesHostNames;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    HashSet<WebCore::RegistrableDomain> resourceLoadStatisticsRegistrableDomains;
#endif
};

}
