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
#include "WebsiteDataRecord.h"

#include <WebCore/LocalizedStrings.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/CrossThreadCopier.h>

#if PLATFORM(COCOA)
#import <pal/spi/cf/CFNetworkSPI.h>
#endif

static String displayNameForLocalFiles()
{
    return WEB_UI_STRING("Local documents on your computer", "'Website' name displayed when local documents have stored local data");
}

namespace WebKit {

String WebsiteDataRecord::displayNameForCookieHostName(const String& hostName)
{
#if PLATFORM(COCOA)
    if (hostName == String(kCFHTTPCookieLocalFileDomain))
        return displayNameForLocalFiles();
#else
    if (hostName == "localhost"_s)
        return hostName;
#endif
    return displayNameForHostName(hostName);
}

String WebsiteDataRecord::displayNameForHostName(const String& hostName)
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    return WebCore::topPrivatelyControlledDomain(hostName);
#endif

    return String();
}

String WebsiteDataRecord::displayNameForOrigin(const WebCore::SecurityOriginData& securityOrigin)
{
    const auto& protocol = securityOrigin.protocol;

    if (protocol == "file"_s)
        return displayNameForLocalFiles();

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (protocol == "http"_s || protocol == "https"_s)
        return WebCore::topPrivatelyControlledDomain(securityOrigin.host);
#endif

    return String();
}

void WebsiteDataRecord::add(WebsiteDataType type, const WebCore::SecurityOriginData& origin)
{
    types.add(type);
    origins.add(origin);
}

void WebsiteDataRecord::addCookieHostName(const String& hostName)
{
    types.add(WebsiteDataType::Cookies);
    cookieHostNames.add(hostName);
}

void WebsiteDataRecord::addHSTSCacheHostname(const String& hostName)
{
    types.add(WebsiteDataType::HSTSCache);
    HSTSCacheHostNames.add(hostName);
}

void WebsiteDataRecord::addAlternativeServicesHostname(const String& hostName)
{
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    types.add(WebsiteDataType::AlternativeServices);
    alternativeServicesHostNames.add(hostName);
#else
    UNUSED_PARAM(hostName);
#endif
}

#if ENABLE(TRACKING_PREVENTION)
void WebsiteDataRecord::addResourceLoadStatisticsRegistrableDomain(const WebCore::RegistrableDomain& domain)
{
    types.add(WebsiteDataType::ResourceLoadStatistics);
    resourceLoadStatisticsRegistrableDomains.add(domain);
}
#endif

static inline bool hostIsInDomain(StringView host, StringView domain)
{
    if (!host.endsWithIgnoringASCIICase(domain))
        return false;
    
    ASSERT(host.length() >= domain.length());
    unsigned suffixOffset = host.length() - domain.length();
    return !suffixOffset || host[suffixOffset - 1] == '.';
}

bool WebsiteDataRecord::matches(const WebCore::RegistrableDomain& domain) const
{
    if (domain.isEmpty())
        return false;

    if (types.contains(WebsiteDataType::Cookies)) {
        for (const auto& hostName : cookieHostNames) {
            if (hostIsInDomain(hostName, domain.string()))
                return true;
        }
    }

    for (const auto& dataRecordOriginData : origins) {
        if (hostIsInDomain(dataRecordOriginData.host, domain.string()))
            return true;
    }

    return false;
}

String WebsiteDataRecord::topPrivatelyControlledDomain()
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (!cookieHostNames.isEmpty())
        return WebCore::topPrivatelyControlledDomain(cookieHostNames.takeAny());
    
    if (!origins.isEmpty())
        return WebCore::topPrivatelyControlledDomain(origins.takeAny().securityOrigin().get().host());
#endif // ENABLE(PUBLIC_SUFFIX_LIST)
    
    return emptyString();
}

WebsiteDataRecord WebsiteDataRecord::isolatedCopy() const &
{
    return WebsiteDataRecord {
        crossThreadCopy(displayName),
        types,
        size,
        crossThreadCopy(origins),
        crossThreadCopy(cookieHostNames),
        crossThreadCopy(HSTSCacheHostNames),
        crossThreadCopy(alternativeServicesHostNames),
#if ENABLE(TRACKING_PREVENTION)
        crossThreadCopy(resourceLoadStatisticsRegistrableDomains),
#endif
    };
}

WebsiteDataRecord WebsiteDataRecord::isolatedCopy() &&
{
    return WebsiteDataRecord {
        crossThreadCopy(WTFMove(displayName)),
        types,
        size,
        crossThreadCopy(WTFMove(origins)),
        crossThreadCopy(WTFMove(cookieHostNames)),
        crossThreadCopy(WTFMove(HSTSCacheHostNames)),
        crossThreadCopy(WTFMove(alternativeServicesHostNames)),
#if ENABLE(TRACKING_PREVENTION)
        crossThreadCopy(WTFMove(resourceLoadStatisticsRegistrableDomains)),
#endif
    };
}

}
