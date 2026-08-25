/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "Helpers/Test.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/WellKnownOriginList.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

class WellKnownOriginList : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

static String wellKnownResourceFor(std::initializer_list<ASCIILiteral> origins)
{
    StringBuilder builder;
    builder.append("{\"origins\":["_s);
    bool first = true;
    for (auto origin : origins) {
        if (!first)
            builder.append(',');
        first = false;
        builder.append('"', origin, '"');
    }
    builder.append("]}"_s);
    return builder.toString();
}

static WebCore::WellKnownOriginListResult findIn(ASCIILiteral callerOrigin, const String& resource,
    const WebCore::WellKnownOriginListPolicy& policy = { })
{
    auto origin = WebCore::SecurityOriginData::fromURL(URL { callerOrigin });
    auto utf8 = resource.utf8();
    return WebCore::findOriginInWellKnownList(origin, byteCast<uint8_t>(utf8.span()), "origins"_s, policy);
}

static bool isRelated(ASCIILiteral callerOrigin, const String& resource)
{
    return findIn(callerOrigin, resource) == WebCore::WellKnownOriginListResult::Found;
}

static bool isMalformed(ASCIILiteral callerOrigin, const String& resource)
{
    return findIn(callerOrigin, resource) == WebCore::WellKnownOriginListResult::Malformed;
}

static bool isRelated(ASCIILiteral callerOrigin, std::initializer_list<ASCIILiteral> origins)
{
    return isRelated(callerOrigin, wellKnownResourceFor(origins));
}

TEST_F(WellKnownOriginList, WantedOriginBeforeLabelLimit)
{
    EXPECT_TRUE(isRelated("https://example.co.kr"_s, {
        "https://example1.com"_s,
        "https://example.co.kr"_s,
        "https://example2.com"_s,
        "https://example3.com"_s,
        "https://example4.com"_s,
        "https://example5.com"_s,
    }));
}

TEST_F(WellKnownOriginList, MoreOriginsThanLabelLimitSharingOneLabel)
{
    EXPECT_TRUE(isRelated("https://example.co.kr"_s, {
        "https://example.co.uk"_s,
        "https://example.de"_s,
        "https://example.co.jp"_s,
        "https://example.fr"_s,
        "https://example.ca"_s,
        "https://example.co.kr"_s,
    }));
}

TEST_F(WellKnownOriginList, WantedOriginAfterLabelLimit)
{
    EXPECT_FALSE(isRelated("https://example.co.kr"_s, {
        "https://example1.com"_s,
        "https://example2.com"_s,
        "https://example3.com"_s,
        "https://example4.com"_s,
        "https://example5.com"_s,
        "https://example.co.kr"_s,
    }));
}

TEST_F(WellKnownOriginList, UnparseableEntryDoesNotCountTowardLabelLimit)
{
    EXPECT_TRUE(isRelated("https://example.co.kr"_s, {
        "this-is-not-a-url"_s,
        "https://example1.com"_s,
        "https://example2.com"_s,
        "https://example3.com"_s,
        "https://example4.com"_s,
        "https://example.co.kr"_s,
    }));
}

TEST_F(WellKnownOriginList, RepeatedLabelDoesNotCountTowardLabelLimitTwice)
{
    EXPECT_TRUE(isRelated("https://target.com"_s, {
        "https://a.com"_s,
        "https://a.com"_s,
        "https://b.com"_s,
        "https://b.com"_s,
        "https://c.com"_s,
        "https://c.com"_s,
        "https://d.com"_s,
        "https://d.com"_s,
        "https://target.com"_s,
    }));
}

TEST_F(WellKnownOriginList, SeenLabelIsStillConsideredAtLabelLimit)
{
    EXPECT_TRUE(isRelated("https://sub.example.co.uk"_s, {
        "https://example.com"_s,
        "https://a2.com"_s,
        "https://a3.com"_s,
        "https://a4.com"_s,
        "https://a5.com"_s,
        "https://sub.example.co.uk"_s,
    }));
}

TEST_F(WellKnownOriginList, MultiPartPublicSuffixLabel)
{
    EXPECT_TRUE(isRelated("https://example.com.au"_s, {
        "https://example.com"_s,
        "https://a2.com"_s,
        "https://a3.com"_s,
        "https://a4.com"_s,
        "https://a5.com"_s,
        "https://example.com.au"_s,
    }));
}

static constexpr std::array specificationExample {
    "https://example.co.uk"_s,
    "https://example.de"_s,
    "https://example.sg"_s,
    "https://example.net"_s,
    "https://exampledelivery.com"_s,
    "https://exampledelivery.co.uk"_s,
    "https://exampledelivery.de"_s,
    "https://exampledelivery.sg"_s,
    "https://myexamplerewards.com"_s,
    "https://examplecars.com"_s,
};

