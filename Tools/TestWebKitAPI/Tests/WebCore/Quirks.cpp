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

#include <WebCore/QuirkTable.h>
#include <WebCore/Quirks.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <array>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

class QuirksTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

static std::optional<String> customUserAgentFor(ASCIILiteral urlString)
{
    return WebCore::Quirks::needsCustomUserAgentOverride(URL { urlString }, "TestApp"_s, "TestBase/1.0 (KHTML, like Gecko) Trailer/1.0"_s);
}

static WebCore::QuirksData resolveQuirksForTopURL(ASCIILiteral urlString)
{
    return WebCore::resolveTopURLQuirks(URL { urlString });
}

static bool matchesTopURL(const WebCore::QuirkURLMatch& match, ASCIILiteral urlString)
{
    return match.matches(WebCore::URLMatchContext { URL { urlString } }, WebCore::URLMatchContext { URL { urlString } }, WebCore::IsTopDocument::Yes);
}

static bool matchesEmbeddedDocument(const WebCore::QuirkURLMatch& match, ASCIILiteral topURLString, ASCIILiteral documentURLString)
{
    return match.matches(WebCore::URLMatchContext { URL { topURLString } }, WebCore::URLMatchContext { URL { documentURLString } }, WebCore::IsTopDocument::No);
}

static constexpr std::array youTubeEmbedDomains { "youtube.com"_s, "youtube-nocookie.com"_s };

TEST_F(QuirksTest, TopURLMatchIgnoresTheDocumentURL)
{
    WebCore::QuirkURLMatch match = WebCore::URLMatch::domain("theguardian.com"_s);

    EXPECT_TRUE(matchesTopURL(match, "https://www.theguardian.com/film"_s));

    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.theguardian.com/film"_s, "https://www.youtube.com/embed/abc"_s));

    EXPECT_FALSE(matchesEmbeddedDocument(match, "https://www.youtube.com/"_s, "https://www.theguardian.com/film"_s));
}

TEST_F(QuirksTest, EmbeddedDocumentMatchesTheDocumentURLNotTheTopURL)
{
    auto match = WebCore::QuirkURLMatch::embeddedDocument(WebCore::URLMatch::domain(youTubeEmbedDomains));

    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.theguardian.com/film"_s, "https://www.youtube.com/embed/abc"_s));
    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.example.com/"_s, "https://www.youtube-nocookie.com/embed/abc"_s));
    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.example.com/"_s, "https://foo.bar.youtube.com/embed/abc"_s));

    EXPECT_FALSE(matchesEmbeddedDocument(match, "https://www.example.com/"_s, "https://vimeo.com/12345"_s));

    EXPECT_FALSE(matchesEmbeddedDocument(match, "https://www.youtube.com/watch?v=abc"_s, "https://vimeo.com/12345"_s));

    EXPECT_FALSE(matchesTopURL(match, "https://www.youtube.com/watch?v=abc"_s));
}

