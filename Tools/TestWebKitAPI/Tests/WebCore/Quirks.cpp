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
    return WebCore::resolveSiteSpecificQuirks(URL { urlString }, URL { urlString }, WebCore::IsTopDocument::Yes);
}

static WebCore::QuirksData resolveQuirksForEmbeddedDocument(ASCIILiteral topURLString, ASCIILiteral documentURLString)
{
    return WebCore::resolveSiteSpecificQuirks(URL { topURLString }, URL { documentURLString }, WebCore::IsTopDocument::No);
}

static std::optional<WebCore::ParameterizedQuirk::EvaluateScriptBeforeRunningScript> firstScriptBehavior(const WebCore::QuirksData& quirksData)
{
    std::optional<WebCore::ParameterizedQuirk::EvaluateScriptBeforeRunningScript> result;
    quirksData.forEachBehavior<WebCore::ParameterizedQuirk::EvaluateScriptBeforeRunningScript>([&](auto& behavior) {
        if (!result)
            result = behavior;
    });
    return result;
}

TEST_F(QuirksTest, TheQuirkTableIsWellFormed)
{
    WebCore::validateQuirkTable();
}

TEST_F(QuirksTest, SiteSpecificQuirksResolveWithoutADocument)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    EXPECT_TRUE(resolveQuirksForTopURL("https://www.airindiaexpress.com/"_s).quirkIsEnabled(SiteSpecificQuirk::NeedsAirIndiaExpressLayeringQuirk));
    EXPECT_TRUE(resolveQuirksForTopURL("https://www.scribd.com/"_s).quirkIsEnabled(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk));

    EXPECT_TRUE(resolveQuirksForTopURL("https://www.linkedin.com/"_s).isSite(WebCore::QuirkSite::LinkedIn));

    auto unrelatedSiteQuirks = resolveQuirksForTopURL("https://www.example.com/"_s);
    EXPECT_TRUE(unrelatedSiteQuirks.behaviors.isEmpty());
    EXPECT_FALSE(unrelatedSiteQuirks.isSite(WebCore::QuirkSite::LinkedIn));
}

TEST_F(QuirksTest, ParameterizedQuirksCarryTheirPayloadInTheTable)
{
    auto iHeartScript = firstScriptBehavior(resolveQuirksForTopURL("https://www.iheart.com/"_s));
    ASSERT_TRUE(iHeartScript);
    EXPECT_STREQ(iHeartScript->script.characters(), "document.cookie = 'app=listen:60; path=/; domain=.iheart.com';");

    auto inVideoScript = firstScriptBehavior(resolveQuirksForTopURL("https://www.invideo.io/"_s));
    ASSERT_TRUE(inVideoScript);
    EXPECT_STREQ(inVideoScript->script.characters(), "if(!window.chrome)window.chrome={};");

    auto unparameterizedQuirks = resolveQuirksForTopURL("https://www.example.com/"_s);
    EXPECT_TRUE(unparameterizedQuirks.behaviors.isEmpty());
}

TEST_F(QuirksTest, ParameterizedQuirksResolveToAPayloadAndNothingElse)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;
    using EvaluateScriptBeforeRunningScript = WebCore::ParameterizedQuirk::EvaluateScriptBeforeRunningScript;

    for (auto url : { "https://www.iheart.com/"_s, "https://www.invideo.io/"_s }) {
        auto quirks = resolveQuirksForTopURL(url);
        ASSERT_EQ(quirks.behaviors.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<EvaluateScriptBeforeRunningScript>(quirks.behaviors[0]));
    }

    auto unparameterized = resolveQuirksForTopURL("https://www.scribd.com/"_s);
    EXPECT_TRUE(unparameterized.quirkIsEnabled(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk));
    EXPECT_FALSE(firstScriptBehavior(unparameterized));
}