static String specificationExampleResource()
{
    StringBuilder builder;
    builder.append("{\"origins\":["_s);
    bool first = true;
    for (auto origin : specificationExample) {
        if (!first)
            builder.append(',');
        first = false;
        builder.append('"', origin, '"');
    }
    builder.append("]}"_s);
    return builder.toString();
}

TEST_F(WellKnownOriginList, SpecificationExampleMatchesEveryListedOrigin)
{
    auto resource = specificationExampleResource();
    for (auto origin : specificationExample)
        EXPECT_TRUE(isRelated(origin, resource)) << origin.characters();
}

TEST_F(WellKnownOriginList, SpecificationExampleDoesNotMatchUnlistedOrigin)
{
    EXPECT_FALSE(isRelated("https://example.fr"_s, specificationExampleResource()));
}

TEST_F(WellKnownOriginList, SchemeMustMatch)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, { "http://example.com"_s }));
}

TEST_F(WellKnownOriginList, PortMustMatch)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, { "https://example.com:8443"_s }));
}

TEST_F(WellKnownOriginList, HostMustMatch)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, { "https://other.com"_s }));
}

TEST_F(WellKnownOriginList, SubdomainIsNotAMatch)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, { "https://www.example.com"_s }));
    EXPECT_FALSE(isRelated("https://www.example.com"_s, { "https://example.com"_s }));
}

TEST_F(WellKnownOriginList, PathAndQueryAreIgnored)
{
    EXPECT_TRUE(isRelated("https://example.com"_s, { "https://example.com/some/path?query=1"_s }));
}

TEST_F(WellKnownOriginList, EmptyResource)
{
    EXPECT_TRUE(isMalformed("https://example.com"_s, emptyString()));
}

TEST_F(WellKnownOriginList, ResourceIsNotJSON)
{
    EXPECT_TRUE(isMalformed("https://example.com"_s, "not json at all"_s));
}

TEST_F(WellKnownOriginList, ResourceIsNotAnObject)
{
    EXPECT_TRUE(isMalformed("https://example.com"_s, "[\"https://example.com\"]"_s));
    EXPECT_FALSE(isRelated("https://example.com"_s, "123"_s));
    EXPECT_FALSE(isRelated("https://example.com"_s, "null"_s));
}

TEST_F(WellKnownOriginList, ResourceIsMissingOrigins)
{
    EXPECT_TRUE(isMalformed("https://example.com"_s, "{\"other\":1}"_s));
}

TEST_F(WellKnownOriginList, OriginsIsNotAnArray)
{
    EXPECT_TRUE(isMalformed("https://example.com"_s, "{\"origins\":\"https://example.com\"}"_s));
    EXPECT_FALSE(isRelated("https://example.com"_s, "{\"origins\":{}}"_s));
}

TEST_F(WellKnownOriginList, OriginsIsEmpty)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, "{\"origins\":[]}"_s));
}

TEST_F(WellKnownOriginList, NonStringEntriesAreSkipped)
{
    EXPECT_FALSE(isRelated("https://example.com"_s, "{\"origins\":[123,null,{\"a\":1}]}"_s));
    EXPECT_TRUE(isRelated("https://example.com"_s, "{\"origins\":[123,\"https://example.com\"]}"_s));
}

static bool isValidRelyingPartyIdentifier(ASCIILiteral callerOrigin, const String& relyingPartyIdentifier)
{
    return WebCore::SecurityOriginData::fromURL(URL { callerOrigin }).securityOrigin()->isMatchingRegistrableDomainSuffix(relyingPartyIdentifier);
}

TEST_F(WellKnownOriginList, RelyingPartyIdentifierAcceptedForms)
{
    EXPECT_TRUE(isValidRelyingPartyIdentifier("https://example.com"_s, "example.com"_s));
    EXPECT_TRUE(isValidRelyingPartyIdentifier("https://sub.example.com"_s, "example.com"_s));
    EXPECT_TRUE(isValidRelyingPartyIdentifier("https://a.b.example.com"_s, "example.com"_s));
    EXPECT_TRUE(isValidRelyingPartyIdentifier("https://example.com"_s, "EXAMPLE.com"_s));
}

TEST_F(WellKnownOriginList, RelyingPartyIdentifierRejectedForms)
{
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://notexample.com"_s, "example.com"_s));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.com"_s, "ample.com"_s));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.com"_s, "sub.example.com"_s));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.com"_s, "other.com"_s));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.com"_s, emptyString()));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.com"_s, "com"_s));
    EXPECT_FALSE(isValidRelyingPartyIdentifier("https://example.co.uk"_s, "co.uk"_s));
}

TEST_F(WellKnownOriginList, WellKnownURLFromHost)
{
    auto url = WebCore::wellKnownURL("example.com"_s, "/.well-known/webauthn"_s);
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.string() == "https://example.com/.well-known/webauthn"_s);
}

