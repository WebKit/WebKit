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
    URL webkitURL { URL(), "https://webkit.org" };
    RegistrableDomain webkitDomainFromURL { webkitURL };
    RegistrableDomain webkitDomainFromString { "webkit.org" };

    ASSERT_EQ(webkitDomainFromURL, webkitDomainFromString);

    URL localhostURL { URL(), "https://localhost:8000" };
    RegistrableDomain localhostDomainFromURL { localhostURL };
    RegistrableDomain localhostDomainFromString { "localhost" };
    
    ASSERT_EQ(localhostDomainFromURL, localhostDomainFromString);

    URL fileURL { URL(), "file:///some/file" };
    RegistrableDomain fileDomainFromURL { fileURL };
    RegistrableDomain fileDomainFromString { "" };
    
    ASSERT_EQ(fileDomainFromURL, fileDomainFromString);
}

TEST(RegistrableDomain, MatchesURLs)
{
    URL webkitURL { URL(), "https://webkit.org" };
    URL webkitURLWithPath { URL(), "https://webkit.org/road/to/nowhere/" };
    URL webkitSubdomainURL { URL(), "https://sub.domain.webkit.org" };
    URL webkitSubdomainURLWithPath { URL(), "https://sub.domain.webkit.org/road/to/nowhere/" };
    RegistrableDomain webkitDomain { webkitURL };

    ASSERT_TRUE(webkitDomain.matches(webkitURL));
    ASSERT_TRUE(webkitDomain.matches(webkitURLWithPath));
    ASSERT_TRUE(webkitDomain.matches(webkitSubdomainURL));
    ASSERT_TRUE(webkitDomain.matches(webkitSubdomainURLWithPath));

    URL localhostURL { URL(), "http://localhost" };
    URL localhostURLWithPath { URL(), "http://localhost/road/to/nowhere/" };
    RegistrableDomain localhostDomain { localhostURL };

    ASSERT_TRUE(localhostDomain.matches(localhostURL));
    ASSERT_TRUE(localhostDomain.matches(localhostURLWithPath));

    ASSERT_FALSE(localhostDomain.matches(webkitURL));
    ASSERT_FALSE(localhostDomain.matches(webkitSubdomainURLWithPath));
    ASSERT_FALSE(webkitDomain.matches(localhostURL));
    ASSERT_FALSE(webkitDomain.matches(localhostURLWithPath));

    URL ebkitURL { URL(), "https://ebkit.org" };
    ASSERT_FALSE(webkitDomain.matches(ebkitURL));
}

} // namespace TestWebKitAPI