TEST_F(QuirksTest, UngatedScriptParameterAppliesToEveryScriptURL)
{
    auto script = firstScriptBehavior(resolveQuirksForTopURL("https://www.iheart.com/"_s));
    ASSERT_TRUE(script);

    EXPECT_TRUE(script->appliesTo(URL { "https://www.iheart.com/anything.js"_s }));
    EXPECT_TRUE(script->appliesTo(URL { "https://cdn.example.com/other.js"_s }));
}

TEST_F(QuirksTest, GatedScriptParameterAppliesOnlyToMatchingScriptURLs)
{
    auto script = firstScriptBehavior(resolveQuirksForTopURL("https://ceac.state.gov/"_s));
    ASSERT_TRUE(script);

    EXPECT_TRUE(script->appliesTo(URL { "https://ceac.state.gov/js/CheckBrowserClose.js"_s }));
    EXPECT_TRUE(script->appliesTo(URL { "https://cdn.example.com/CheckBrowserClose.js"_s }));

    EXPECT_FALSE(script->appliesTo(URL { "https://ceac.state.gov/js/CheckBrowserClose.js.map"_s }));
    EXPECT_FALSE(script->appliesTo(URL { "https://ceac.state.gov/CheckBrowserClose.js/inner.js"_s }));
    EXPECT_FALSE(script->appliesTo(URL { "https://ceac.state.gov/js/other.js"_s }));
}


static WebCore::QuirkBehavior scriptBehavior(ASCIILiteral script, std::optional<WebCore::QuirkMatch> gate = std::nullopt)
{
    return WebCore::ParameterizedQuirk::EvaluateScriptBeforeRunningScript::create(script, gate);
}

TEST_F(QuirksTest, EveryBehaviorNamesItself)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    auto bareQuirk = WebCore::QuirkBehavior { SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk };
    EXPECT_STREQ(WebCore::quirkBehaviorName(bareQuirk).utf8().data(), "NeedsReuseLiveRangeForSelectionUpdateQuirk");
    EXPECT_STREQ(WebCore::quirkBehaviorName(scriptBehavior("first();"_s)).utf8().data(), "EvaluateScriptBeforeRunningScript");
}

TEST_F(QuirksTest, EnablingABareQuirkIsIdempotentButPayloadsAccumulate)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    WebCore::QuirksData data;
    data.enableQuirk(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk);
    data.enableQuirk(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk);

    EXPECT_EQ(data.behaviors.size(), 1u);
    EXPECT_TRUE(data.quirkIsEnabled(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk));

    data.behaviors.append(scriptBehavior("first();"_s));
    data.behaviors.append(scriptBehavior("second();"_s));
    EXPECT_EQ(data.behaviors.size(), 3u);
}

TEST_F(QuirksTest, ClearingAQuirkStateRemovesItFromTheBehaviorList)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    WebCore::QuirksData data;
    data.setQuirkState(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk, true);
    data.behaviors.append(scriptBehavior("first();"_s));

    data.setQuirkState(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk, false);

    EXPECT_FALSE(data.quirkIsEnabled(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk));
    EXPECT_EQ(data.behaviors.size(), 1u);
    EXPECT_TRUE(firstScriptBehavior(data));
}