TEST_F(WellKnownOriginList, WellKnownURLIsCaseInsensitiveInTheHost)
{
    auto url = WebCore::wellKnownURL("EXAMPLE.com"_s, "/.well-known/webauthn"_s);
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.host() == "example.com"_s);
}

TEST_F(WellKnownOriginList, WellKnownURLRejectsAnythingThatIsNotJustAHost)
{
    static constexpr std::array notJustAHost {
        "example.com:8443"_s, "example.com/path"_s, "user@example.com"_s,
        "user:pass@example.com"_s, "https://example.com"_s, "example.com?q=1"_s,
        "example.com#f"_s, "exa mple.com"_s, ""_s,
    };
    for (auto host : notJustAHost)
        EXPECT_FALSE(WebCore::wellKnownURL(host, "/.well-known/webauthn"_s).isValid()) << host.characters();
}

TEST_F(WellKnownOriginList, WellKnownURLAcceptsUnusualButValidHosts)
{
    EXPECT_TRUE(WebCore::wellKnownURL("localhost"_s, "/.well-known/webauthn"_s).isValid());
    EXPECT_TRUE(WebCore::wellKnownURL("."_s, "/.well-known/webauthn"_s).isValid());
}

TEST_F(WellKnownOriginList, WellKnownURLRequiresAnAbsolutePath)
{
    EXPECT_FALSE(WebCore::wellKnownURL("example.com"_s, "relative"_s).isValid());
}

TEST_F(WellKnownOriginList, ResponseMustBe200AndJSON)
{
    EXPECT_TRUE(WebCore::isWellKnownResponseAcceptable(200, "application/json"_s));
    EXPECT_TRUE(WebCore::isWellKnownResponseAcceptable(200, "APPLICATION/JSON"_s));

    EXPECT_FALSE(WebCore::isWellKnownResponseAcceptable(200, "text/plain"_s));
    EXPECT_FALSE(WebCore::isWellKnownResponseAcceptable(200, "text/html"_s));
    EXPECT_FALSE(WebCore::isWellKnownResponseAcceptable(200, emptyString()));
    for (int status : { 0, 204, 301, 302, 304, 400, 403, 404, 500, 503 })
        EXPECT_FALSE(WebCore::isWellKnownResponseAcceptable(status, "application/json"_s)) << status;
}

TEST_F(WellKnownOriginList, RedirectsMustStayHTTPS)
{
    EXPECT_TRUE(WebCore::isWellKnownRedirectAllowed(URL { "https://example.com/x"_s }));

    static constexpr std::array notHTTPS {
        "http://example.com/x"_s, "ftp://example.com/x"_s,
        "data:text/plain,x"_s, "about:blank"_s, "not a url"_s, ""_s,
    };
    for (auto candidate : notHTTPS)
        EXPECT_FALSE(WebCore::isWellKnownRedirectAllowed(URL { candidate })) << candidate.characters();
}

TEST_F(WellKnownOriginList, CandidateLimitBoundsTheWorkDone)
{
    StringBuilder builder;
    builder.append("{\"origins\":["_s);
    for (unsigned i = 0; i < 500; ++i)
        builder.append(i ? ",\"https://filler"_s : "\"https://filler"_s, i, ".com\""_s);
    builder.append(",\"https://example.com\"]}"_s);
    auto resource = builder.toString();

    EXPECT_FALSE(isRelated("https://example.com"_s, resource));

    WebCore::WellKnownOriginListPolicy generous;
    generous.maxCandidates = 1000;
    generous.maxRegistrableOriginLabels = 1000;
    EXPECT_EQ(findIn("https://example.com"_s, resource, generous), WebCore::WellKnownOriginListResult::Found);
}

TEST_F(WellKnownOriginList, ResourceLargerThanTheLimitIsRejected)
{
    StringBuilder builder;
    builder.append("{\"origins\":[\"https://example.com\","_s);
    while (builder.length() < 4096)
        builder.append("\"https://padding.example.org\","_s);
    builder.append("\"https://tail.example.org\"]}"_s);

    WebCore::WellKnownOriginListPolicy tiny;
    tiny.maxResourceSize = 1024;
    EXPECT_EQ(findIn("https://example.com"_s, builder.toString(), tiny), WebCore::WellKnownOriginListResult::Malformed);
}

TEST_F(WellKnownOriginList, LabelLimitIsConfigurable)
{
    auto resource = wellKnownResourceFor({
        "https://a1.com"_s, "https://a2.com"_s, "https://example.co.kr"_s,
    });
    WebCore::WellKnownOriginListPolicy strict;
    strict.maxRegistrableOriginLabels = 2;
    EXPECT_EQ(findIn("https://example.co.kr"_s, resource, strict), WebCore::WellKnownOriginListResult::NotFound);

    strict.maxRegistrableOriginLabels = 3;
    EXPECT_EQ(findIn("https://example.co.kr"_s, resource, strict), WebCore::WellKnownOriginListResult::Found);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
