/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/RegistrableDomain.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(RegistrableDomain, StringVsURL)
{
    URL webkitURL { "https://webkit.org"_str };
    RegistrableDomain webkitDomainFromURL { webkitURL };
    auto webkitDomainFromString = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("webkit.org"_s);

    ASSERT_EQ(webkitDomainFromURL, webkitDomainFromString);

    URL localhostURL { "https://localhost:8000"_str };
    RegistrableDomain localhostDomainFromURL { localhostURL };
    auto localhostDomainFromString = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("localhost"_s);

    ASSERT_EQ(localhostDomainFromURL, localhostDomainFromString);

    URL fileURL { "file:///some/file"_str };
    RegistrableDomain fileDomainFromURL { fileURL };
    auto fileDomainFromString = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(emptyString());
    
    ASSERT_EQ(fileDomainFromURL, fileDomainFromString);
}

TEST(RegistrableDomain, MatchesURLs)
{
    URL webkitURL { "https://webkit.org"_str };
    URL webkitURLWithPath { "https://webkit.org/road/to/nowhere/"_str };
    URL webkitSubdomainURL { "https://sub.domain.webkit.org"_str };
    URL webkitOtherSubdomainURL { "https://sub.other.webkit.org"_str };
    URL webkitDuplicateSubdomainURL { "https://domain.domain.webkit.org"_str };
    URL webkitSubdomainURLWithPath { "https://sub.domain.webkit.org/road/to/nowhere/"_str };
    RegistrableDomain webkitDomain { webkitURL };
    RegistrableDomain webkitSubdomain { webkitSubdomainURL };

    ASSERT_TRUE(webkitDomain.matches(webkitURL));
    ASSERT_TRUE(webkitDomain.matches(webkitURLWithPath));
    ASSERT_TRUE(webkitDomain.matches(webkitSubdomainURL));
    ASSERT_TRUE(webkitDomain.matches(webkitDuplicateSubdomainURL));
    ASSERT_TRUE(webkitDomain.matches(webkitSubdomainURLWithPath));
    ASSERT_TRUE(webkitSubdomain.matches(webkitOtherSubdomainURL));
    ASSERT_TRUE(webkitSubdomain.matches(webkitDuplicateSubdomainURL));

    URL localhostURL { "http://localhost"_s };
    URL localhostURLWithPath { "http://localhost/road/to/nowhere/"_s };
    RegistrableDomain localhostDomain { localhostURL };

    ASSERT_TRUE(localhostDomain.matches(localhostURL));
    ASSERT_TRUE(localhostDomain.matches(localhostURLWithPath));

    ASSERT_FALSE(localhostDomain.matches(webkitURL));
    ASSERT_FALSE(localhostDomain.matches(webkitSubdomainURLWithPath));
    ASSERT_FALSE(webkitDomain.matches(localhostURL));
    ASSERT_FALSE(webkitDomain.matches(localhostURLWithPath));

    URL ebkitURL { "https://ebkit.org"_s };
    ASSERT_FALSE(webkitDomain.matches(ebkitURL));
}

TEST(RegistrableDomain, UncheckedCreateFromHost)
{
    auto webkitDomainFromString = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("webkit.org"_s);

    auto webkitDomainFromHost = RegistrableDomain::uncheckedCreateFromHost("webkit.org"_s);
    ASSERT_EQ(webkitDomainFromHost, webkitDomainFromString);
    // This test is important for matching cookies' domain attributes which often have a leading dot.
    auto dotWebkitDomainFromHost = RegistrableDomain::uncheckedCreateFromHost(".webkit.org"_s);
    ASSERT_EQ(dotWebkitDomainFromHost, webkitDomainFromString);
}

} // namespace TestWebKitAPI