TEST_F(QuirksTest, ASingleScriptIsReturnedVerbatim)
{
    WebCore::QuirksData data;
    data.behaviors.append(scriptBehavior("first();"_s));

    EXPECT_STREQ(data.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/a.js"_s }).utf8().data(), "first();");
}

TEST_F(QuirksTest, TwoMatchingScriptsAreConcatenatedInTableOrder)
{
    WebCore::QuirksData data;
    data.behaviors.append(scriptBehavior("first();"_s));
    data.behaviors.append(scriptBehavior("second();"_s));

    EXPECT_STREQ(data.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/a.js"_s }).utf8().data(), "first();\n;\nsecond();");
}

TEST_F(QuirksTest, ConcatenationSkipsScriptsWhoseGateDoesNotMatch)
{
    using namespace WebCore::QuirkRefinement;

    WebCore::QuirksData data;
    data.behaviors.append(scriptBehavior("ungated();"_s));
    data.behaviors.append(scriptBehavior("gated();"_s, WebCore::QuirkMatch::anyURL().when(lastPathComponentIs("wanted.js"_s))));

    EXPECT_STREQ(data.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/other.js"_s }).utf8().data(), "ungated();");
    EXPECT_STREQ(data.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/wanted.js"_s }).utf8().data(), "ungated();\n;\ngated();");
}

TEST_F(QuirksTest, NoMatchingScriptYieldsAnEmptyString)
{
    using namespace WebCore::QuirkRefinement;

    WebCore::QuirksData data;
    data.behaviors.append(scriptBehavior("gated();"_s, WebCore::QuirkMatch::anyURL().when(lastPathComponentIs("wanted.js"_s))));

    EXPECT_TRUE(data.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/other.js"_s }).isEmpty());
    EXPECT_TRUE(WebCore::QuirksData { }.scriptsToEvaluateBeforeRunningScript(URL { "https://example.com/a.js"_s }).isEmpty());
}

TEST_F(QuirksTest, ARealTableEntryResolvesToItsScriptAloneWithNoSeparator)
{
    auto script = resolveQuirksForTopURL("https://www.iheart.com/"_s).scriptsToEvaluateBeforeRunningScript(URL { "https://www.iheart.com/a.js"_s });
    EXPECT_STREQ(script.utf8().data(), "document.cookie = 'app=listen:60; path=/; domain=.iheart.com';");
}

TEST_F(QuirksTest, ScriptGatesCanRequireBothAHostAndAFileName)
{
    auto script = firstScriptBehavior(resolveQuirksForTopURL("https://www.dictionary.com/"_s));

#if PLATFORM(IOS_FAMILY)
    ASSERT_TRUE(script);

    EXPECT_TRUE(script->appliesTo(URL { "https://player.anyclip.com/foo-lre.js"_s }));

    EXPECT_FALSE(script->appliesTo(URL { "https://player.anyclip.com/foo.js"_s }));
    EXPECT_FALSE(script->appliesTo(URL { "https://cdn.example.com/foo-lre.js"_s }));
#else
    EXPECT_FALSE(script);
#endif
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

TEST_F(QuirksTest, ADocumentMatchAppliesToAnEmbedRegardlessOfTheEmbeddingPage)
{
#if PLATFORM(IOS_FAMILY)
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.theguardian.com/film"_s, "https://x.com/i/status/123"_s)
        .quirkIsEnabled(SiteSpecificQuirk::ShouldDisableElementFullscreenQuirk));
    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://platform.x.com/embed/Tweet.html"_s)
        .quirkIsEnabled(SiteSpecificQuirk::ShouldDisableElementFullscreenQuirk));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://vimeo.com/12345"_s)
        .quirkIsEnabled(SiteSpecificQuirk::ShouldDisableElementFullscreenQuirk));

    EXPECT_FALSE(resolveQuirksForTopURL("https://x.com/i/status/123"_s)
        .quirkIsEnabled(SiteSpecificQuirk::ShouldDisableElementFullscreenQuirk));
#endif
}

TEST_F(QuirksTest, NamingBothURLsRequiresBothToMatch)
{
#if PLATFORM(IOS_FAMILY)
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.theguardian.com/film"_s, "https://www.youtube.com/embed/abc"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk));
    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.theguardian.co.uk/film"_s, "https://www.youtube-nocookie.com/embed/abc"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.theguardian.com/film"_s, "https://vimeo.com/12345"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk));
    EXPECT_FALSE(resolveQuirksForTopURL("https://www.theguardian.com/film"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk));
#endif
}

TEST_F(QuirksTest, APageMatchStillAppliesInsideAnEmbeddedDocument)
{
    using SiteSpecificQuirk = WebCore::SiteSpecificQuirk;

    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.airindiaexpress.com/"_s, "https://ads.example.com/frame.html"_s)
        .quirkIsEnabled(SiteSpecificQuirk::NeedsAirIndiaExpressLayeringQuirk));
}

} // namespace TestWebKitAPI
