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

#include "config.h"

#include <WebCore/IPAddressSpace.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/MixedContentChecker.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(MixedContentChecker, CanModifyRequest)
{
    URL url { "http://example.com/cat.jpg"_s };
    FetchOptions::Destination destination = FetchOptions::Destination::Image;
    Initiator initiator = Initiator::EmptyString;

    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        url,
        destination,
        initiator
    ));

    // 4.1.1 request’s URL is a potentially trustworthy URL.
    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "https://example.com/cat.jpg"_s },
        destination,
        initiator
    ));

    // 4.1.2 request’s URL’s host is an IP address.
    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "http://192.0.1.36"_s },
        destination,
        initiator
    ));

    // Exception for 4.1.2 potentially truthworthy address
    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        URL { "http://127.0.0.1"_s },
        destination,
        initiator
    ));

    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        URL { "http://localhost"_s },
        destination,
        initiator
    ));

    // 4.1.4 request’s destination is not "image", "audio", or "video".
    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        url,
        FetchOptions::Destination::Audio,
        initiator
    ));

    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        url,
        FetchOptions::Destination::Video,
        initiator
    ));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        url,
        FetchOptions::Destination::Font,
        initiator
    ));

    // 4.1.5 request’s destination is "image" and request’s initiator is "imageset".
    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        url,
        destination,
        Initiator::Imageset
    ));

    // But if the scheme is handled by the handler, it is modifiable even if the initiator is "imageset".
    LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler("custom"_s);

    ASSERT_TRUE(LegacySchemeRegistry::schemeIsHandledBySchemeHandler("custom"_s));

    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        URL { "custom://example.com/cat.jpg"_s },
        destination,
        Initiator::Imageset
    ));

    // This exception won't apply if the cheme is not registered.
    ASSERT_FALSE(LegacySchemeRegistry::schemeIsHandledBySchemeHandler("custom2"_s));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "custom2://example.com/cat.jpg"_s },
        destination,
        Initiator::Imageset
    ));
}

TEST(MixedContentChecker, CanModifyRequestLocalNetworkAccessExemption)
{
    URL url { "http://example.com/cat.jpg"_s };
    FetchOptions::Destination destination = FetchOptions::Destination::Image;
    Initiator initiator = Initiator::EmptyString;

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        url,
        destination,
        initiator,
        IPAddressSpace::Local
    ));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        url,
        destination,
        initiator,
        IPAddressSpace::Loopback
    ));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "http://192.0.1.36"_s },
        destination,
        initiator,
        IPAddressSpace::Local
    ));

    // Explicit IPAddressSpace::Public should behave identically to omitting the parameter.
    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        url,
        destination,
        initiator,
        IPAddressSpace::Public
    ));

    // 4.1.2's IP-address rule carries an exception for potentially trustworthy hosts, so a loopback URL
    // is the one local address that is modifiable while its declared space still reads as public. Naming
    // the space it is really in is what makes Local Network Access exempt it, and this pair is the whole
    // behaviour change: same URL, opposite answer.
    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        URL { "http://127.0.0.1"_s },
        destination,
        initiator,
        IPAddressSpace::Public
    ));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "http://127.0.0.1"_s },
        destination,
        initiator,
        IPAddressSpace::Loopback
    ));

    // The case the exemption exists for. 4.1.2 only rejects IP *literals*, so a name that resolves into
    // the local network reaches the end of the checks and gets upgraded, unlike 192.0.1.36 above. A
    // ".local" name is Local per determineIPAddressSpace, so declaring it is the only thing that stops
    // the upgrade -- nothing else in mixed content would have.
    ASSERT_TRUE(MixedContentChecker::canModifyRequest(
        URL { "http://printer.local/cat.jpg"_s },
        destination,
        initiator,
        IPAddressSpace::Public
    ));

    ASSERT_FALSE(MixedContentChecker::canModifyRequest(
        URL { "http://printer.local/cat.jpg"_s },
        destination,
        initiator,
        IPAddressSpace::Local
    ));
}

} // namespace TestWebKitAPI