TEST_F(QuirksTest, EmbeddedDocumentInTopMatchRequiresBothURLsToMatch)
{
    auto match = WebCore::QuirkURLMatch::embeddedDocumentInTopMatch(WebCore::URLMatch::anyTopLevelDomain("theguardian"_s), WebCore::URLMatch::domain(youTubeEmbedDomains));

    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.theguardian.com/film"_s, "https://www.youtube.com/embed/abc"_s));
    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.theguardian.co.uk/film"_s, "https://www.youtube-nocookie.com/embed/abc"_s));

    EXPECT_FALSE(matchesEmbeddedDocument(match, "https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s));
    EXPECT_FALSE(matchesEmbeddedDocument(match, "https://www.theguardian.com/film"_s, "https://vimeo.com/12345"_s));
    EXPECT_FALSE(matchesTopURL(match, "https://www.theguardian.com/film"_s));
}

TEST_F(QuirksTest, EmbeddedMatchesNeverApplyToTheTopDocument)
{
    auto match = WebCore::QuirkURLMatch::embeddedDocumentInTopMatch(WebCore::URLMatch::anyURL(), WebCore::URLMatch::domain("youtube.com"_s));

    EXPECT_TRUE(matchesEmbeddedDocument(match, "https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s));
    EXPECT_FALSE(matchesTopURL(match, "https://www.youtube.com/watch?v=abc"_s));
}

#if PLATFORM(COCOA)
static WebCore::QuirksData resolveQuirksForEmbeddedDocument(ASCIILiteral topURLString, ASCIILiteral documentURLString)
{
    return WebCore::resolveSiteSpecificQuirks(URL { topURLString }, URL { documentURLString }, WebCore::IsTopDocument::No);
}

TEST_F(QuirksTest, EmbeddedQuirksResolveFromTheDocumentURL)
{
    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s).quirkIsEnabled(WebCore::Behaviors::needsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://vimeo.com/12345"_s).quirkIsEnabled(WebCore::Behaviors::needsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).quirkIsEnabled(WebCore::Behaviors::needsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.youtube-nocookie.com/embed/abc"_s).quirkIsEnabled(WebCore::Behaviors::needsYouTubeCaptionQuirk));
}
#endif

TEST_F(QuirksTest, SiteSpecificQuirksResolveWithoutADocument)
{
    EXPECT_TRUE(resolveQuirksForTopURL("https://www.airindiaexpress.com/"_s).quirkIsEnabled(WebCore::Behaviors::needsAirIndiaExpressLayeringQuirk));
    EXPECT_TRUE(resolveQuirksForTopURL("https://www.scribd.com/"_s).quirkIsEnabled(WebCore::Behaviors::needsReuseLiveRangeForSelectionUpdateQuirk));

    EXPECT_TRUE(resolveQuirksForTopURL("https://www.iheart.com/"_s).isSite(WebCore::QuirkSite::IHeart));

    auto unrelatedSiteQuirks = resolveQuirksForTopURL("https://www.example.com/"_s);
    EXPECT_TRUE(unrelatedSiteQuirks.activeQuirks.isEmpty());
    EXPECT_FALSE(unrelatedSiteQuirks.isSite(WebCore::QuirkSite::IHeart));
}

TEST_F(QuirksTest, NeedsIPadMiniUserAgent)
{
    EXPECT_TRUE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://roblox.com/"_s }));
    EXPECT_TRUE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://www.roblox.com/games"_s }));
    EXPECT_TRUE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://web.roblox.com/home"_s }));

    EXPECT_FALSE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://notroblox.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://roblox.com.example.com/"_s }));

    EXPECT_TRUE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://www.indiatimes.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://indiatimes.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://timesofindia.indiatimes.com/"_s }));

    EXPECT_FALSE(WebCore::Quirks::needsIPadMiniUserAgent(URL { "https://www.example.com/"_s }));
}

TEST_F(QuirksTest, NeedsIPhoneUserAgent)
{
    auto shopeeLandingURL = URL { "https://shopee.sg/payment/account-linking/landing"_s };

#if PLATFORM(IOS_FAMILY)
    EXPECT_TRUE(WebCore::Quirks::needsIPhoneUserAgent(shopeeLandingURL));

    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://shopee.sg/"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://shopee.sg/payment/account-linking/landing/"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://shopee.sg/payment/account-linking/landing/step2"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://shopee.sg/payment/account-linking"_s }));

    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://www.shopee.sg/payment/account-linking/landing"_s }));
    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://shopee.com/payment/account-linking/landing"_s }));
#else
    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(shopeeLandingURL));
#endif

    EXPECT_FALSE(WebCore::Quirks::needsIPhoneUserAgent(URL { "https://www.example.com/"_s }));
}

TEST_F(QuirksTest, ShouldTranscodeHeicImagesForURL)
{
    EXPECT_TRUE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://www.zillow.com/"_s }));
    EXPECT_TRUE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://zillow.com/homes"_s }));
    EXPECT_TRUE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://www.canva.com/design"_s }));
    EXPECT_TRUE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://member.uhc.com/"_s }));

    EXPECT_FALSE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://www.example.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::shouldTranscodeHeicImagesForURL(URL { "https://zillow.com.example.com/"_s }));
}

TEST_F(QuirksTest, IsMicrosoftTeamsRedirectURL)
{
    auto isRedirect = [](ASCIILiteral urlString) {
        return resolveQuirksForTopURL(urlString).quirkIsEnabled(WebCore::Behaviors::isMicrosoftTeamsRedirectURLQuirk);
    };

    EXPECT_TRUE(isRedirect("https://teams.microsoft.com/?error=Retried+3+times+without+success"_s));

    EXPECT_FALSE(isRedirect("https://teams.microsoft.com/"_s));
    EXPECT_FALSE(isRedirect("https://teams.live.com/?error=Retried+3+times+without+success"_s));
    EXPECT_FALSE(isRedirect("https://www.example.com/?error=Retried+3+times+without+success"_s));
}

TEST_F(QuirksTest, ShouldAllowNavigationToCustomProtocolWithoutUserGesture)
{
    auto allows = [](ASCIILiteral protocol, ASCIILiteral originURL) {
        return WebCore::Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(protocol, WebCore::SecurityOriginData::fromURL(URL { originURL }));
    };

    EXPECT_TRUE(allows("msteams"_s, "https://teams.live.com/"_s));
    EXPECT_TRUE(allows("msteams"_s, "https://teams.microsoft.com/v2/"_s));

    EXPECT_FALSE(allows("msteams"_s, "https://www.example.com/"_s));
    EXPECT_FALSE(allows("msteams"_s, "https://microsoft.com/"_s));
    EXPECT_FALSE(allows("mailto"_s, "https://teams.microsoft.com/"_s));
}

TEST_F(QuirksTest, NeedsPartitionedCookies)
{
    auto needsPartitionedCookies = [](ASCIILiteral urlString, bool isTopSite) {
        WebCore::ResourceRequest request { URL { urlString } };
        request.setIsTopSite(isTopSite);
        return WebCore::Quirks::needsPartitionedCookies(request);
    };

    EXPECT_TRUE(needsPartitionedCookies("https://biller.billpaysite.com/pay"_s, false));

    EXPECT_FALSE(needsPartitionedCookies("https://biller.billpaysite.com/pay"_s, true));
    EXPECT_FALSE(needsPartitionedCookies("https://notbillpaysite.com/"_s, false));
    EXPECT_FALSE(needsPartitionedCookies("https://billpaysite.com.example.com/"_s, false));
    EXPECT_FALSE(needsPartitionedCookies("https://www.example.com/"_s, false));
}

#if ENABLE(TOUCH_EVENTS)
TEST_F(QuirksTest, ShouldOmitTouchEventDOMAttributesForDesktopWebsite)
{
    EXPECT_TRUE(WebCore::Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(URL { "https://secure.chase.com/web/auth/dashboard"_s }));

    EXPECT_FALSE(WebCore::Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(URL { "https://www.chase.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(URL { "https://chase.com/"_s }));
    EXPECT_FALSE(WebCore::Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(URL { "https://www.example.com/"_s }));
}
#endif // ENABLE(TOUCH_EVENTS)

TEST_F(QuirksTest, NeedsCustomUserAgentOverrideNotAffected)
{
    EXPECT_FALSE(customUserAgentFor("https://www.example.com/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://webkit.org/"_s).has_value());
}

TEST_F(QuirksTest, NeedsCustomUserAgentOverrideOutlook)
{
    auto agent = customUserAgentFor("https://outlook.live.com/mail/0/"_s);
    ASSERT_TRUE(agent.has_value());
    EXPECT_TRUE(agent->contains("Chrome/"_s));

    EXPECT_FALSE(customUserAgentFor("https://live.com/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://onedrive.live.com/"_s).has_value());
}

TEST_F(QuirksTest, NeedsCustomUserAgentOverrideGroupCall)
{
    for (auto url : { "https://www.messenger.com/groupcall/ROOM:12345"_s, "https://www.facebook.com/groupcall/ROOM:12345"_s }) {
        auto agent = WebCore::Quirks::needsCustomUserAgentOverride(URL { url }, "TestApp"_s, "TestBase/1.0 (KHTML, like Gecko) Trailer/1.0"_s);
        ASSERT_TRUE(agent.has_value());
        EXPECT_TRUE(agent->contains("Chrome/"_s));
    }

    EXPECT_FALSE(customUserAgentFor("https://www.facebook.com/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://www.messenger.com/t/12345"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://www.facebook.com/groupcall/"_s).has_value());
}

TEST_F(QuirksTest, NeedsCustomUserAgentOverrideIsRegistrableDomainGranularity)
{
    EXPECT_EQ(customUserAgentFor("https://app.101edu.co/"_s).has_value(), customUserAgentFor("https://101edu.co/"_s).has_value());
    EXPECT_EQ(customUserAgentFor("https://app.aktiv.com/"_s).has_value(), customUserAgentFor("https://aktiv.com/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://app.101edu.co/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://app.aktiv.com/"_s).has_value());
}

#if PLATFORM(COCOA)
TEST_F(QuirksTest, NeedsCustomUserAgentOverrideRewritesBaseAgent)
{
    struct Case {
        ASCIILiteral url;
        ASCIILiteral marker;
    };

    Case cases[] = {
        { "https://www.tiktok.com/"_s, "like Chrome/136."_s },
        { "https://mms.pinduoduo.com/"_s, "like Chrome/149."_s },
        { "https://github.com/WebKit/WebKit"_s, "like Chrome/151."_s },
    };

    for (auto testCase : cases) {
        auto agent = customUserAgentFor(testCase.url);
        ASSERT_TRUE(agent.has_value());
        EXPECT_TRUE(agent->contains(testCase.marker));
        EXPECT_TRUE(agent->startsWith("TestBase/1.0"_s));
        EXPECT_TRUE(agent->contains("Trailer/1.0"_s));
    }

    EXPECT_FALSE(customUserAgentFor("https://www.pinduoduo.com/"_s).has_value());

    EXPECT_TRUE(customUserAgentFor("https://gist.github.com/"_s).has_value());
    EXPECT_TRUE(customUserAgentFor("https://ads.tiktok.com/"_s).has_value());
}
#endif // PLATFORM(COCOA)

#if PLATFORM(IOS)
TEST_F(QuirksTest, NeedsCustomUserAgentOverrideAmazonPrimeVideo)
{
    auto agent = customUserAgentFor("https://www.amazon.com/gp/video/"_s);
    ASSERT_TRUE(agent.has_value());
    EXPECT_TRUE(agent->contains("Chrome/"_s));

    EXPECT_TRUE(customUserAgentFor("https://www.amazon.co.uk/gp/video/"_s).has_value());

    EXPECT_FALSE(customUserAgentFor("https://www.amazon.com/"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://www.amazon.com/gp/video/storefront"_s).has_value());
}
#endif // PLATFORM(IOS)

} // namespace TestWebKitAPI
