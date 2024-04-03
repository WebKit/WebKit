/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCookieJar.h"

#if PLATFORM(COCOA)

#import <WebCore/Document.h>
#import <WebCore/Quirks.h>
#import <WebCore/SameSiteInfo.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/cf/StringConcatenateCF.h>

namespace WebKit {

static RetainPtr<NSDictionary> policyProperties(const WebCore::SameSiteInfo& sameSiteInfo, const URL& url)
{
    static NSURL *emptyURL = [[NSURL alloc] initWithString:@""];
    NSURL *nsURL = url;
    NSDictionary *policyProperties = @{
        @"_kCFHTTPCookiePolicyPropertySiteForCookies": sameSiteInfo.isSameSite ? nsURL : emptyURL,
        @"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation": [NSNumber numberWithBool:sameSiteInfo.isTopSite],
    };
    return policyProperties;
}

String WebCookieJar::cookiesInPartitionedCookieStorage(const WebCore::Document& document, const URL& cookieURL, const WebCore::SameSiteInfo& sameSiteInfo) const
{
    if (!document.quirks().shouldUseEphemeralPartitionedStorageForDOMCookies(cookieURL))
        return { };

    if (!m_partitionedStorageForDOMCookies)
        return { };

    URL firstPartyURL = document.firstPartyForCookies();
    String partition = WebCore::RegistrableDomain(firstPartyURL).string();
    if (partition.isEmpty())
        return { };

    __block RetainPtr<NSArray> cookies;
    [m_partitionedStorageForDOMCookies.get() _getCookiesForURL:cookieURL mainDocumentURL:firstPartyURL partition:partition policyProperties:policyProperties(sameSiteInfo, cookieURL).get() completionHandler:^(NSArray *result) {
        cookies = result;
    }];

    if (!cookies || ![cookies.get() count])
        return { };

    StringBuilder cookiesBuilder;
    for (NSHTTPCookie *cookie in cookies.get()) {
        if (![[cookie name] length] || [cookie isHTTPOnly])
            continue;

        cookiesBuilder.append(cookiesBuilder.isEmpty() ? "" : "; ", [cookie name], "=", [cookie value]);
    }

    return cookiesBuilder.toString();
}

void WebCookieJar::setCookiesInPartitionedCookieStorage(const WebCore::Document& document, const URL& cookieURL, const WebCore::SameSiteInfo& sameSiteInfo, const String& cookieString)
{
    if (!document.quirks().shouldUseEphemeralPartitionedStorageForDOMCookies(cookieURL))
        return;

    if (cookieString.isEmpty())
        return;

    auto firstPartyURL = document.firstPartyForCookies();
    String partition = WebCore::RegistrableDomain(firstPartyURL).string();
    if (partition.isEmpty())
        return;

    NSHTTPCookie *cookie = [NSHTTPCookie _cookieForSetCookieString:cookieString forURL:cookieURL partition:partition];
    if (!cookie || ![[cookie name] length] || [cookie isHTTPOnly])
        return;

    [ensurePartitionedCookieStorage() _setCookies:@[cookie] forURL:cookieURL mainDocumentURL:firstPartyURL policyProperties:policyProperties(sameSiteInfo, cookieURL).get()];
}

NSHTTPCookieStorage* WebCookieJar::ensurePartitionedCookieStorage()
{
    if (!m_partitionedStorageForDOMCookies) {
        m_partitionedStorageForDOMCookies = adoptNS([[NSHTTPCookieStorage alloc] _initWithIdentifier:@"WebCookieJar" private:true]);
        m_partitionedStorageForDOMCookies.get().cookieAcceptPolicy = NSHTTPCookieAcceptPolicyAlways;
    }

    return m_partitionedStorageForDOMCookies.get();
}

} // namespace WebKit
#endif
