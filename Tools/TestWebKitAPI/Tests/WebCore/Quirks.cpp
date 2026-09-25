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
#include "TestPageHarness.h"

#include <WebCore/DocumentQuirks.h>
#include <WebCore/NodeInlines.h>
#include <WebCore/QuirkSelectors.h>
#include <WebCore/QuirkTable.h>
#include <WebCore/Quirks.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <array>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore::QuirkSelectors;

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
    EXPECT_TRUE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://vimeo.com/12345"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsYouTubeCaptionQuirk));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.youtube-nocookie.com/embed/abc"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsYouTubeCaptionQuirk));
}
#endif

TEST_F(QuirksTest, SiteSpecificQuirksResolveWithoutADocument)
{
    EXPECT_TRUE(resolveQuirksForTopURL("https://www.airindiaexpress.com/"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk));
    EXPECT_TRUE(resolveQuirksForTopURL("https://www.scribd.com/"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsReuseLiveRangeForSelectionUpdateQuirk));

    EXPECT_TRUE(resolveQuirksForTopURL("https://www.bankofamerica.com/"_s).isSite(WebCore::QuirkSite::BankOfAmerica));

    auto unrelatedSiteQuirks = resolveQuirksForTopURL("https://www.example.com/"_s);
    EXPECT_FALSE(unrelatedSiteQuirks.hasBehaviors());
    EXPECT_FALSE(unrelatedSiteQuirks.isSite(WebCore::QuirkSite::BankOfAmerica));
}

static Vector<String> scriptsForScriptURL(const WebCore::QuirksData& quirks, ASCIILiteral scriptURLString)
{
    Vector<String> scripts;
    WebCore::URLMatchContext scriptURLContext { URL { scriptURLString } };

    for (const auto& behavior : quirks.behaviors()) {
        if (!behavior.parameters)
            continue;

        if (behavior.parameters->script.length() && behavior.secondaryURLConditionMatches(scriptURLContext))
            scripts.append(behavior.parameters->script);
    }

    return scripts;
}

TEST_F(QuirksTest, ScriptQuirkWithoutAScriptURLMatchAppliesToEveryScript)
{
    auto quirks = resolveQuirksForTopURL("https://www.iheart.com/"_s);

    auto scripts = scriptsForScriptURL(quirks, "https://cdn.example.com/vendor.js"_s);
    ASSERT_EQ(scripts.size(), 1u);
    EXPECT_TRUE(scripts[0].contains("app=listen:60"_s));
}

TEST_F(QuirksTest, ScriptQuirkWithAScriptURLMatchAppliesOnlyToMatchingScripts)
{
    auto quirks = resolveQuirksForTopURL("https://ceac.state.gov/GenNIV/Default.aspx"_s);

    auto scripts = scriptsForScriptURL(quirks, "https://ceac.state.gov/js/CheckBrowserClose.js"_s);
    ASSERT_EQ(scripts.size(), 1u);
    EXPECT_TRUE(scripts[0].contains("__ceacBeforeUnloadFix"_s));

    EXPECT_TRUE(scriptsForScriptURL(quirks, "https://ceac.state.gov/js/Other.js"_s).isEmpty());
}

TEST_F(QuirksTest, DocumentsWithoutAScriptQuirkGetNoParameters)
{
    auto quirks = resolveQuirksForTopURL("https://www.example.com/"_s);
    EXPECT_FALSE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk));
    EXPECT_TRUE(scriptsForScriptURL(quirks, "https://www.example.com/app.js"_s).isEmpty());
}

TEST_F(QuirksTest, ParametersAreOnlyReturnedForTheBehaviorThatSuppliedThem)
{
    static constexpr auto behaviors = WTF::toArray({
        WebCore::QuirkBehaviors::needsAirIndiaExpressLayeringQuirk,
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("script"_s)),
    });

    WebCore::QuirksData quirks;
    for (const auto& behavior : behaviors)
        quirks.addBehavior(behavior);

    WebCore::URLMatchContext context { URL { "https://www.example.com/app.js"_s } };

    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk));
    EXPECT_EQ(scriptsForScriptURL(quirks, "https://www.example.com/app.js"_s), Vector<String> { "script"_str });
}

TEST_F(QuirksTest, OneQuirkCanCarryDifferentParametersForDifferentScriptURLs)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto firstScriptURL = WebCore::URLMatch::host("first.example.com"_s);
    static constexpr auto secondScriptURL = WebCore::URLMatch::host("second.example.com"_s);

    static constexpr auto behaviors = WTF::toArray({
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("firstScript"_s)).when(secondaryURLMatches(firstScriptURL)),
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("secondScript"_s)).when(secondaryURLMatches(secondScriptURL)),
    });

    WebCore::QuirksData quirks;
    for (const auto& behavior : behaviors)
        quirks.addBehavior(behavior);

    EXPECT_EQ(scriptsForScriptURL(quirks, "https://first.example.com/a.js"_s), Vector<String> { "firstScript"_str });
    EXPECT_EQ(scriptsForScriptURL(quirks, "https://second.example.com/b.js"_s), Vector<String> { "secondScript"_str });

    EXPECT_TRUE(scriptsForScriptURL(quirks, "https://third.example.com/c.js"_s).isEmpty());

    constexpr auto id = WebCore::QuirkBehaviorID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk;
    EXPECT_TRUE(quirks.behaviorAppliesToURL(id, URL { "https://first.example.com/a.js"_s }));
    EXPECT_TRUE(quirks.behaviorAppliesToURL(id, URL { "https://second.example.com/b.js"_s }));
    EXPECT_FALSE(quirks.behaviorAppliesToURL(id, URL { "https://third.example.com/c.js"_s }));

    quirks.addBehavior(WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("unscopedScript"_s)));
    EXPECT_TRUE(quirks.behaviorAppliesToURL(id, URL { "https://third.example.com/c.js"_s }));
}

TEST_F(QuirksTest, EveryMatchingRowContributesWhenSeveralSupplyTheSameBehavior)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto anyScriptURL = WebCore::URLMatch::anyURL();

    static constexpr auto behaviors = WTF::toArray({
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("firstScript"_s)).when(secondaryURLMatches(anyScriptURL)),
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("secondScript"_s)).when(secondaryURLMatches(anyScriptURL)),
    });

    WebCore::QuirksData quirks;
    for (const auto& behavior : behaviors)
        quirks.addBehavior(behavior);

    Vector<String> expected { "firstScript"_str, "secondScript"_str };
    EXPECT_EQ(scriptsForScriptURL(quirks, "https://first.example.com/a.js"_s), expected);
}

static Vector<String> elementSelectorsFor(const WebCore::QuirksData& quirks, WebCore::QuirkBehaviorID id)
{
    return WTF::compactMap(quirks.behaviors(), [&](const auto& behavior) -> std::optional<String> {
        if (behavior.id == id && behavior.elementSelectorCondition)
            return *behavior.elementSelectorCondition;
        return std::nullopt;
    });
}

TEST_F(QuirksTest, AnElementSelectorConditionIsRecordedOnTheBehavior)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto behavior = WebCore::QuirkBehaviors::shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(".target, .target *"_s));

    ASSERT_TRUE(behavior.elementSelectorCondition.has_value());
    EXPECT_EQ(String { *behavior.elementSelectorCondition }, ".target, .target *"_str);
    EXPECT_FALSE(behavior.parameters.has_value());
}

TEST_F(QuirksTest, ASecondaryURLConditionIsRecordedOnTheBehavior)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto scriptURL = WebCore::URLMatch::host("cdn.example.com"_s);
    static constexpr auto behavior = WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("script"_s)).when(secondaryURLMatches(scriptURL));

    ASSERT_TRUE(behavior.parameters.has_value());
    ASSERT_TRUE(behavior.secondaryURLCondition.has_value());
    EXPECT_TRUE(behavior.secondaryURLConditionMatches(WebCore::URLMatchContext { URL { "https://cdn.example.com/a.js"_s } }));
    EXPECT_FALSE(behavior.secondaryURLConditionMatches(WebCore::URLMatchContext { URL { "https://other.example.com/a.js"_s } }));

    static constexpr auto unscoped = WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("script"_s));
    EXPECT_FALSE(unscoped.secondaryURLCondition.has_value());
    EXPECT_TRUE(unscoped.secondaryURLConditionMatches(WebCore::URLMatchContext { URL { "https://other.example.com/a.js"_s } }));
}

TEST_F(QuirksTest, BehaviorAppliesToURLRequiresTheBehaviorToBeEnabled)
{
    WebCore::QuirksData quirks;
    quirks.addBehavior(WebCore::QuirkBehaviors::needsAirIndiaExpressLayeringQuirk);

    EXPECT_FALSE(quirks.behaviorAppliesToURL(WebCore::QuirkBehaviorID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk, URL { "https://www.example.com/a.js"_s }));
    EXPECT_TRUE(quirks.behaviorAppliesToURL(WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk, URL { "https://www.example.com/a.js"_s }));
}

TEST_F(QuirksTest, MediaRangeRewriteAppliesOnlyToBingRequestURLs)
{
    constexpr auto id = WebCore::QuirkBehaviorID::NeedsMediaRewriteRangeRequestQuirk;

    auto bing = resolveQuirksForTopURL("https://www.bing.com/videos"_s);
    EXPECT_TRUE(bing.behaviorAppliesToURL(id, URL { "https://th.bing.com/video.mp4"_s }));
    EXPECT_FALSE(bing.behaviorAppliesToURL(id, URL { "https://cdn.example.com/video.mp4"_s }));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).behaviorAppliesToURL(id, URL { "https://th.bing.com/video.mp4"_s }));
}

TEST_F(QuirksTest, LogoutCookieCleanupAppliesOnlyToTheLogoutEndpoint)
{
    constexpr auto id = WebCore::QuirkBehaviorID::NeedsLogoutCookieCleanupQuirk;

    auto claude = resolveQuirksForTopURL("https://claude.ai/chat"_s);
    EXPECT_TRUE(claude.behaviorAppliesToURL(id, URL { "https://claude.ai/api/auth/logout"_s }));
    for (auto urlString : { "https://claude.ai/api/auth/logout/"_s, "https://claude.ai/api/auth/login"_s, "https://api.claude.ai/api/auth/logout"_s, "https://claude.com/api/auth/logout"_s })
        EXPECT_FALSE(claude.behaviorAppliesToURL(id, URL { urlString }));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).behaviorAppliesToURL(id, URL { "https://claude.ai/api/auth/logout"_s }));
}

TEST_F(QuirksTest, LogoutCookieCleanupCarriesTheCookiesToDelete)
{
    auto behaviors = resolveQuirksForTopURL("https://claude.ai/"_s).behaviorsMatching(WebCore::QuirkBehaviorID::NeedsLogoutCookieCleanupQuirk);
    ASSERT_EQ(behaviors.size(), 1u);
    ASSERT_TRUE(behaviors[0].parameters.has_value());

    auto cookieNames = WTF::map(behaviors[0].parameters->cookieNames, [](auto name) {
        return String { name };
    });
    Vector<String> expected { "__ssid"_str, "__cf_bm"_str, "anthropic-device-id"_str, "lastActiveOrg"_str, "activitySessionId"_str };
    EXPECT_EQ(cookieNames, expected);
}

#if PLATFORM(IOS_FAMILY)
TEST_F(QuirksTest, OneDrivePopupBypassRequiresAOneDriveTarget)
{
    constexpr auto id = WebCore::QuirkBehaviorID::ShouldAllowPopupFromMicrosoftOfficeToOneDrive;

    auto m365 = resolveQuirksForTopURL("https://m365.cloud.microsoft/"_s);
    EXPECT_TRUE(m365.behaviorAppliesToURL(id, URL { "https://onedrive.live.com/edit"_s }));
    EXPECT_TRUE(m365.behaviorAppliesToURL(id, URL { "https://foo.onedrive.live.com/"_s }));
    EXPECT_FALSE(m365.behaviorAppliesToURL(id, URL { "https://xonedrive.live.com/"_s }));
    EXPECT_FALSE(m365.behaviorAppliesToURL(id, URL { "https://live.com/"_s }));

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).behaviorAppliesToURL(id, URL { "https://onedrive.live.com/edit"_s }));
}

TEST_F(QuirksTest, ChromeOSUserAgentAppliesOnlyToTheWordEditorScriptInItsFrame)
{
    constexpr auto id = WebCore::QuirkBehaviorID::NeedsChromeOSNavigatorUserAgentQuirk;
    constexpr auto editorFrameURL = "https://word-edit.officeapps.live.com/we/wordeditorframe.aspx"_s;

    auto editorFrame = resolveQuirksForEmbeddedDocument("https://onedrive.live.com/"_s, editorFrameURL);
    EXPECT_TRUE(editorFrame.behaviorAppliesToURL(id, URL { "https://cdn.example.com/we/wordeditords.js"_s }));
    EXPECT_FALSE(editorFrame.behaviorAppliesToURL(id, URL { "https://cdn.example.com/we/other.js"_s }));

    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://onedrive.live.com/"_s, "https://word-edit.officeapps.live.com/we/other.aspx"_s).isBehaviorEnabled(id));
    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, editorFrameURL).isBehaviorEnabled(id));
    EXPECT_FALSE(resolveQuirksForTopURL(editorFrameURL).isBehaviorEnabled(id));
    EXPECT_FALSE(resolveQuirksForTopURL("https://onedrive.live.com/"_s).isBehaviorEnabled(id));
}
#endif

TEST_F(QuirksTest, AddingAndRemovingBehaviorsKeepsTheEnabledFlagInSync)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto onFirst = WebCore::QuirkBehaviors::shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(".first"_s));
    static constexpr auto onSecond = WebCore::QuirkBehaviors::shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(".second"_s));

    WebCore::QuirksData quirks;
    EXPECT_FALSE(quirks.hasBehaviors());
    EXPECT_FALSE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));

    quirks.addBehavior(onFirst);
    quirks.addBehavior(onSecond);
    EXPECT_TRUE(quirks.hasBehaviors());
    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), (Vector<String> { ".first"_str, ".second"_str }));

    quirks.setEnabled(onFirst, false);
    EXPECT_FALSE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_TRUE(quirks.behaviors().isEmpty());
    EXPECT_FALSE(quirks.hasBehaviors());

    quirks.setEnabled(onSecond, true);
    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), Vector<String> { ".second"_str });
}

TEST_F(QuirksTest, MergeUnionsFlagsAndSitesAndConcatenatesBehaviors)
{
    using namespace WebCore::QuirkBehaviorConditions;
    static constexpr auto onFirst = WebCore::QuirkBehaviors::shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(".first"_s));
    static constexpr auto onSecond = WebCore::QuirkBehaviors::shouldDispatchSimulatedMouseEventsQuirk.when(elementMatchesSelector(".second"_s));

    WebCore::QuirksData quirks;
    quirks.addBehavior(onFirst);
    quirks.addSite(WebCore::QuirkSite::Vimeo);

    WebCore::QuirksData other;
    other.addBehavior(onSecond);
    other.addBehavior(WebCore::QuirkBehaviors::needsAirIndiaExpressLayeringQuirk);
    other.addSite(WebCore::QuirkSite::BankOfAmerica);

    quirks.merge(other);

    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk));
    EXPECT_TRUE(quirks.isSite(WebCore::QuirkSite::Vimeo));
    EXPECT_TRUE(quirks.isSite(WebCore::QuirkSite::BankOfAmerica));
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), (Vector<String> { ".first"_str, ".second"_str }));
}

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
TEST_F(QuirksTest, SitesThatOnlyNeedSimulatedMouseEventsOnSomeElementsCarryASelector)
{
    for (auto urlString : { "https://www.facebook.com/"_s, "https://www.tiktok.com/"_s }) {
        auto quirks = resolveQuirksForTopURL(urlString);
        EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
        EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), Vector<String> { String { onSliderRole } });
    }

    auto myBinder = resolveQuirksForTopURL("https://mybinder.org/"_s);
    EXPECT_TRUE(myBinder.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_EQ(elementSelectorsFor(myBinder, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), Vector<String> { String { onDockPanelTabBar } });
}

TEST_F(QuirksTest, SitesThatNeedSimulatedMouseEventsEverywhereCarryNoSelector)
{
    for (auto urlString : { "https://www.airtable.com/"_s, "https://www.flipkart.com/"_s, "https://www.amazon.com/"_s, "https://soundcloud.com/"_s, "https://www.wix.com/"_s }) {
        auto quirks = resolveQuirksForTopURL(urlString);
        EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
        EXPECT_TRUE(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk).isEmpty());
    }
}
#endif // ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)

static bool matchesSelector(ASCIILiteral selector, const WebCore::Node* node)
{
    return WebCore::Quirks::elementMatchesSelectorCondition(selector, node);
}

TEST_F(QuirksTest, ATargetThatIsNotAnElementAndHasNoParentElementNeverMatches)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div role='slider'></div>"_s);

    EXPECT_FALSE(matchesSelector(onSliderRole, nullptr));
    EXPECT_FALSE(matchesSelector(onSliderRole, &page.document()));
}

TEST_F(QuirksTest, AnElementTargetIsMatchedAgainstTheSelectorItself)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div id='slider' role='slider'></div><div id='plain'></div>"_s);

    EXPECT_TRUE(matchesSelector(onSliderRole, page.getElementById("slider"_s).get()));
    EXPECT_FALSE(matchesSelector(onSliderRole, page.getElementById("plain"_s).get()));
}

TEST_F(QuirksTest, DescendantsMatchOnlyBecauseTheSelectorReachesDownToThem)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div role='slider'><span><b id='thumb'>x</b></span></div>"_s);

    RefPtr thumb = page.getElementById("thumb"_s);

    EXPECT_TRUE(matchesSelector(onSliderRole, thumb.get()));

    EXPECT_FALSE(matchesSelector("[role=slider]"_s, thumb.get()));
}

TEST_F(QuirksTest, TextNodeTargetsResolveToTheirParentElement)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div role='slider'><b id='thumb'>inside</b></div><p id='plain'>outside</p>"_s);

    RefPtr inside = page.getElementById("thumb"_s)->firstChild();
    ASSERT_TRUE(inside && inside->isTextNode());
    EXPECT_TRUE(matchesSelector(onSliderRole, inside.get()));

    RefPtr outside = page.getElementById("plain"_s)->firstChild();
    ASSERT_TRUE(outside && outside->isTextNode());
    EXPECT_FALSE(matchesSelector(onSliderRole, outside.get()));
}

TEST_F(QuirksTest, DetachedTargetsAreStillMatchedAgainstTheirOwnSubtree)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div id='slider' role='slider'><b id='thumb'>x</b></div>"_s);

    RefPtr slider = page.getElementById("slider"_s);
    RefPtr thumb = page.getElementById("thumb"_s);
    ASSERT_FALSE(slider->remove().hasException());

    EXPECT_TRUE(matchesSelector(onSliderRole, slider.get()));
    EXPECT_TRUE(matchesSelector(onSliderRole, thumb.get()));
}

TEST_F(QuirksTest, MixedCaseClassSelectorMatchesInEitherDocumentMode)
{
    for (auto doctype : { ""_s, "<!DOCTYPE html>"_s }) {
        auto page = TestPageHarness::create();
        page.loadHTML(makeString(doctype, "<div id='tabBar' class='lm-DockPanel-tabBar'><b id='tab'>x</b></div>"_s));
        ASSERT_EQ(page.document().inQuirksMode(), doctype.isEmpty());

        EXPECT_TRUE(matchesSelector(onDockPanelTabBar, page.getElementById("tabBar"_s).get()));
        EXPECT_TRUE(matchesSelector(onDockPanelTabBar, page.getElementById("tab"_s).get()));
    }
}

TEST_F(QuirksTest, ConditionsSupportTheSelectorShapesAQuirkMightNeed)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><ul class='menu'><li id='item' class='entry active' data-kind='tab'>x</li></ul><p id='loose' class='entry'>y</p>"_s);

    RefPtr item = page.getElementById("item"_s);
    RefPtr loose = page.getElementById("loose"_s);

    EXPECT_TRUE(matchesSelector(".entry.active"_s, item.get()));
    EXPECT_FALSE(matchesSelector(".entry.active"_s, loose.get()));

    EXPECT_TRUE(matchesSelector("[data-kind='tab']"_s, item.get()));
    EXPECT_FALSE(matchesSelector("[data-kind='tab']"_s, loose.get()));

    EXPECT_TRUE(matchesSelector(".menu > .entry"_s, item.get()));
    EXPECT_FALSE(matchesSelector(".menu > .entry"_s, loose.get()));

    EXPECT_TRUE(matchesSelector(".nothing, [data-kind='tab']"_s, item.get()));
    EXPECT_FALSE(matchesSelector(".nothing, [data-kind='menu']"_s, item.get()));

    EXPECT_FALSE(matchesSelector(".entry:not(.active)"_s, item.get()));
    EXPECT_TRUE(matchesSelector(".entry:not(.active)"_s, loose.get()));
}

TEST_F(QuirksTest, TheSameSelectorIsEvaluatedPerDocument)
{
    auto quirksModePage = TestPageHarness::create();
    quirksModePage.loadHTML("<div id='tabBar' class='LM-DOCKPANEL-TABBAR'></div>"_s);

    auto standardsModePage = TestPageHarness::create();
    standardsModePage.loadHTML("<!DOCTYPE html><div id='tabBar' class='LM-DOCKPANEL-TABBAR'></div>"_s);

    EXPECT_TRUE(matchesSelector(onDockPanelTabBar, quirksModePage.getElementById("tabBar"_s).get()));
    EXPECT_FALSE(matchesSelector(onDockPanelTabBar, standardsModePage.getElementById("tabBar"_s).get()));
}

static Vector<String> selectorsFor(ASCIILiteral urlString, WebCore::QuirkBehaviorID id)
{
    return elementSelectorsFor(resolveQuirksForTopURL(urlString), id);
}

#if ENABLE(TOUCH_EVENTS)

TEST_F(QuirksTest, SitesThatAssumeDefaultPreventedScopeItToTheTargetElement)
{
    constexpr auto id = WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk;

    EXPECT_EQ(selectorsFor("https://www.amazon.com/"_s, id), Vector<String> { String { onAmazonMagnifierLens } });
    EXPECT_EQ(selectorsFor("https://soundcloud.com/"_s, id), Vector<String> { String { onSoundCloudSceneLayer } });

    for (auto urlString : { "https://www.facebook.com/"_s, "https://www.tiktok.com/"_s })
        EXPECT_EQ(selectorsFor(urlString, id), Vector<String> { String { onSliderRoleItself } });
}
#endif

#if ENABLE(TWO_PHASE_CLICKS)

TEST_F(QuirksTest, ContentObservationSelectorsAreScopedPerSite)
{
    constexpr auto id = WebCore::QuirkBehaviorID::MayNeedToIgnoreContentObservation;

    EXPECT_EQ(selectorsFor("https://www.google.com/maps/"_s, id), Vector<String> { String { onSuggestionsLabel } });
    EXPECT_EQ(selectorsFor("https://www.walmart.com/"_s, id), Vector<String> { String { onButtonInListItem } });
    EXPECT_EQ(selectorsFor("https://outlook.live.com/"_s, id), Vector<String> { String { onSwatchColorPicker } });
}
#endif

TEST_F(QuirksTest, OutlookSwatchPickerSelectorMatchesTheIDPrefix)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div id='swatchColorPicker'></div>"
        "<div id='swatchColorPicker-42'></div>"
        "<div id='SWATCHCOLORPICKER'></div>"
        "<div id='outerSwatchColorPicker'></div>"
        "<div id='plain'></div>"_s);

    EXPECT_TRUE(matchesSelector(onSwatchColorPicker, page.getElementById("swatchColorPicker"_s).get()));
    EXPECT_TRUE(matchesSelector(onSwatchColorPicker, page.getElementById("swatchColorPicker-42"_s).get()));

    EXPECT_FALSE(matchesSelector(onSwatchColorPicker, page.getElementById("SWATCHCOLORPICKER"_s).get()));
    EXPECT_FALSE(matchesSelector(onSwatchColorPicker, page.getElementById("outerSwatchColorPicker"_s).get()));
    EXPECT_FALSE(matchesSelector(onSwatchColorPicker, page.getElementById("plain"_s).get()));
}

TEST_F(QuirksTest, SingleSiteSelectorConditions)
{
    EXPECT_EQ(selectorsFor("https://www.ea.com/"_s, WebCore::QuirkBehaviorID::ShouldPreventKeyframeEffectAccelerationQuirk), Vector<String> { String { onEANetworkNav } });

    for (auto urlString : { "https://www.hotels.com/"_s, "https://www.expedia.fr/"_s, "https://www.ebookers.de/"_s })
        EXPECT_EQ(selectorsFor(urlString, WebCore::QuirkBehaviorID::NeedsExpediaGroupAnimationQuirk), Vector<String> { String { onExpediaOpeningMenu } });
}

#if PLATFORM(IOS_FAMILY)

TEST_F(QuirksTest, SingleSiteSelectorConditionsOnIOS)
{
    EXPECT_EQ(selectorsFor("https://www.theguardian.com/"_s, WebCore::QuirkBehaviorID::ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk), Vector<String> { String { onCrosswordID } });
    EXPECT_EQ(selectorsFor("https://www.cbssports.com/"_s, WebCore::QuirkBehaviorID::ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk), Vector<String> { String { onAviaButton } });
    EXPECT_EQ(selectorsFor("https://docs.google.com/"_s, WebCore::QuirkBehaviorID::ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk), Vector<String> { String { onGoogleDocsMLPromotion } });

#if ENABLE(IOS_TOUCH_EVENTS)
    EXPECT_EQ(selectorsFor("https://www.linkedin.com/"_s, WebCore::QuirkBehaviorID::ShouldAllowNativeTapsOnMediaElementsQuirk), Vector<String> { String { onVideoJSTech } });
#endif
}
#endif

TEST_F(QuirksTest, StorageAccessQuirksCarrySignInSelectors)
{
    constexpr auto signInID = WebCore::QuirkBehaviorID::NeedsStorageAccessOnLoginButtonClickQuirk;

    EXPECT_EQ(selectorsFor("https://www.microsoft.com/"_s, signInID), Vector<String> { String { onMicrosoftSignInButton } });
    for (auto urlString : { "https://www.playstation.com/"_s, "https://my.playstation.com/"_s })
        EXPECT_EQ(selectorsFor(urlString, signInID), Vector<String> { String { onPlayStationSignInButton } });

    EXPECT_FALSE(resolveQuirksForTopURL("https://microsoft.com/"_s).isBehaviorEnabled(signInID));
    EXPECT_FALSE(resolveQuirksForTopURL("https://playstation.com/"_s).isBehaviorEnabled(signInID));

    constexpr auto kinjaID = WebCore::QuirkBehaviorID::NeedsKinjaLoginStorageAccessQuirk;
    for (auto urlString : { "https://jalopnik.com/"_s, "https://kotaku.com/"_s, "https://theroot.com/"_s, "https://www.theinventory.com/"_s })
        EXPECT_EQ(selectorsFor(urlString, kinjaID), Vector<String> { String { onKinjaLoginAvatar } });

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.example.com/"_s).isBehaviorEnabled(kinjaID));
}

#if PLATFORM(COCOA)
TEST_F(QuirksTest, YouTubeWatchLaterStorageAccessIsEmbeddedOnly)
{
    constexpr auto id = WebCore::QuirkBehaviorID::NeedsStorageAccessForYouTubeWatchLaterQuirk;

    auto embedded = resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://www.youtube.com/embed/abc"_s);
    EXPECT_TRUE(embedded.isBehaviorEnabled(id));
    EXPECT_EQ(elementSelectorsFor(embedded, id), Vector<String> { String { onYouTubeWatchLaterIcon } });

    EXPECT_FALSE(resolveQuirksForTopURL("https://www.youtube.com/"_s).isBehaviorEnabled(id));
    EXPECT_FALSE(resolveQuirksForEmbeddedDocument("https://www.example.com/"_s, "https://vimeo.com/12345"_s).isBehaviorEnabled(id));
}
#endif

TEST_F(QuirksTest, MigratedSelectorsMatchWhatTheHandWrittenChecksDid)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div id='before'></div><div id='magnifierLens'></div><div id='after'></div>"
        "<div id='farBefore'></div><span></span><div id='lens2'></div>"
        "<div id='quick-crossword-3'></div><div id='CROSSWORD'></div><div id='cross'></div>"
        "<div role='listitem'><span id='gridButton' role='button'></span></div>"
        "<div role='LISTITEM'><span id='shoutyButton' role='BUTTON'></span></div>"
        "<div role='listitem'><i><span id='deepButton' role='button'></span></i></div>"
        "<video id='media' class='vjs-tech'>fallback</video><div id='notMedia' class='vjs-tech'></div>"_s);

    EXPECT_TRUE(matchesSelector(onAmazonMagnifierLens, page.getElementById("magnifierLens"_s).get()));
    EXPECT_TRUE(matchesSelector(onAmazonMagnifierLens, page.getElementById("before"_s).get()));
    EXPECT_FALSE(matchesSelector(onAmazonMagnifierLens, page.getElementById("after"_s).get()));
    EXPECT_FALSE(matchesSelector(onAmazonMagnifierLens, page.getElementById("farBefore"_s).get()));

    EXPECT_TRUE(matchesSelector(onCrosswordID, page.getElementById("quick-crossword-3"_s).get()));
    EXPECT_FALSE(matchesSelector(onCrosswordID, page.getElementById("CROSSWORD"_s).get()));
    EXPECT_FALSE(matchesSelector(onCrosswordID, page.getElementById("cross"_s).get()));

    EXPECT_TRUE(matchesSelector(onButtonInListItem, page.getElementById("gridButton"_s).get()));
    EXPECT_TRUE(matchesSelector(onButtonInListItem, page.getElementById("shoutyButton"_s).get()));
    EXPECT_FALSE(matchesSelector(onButtonInListItem, page.getElementById("deepButton"_s).get()));

    EXPECT_TRUE(matchesSelector(onVideoJSTech, page.getElementById("media"_s).get()));
    EXPECT_FALSE(matchesSelector(onVideoJSTech, page.getElementById("notMedia"_s).get()));

    RefPtr fallback = page.getElementById("media"_s)->firstChild();
    ASSERT_TRUE(fallback && fallback->isTextNode());
    EXPECT_TRUE(matchesSelector(onVideoJSTech, fallback.get()));
}

#if PLATFORM(IOS_FAMILY)
TEST_F(QuirksTest, GoogleDocsPromotionSelectorStopsAtTheGrandparent)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div id='self' class='docs-ml-promotion-action-container'>"
        "<span id='child'><i id='grandchild'><b id='greatGrandchild'>x</b></i></span>"
        "</div>"
        "<div id='unrelated'></div>"_s);

    EXPECT_TRUE(matchesSelector(onGoogleDocsMLPromotion, page.getElementById("self"_s).get()));
    EXPECT_TRUE(matchesSelector(onGoogleDocsMLPromotion, page.getElementById("child"_s).get()));
    EXPECT_TRUE(matchesSelector(onGoogleDocsMLPromotion, page.getElementById("grandchild"_s).get()));

    EXPECT_FALSE(matchesSelector(onGoogleDocsMLPromotion, page.getElementById("greatGrandchild"_s).get()));
    EXPECT_FALSE(matchesSelector(onGoogleDocsMLPromotion, page.getElementById("unrelated"_s).get()));
}
#endif

TEST_F(QuirksTest, ExpediaMenuSelectorMatchesInEitherDocumentMode)
{
    for (auto doctype : { ""_s, "<!DOCTYPE html>"_s }) {
        auto page = TestPageHarness::create();
        page.loadHTML(makeString(doctype,
            "<div class='uitk-menu-mounted'>"
            "<div id='opening' class='uitk-menu-container uitk-menu-container-autoposition uitk-menu-container-has-intersection-root-el uitk-menu-open'></div>"
            "<div id='closed' class='uitk-menu-container uitk-menu-container-autoposition uitk-menu-container-has-intersection-root-el'></div>"
            "</div>"
            "<div id='unmounted' class='uitk-menu-container uitk-menu-container-autoposition uitk-menu-container-has-intersection-root-el uitk-menu-open'></div>"_s));
        ASSERT_EQ(page.document().inQuirksMode(), doctype.isEmpty());

        EXPECT_TRUE(matchesSelector(onExpediaOpeningMenu, page.getElementById("opening"_s).get()));
        EXPECT_FALSE(matchesSelector(onExpediaOpeningMenu, page.getElementById("closed"_s).get()));
        EXPECT_FALSE(matchesSelector(onExpediaOpeningMenu, page.getElementById("unmounted"_s).get()));
    }
}

TEST_F(QuirksTest, KinjaAvatarSelectorCoversTheSVGAndItsDirectPath)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div id='burner' class='js_switch-to-burner-login'></div>"
        "<div id='plain'></div>"_s);

    Ref avatars = page.document().createElementForBindings("div"_s).releaseReturnValue();
    EXPECT_FALSE(avatars->setInnerHTML(String {
        "<svg id='avatar' aria-label='UserFilled icon'><path id='glyph'></path><g><path id='deepGlyph'></path></g></svg>"
        "<svg id='lowercase' aria-label='userfilled icon'></svg>"_s }).hasException());

    auto avatarElement = [&](ASCIILiteral id) {
        return avatars->querySelector(makeString('#', id)).releaseReturnValue();
    };

    EXPECT_TRUE(matchesSelector(onKinjaLoginAvatar, page.getElementById("burner"_s).get()));
    EXPECT_TRUE(matchesSelector(onKinjaLoginAvatar, avatarElement("avatar"_s)));
    EXPECT_TRUE(matchesSelector(onKinjaLoginAvatar, avatarElement("glyph"_s)));

    EXPECT_FALSE(matchesSelector(onKinjaLoginAvatar, avatarElement("deepGlyph"_s)));
    EXPECT_FALSE(matchesSelector(onKinjaLoginAvatar, avatarElement("lowercase"_s)));
    EXPECT_FALSE(matchesSelector(onKinjaLoginAvatar, page.getElementById("plain"_s).get()));
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
        return resolveQuirksForTopURL(urlString).isBehaviorEnabled(WebCore::QuirkBehaviorID::IsMicrosoftTeamsRedirectURLQuirk);
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
    EXPECT_TRUE(customUserAgentFor("https://web.messenger.com/groupcall/ROOM:12345"_s).has_value());
    EXPECT_FALSE(customUserAgentFor("https://messenger.com.example.com/groupcall/ROOM:12345"_s).has_value());
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

#if PLATFORM(COCOA)
TEST_F(QuirksTest, OnlyGoogleSearchLimitsMouseFocusableAnchorsToTheExpandablePanel)
{
    constexpr auto id = WebCore::QuirkBehaviorID::NeedsAnchorToBeMouseFocusableQuirk;

    auto search = resolveQuirksForTopURL("https://www.google.com/search"_s);
    EXPECT_TRUE(search.isBehaviorEnabled(id));
    EXPECT_EQ(elementSelectorsFor(search, id), Vector<String> { String { onExpandablePanel } });

    for (auto urlString : { "https://www.dictionary.com/"_s, "https://www.thesaurus.com/"_s }) {
        auto quirks = resolveQuirksForTopURL(urlString);
        EXPECT_TRUE(quirks.isBehaviorEnabled(id));
        EXPECT_TRUE(elementSelectorsFor(quirks, id).isEmpty());
    }
}
#endif // PLATFORM(COCOA)

TEST_F(QuirksTest, ExpandablePanelSelectorMatchesThePanelAndAnythingInsideIt)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div data-expc><span><a id='inside' href='#'>x</a></span></div><a id='panel' data-expc href='#'>y</a><a id='outside' href='#'>z</a>"_s);

    EXPECT_TRUE(matchesSelector(onExpandablePanel, page.getElementById("inside"_s).get()));
    EXPECT_TRUE(matchesSelector(onExpandablePanel, page.getElementById("panel"_s).get()));
    EXPECT_FALSE(matchesSelector(onExpandablePanel, page.getElementById("outside"_s).get()));
}

#if PLATFORM(IOS_FAMILY)
TEST_F(QuirksTest, ClaudeSidebarQuirkCarriesTheSidebarSelector)
{
    auto quirks = resolveQuirksForTopURL("https://claude.ai/"_s);
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::NeedsClaudeSidebarViewportUnitQuirk), Vector<String> { String { onClaudeSidebar } });
}

TEST_F(QuirksTest, OnlyGoogleDocsOpensAboutURLsAsAboutBlank)
{
    EXPECT_TRUE(resolveQuirksForTopURL("https://docs.google.com/"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldOpenAsAboutBlankQuirk));
    EXPECT_FALSE(resolveQuirksForTopURL("https://www.google.com/"_s).isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldOpenAsAboutBlankQuirk));
}
#endif // PLATFORM(IOS_FAMILY)

TEST_F(QuirksTest, ClaudeSidebarSelectorMatchesTheAriaLabelExactly)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><nav id='sidebar' aria-label='Sidebar'></nav><nav id='lowercase' aria-label='sidebar'></nav><nav id='plain'></nav>"_s);

    EXPECT_TRUE(matchesSelector(onClaudeSidebar, page.getElementById("sidebar"_s).get()));
    EXPECT_FALSE(matchesSelector(onClaudeSidebar, page.getElementById("lowercase"_s).get()));
    EXPECT_FALSE(matchesSelector(onClaudeSidebar, page.getElementById("plain"_s).get()));
}

TEST_F(QuirksTest, TikTokCarriesSeparateSelectorsForTheCommentsAndVideoContainers)
{
    auto quirks = resolveQuirksForTopURL("https://www.tiktok.com/"_s);
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::NeedsTikTokCommentsOverflowingContentQuirk), Vector<String> { String { onTikTokCommentsContainer } });
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::NeedsTikTokVideoOverflowingContentQuirk), Vector<String> { String { onTikTokVideoContainer } });
}

TEST_F(QuirksTest, TikTokSelectorsMatchClassNameSubstringsUnderTheBrowserModeContainer)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div class='css-1-DivBrowserModeContainer'><div id='comments' class='a css-2-DivContentContainer'></div><div id='video' class='css-3-DivVideoContainer b'></div></div>"
        "<div><div id='orphan' class='css-2-DivContentContainer'></div></div>"_s);

    RefPtr comments = page.getElementById("comments"_s);
    RefPtr video = page.getElementById("video"_s);
    RefPtr orphan = page.getElementById("orphan"_s);

    EXPECT_TRUE(matchesSelector(onTikTokCommentsContainer, comments.get()));
    EXPECT_FALSE(matchesSelector(onTikTokVideoContainer, comments.get()));
    EXPECT_TRUE(matchesSelector(onTikTokVideoContainer, video.get()));
    EXPECT_FALSE(matchesSelector(onTikTokCommentsContainer, video.get()));
    EXPECT_FALSE(matchesSelector(onTikTokCommentsContainer, orphan.get()));
}

#if ENABLE(TOUCH_EVENTS)
TEST_F(QuirksTest, EachSiteThatPreventsTouchDispatchCarriesOnlyItsOwnSelector)
{
    constexpr auto touchEnd = WebCore::QuirkBehaviorID::ShouldPreventTouchEndDispatchQuirk;
    constexpr auto touchMove = WebCore::QuirkBehaviorID::ShouldPreventTouchMoveDispatchQuirk;

    auto sites = resolveQuirksForTopURL("https://sites.google.com/"_s);
    EXPECT_EQ(elementSelectorsFor(sites, touchEnd), Vector<String> { String { onGoogleSitesButton } });
    EXPECT_FALSE(sites.isBehaviorEnabled(touchMove));

    auto yahoo = resolveQuirksForTopURL("https://www.yahoo.com/"_s);
    EXPECT_EQ(elementSelectorsFor(yahoo, touchEnd), Vector<String> { String { onYahooButton } });
    EXPECT_FALSE(yahoo.isBehaviorEnabled(touchMove));

    auto outlook = resolveQuirksForTopURL("https://outlook.live.com/"_s);
    EXPECT_EQ(elementSelectorsFor(outlook, touchMove), Vector<String> { String { onOutlookSuggestions } });
    EXPECT_FALSE(outlook.isBehaviorEnabled(touchEnd));
}
#endif // ENABLE(TOUCH_EVENTS)

TEST_F(QuirksTest, TouchDispatchSelectorsRequireEveryClassInThePair)
{
    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html>"
        "<div id='button' class='DPvwYc sm8sCf'></div><div id='half' class='DPvwYc'></div>"
        "<div id='captions' class='vjs-menu-button vjs-subs-cap-button'></div>"
        "<div class='ms-Suggestions'><div><span id='suggestion'>x</span></div></div><div id='elsewhere'></div>"_s);

    EXPECT_TRUE(matchesSelector(onGoogleSitesButton, page.getElementById("button"_s).get()));
    EXPECT_FALSE(matchesSelector(onGoogleSitesButton, page.getElementById("half"_s).get()));
    EXPECT_FALSE(matchesSelector(onGoogleSitesButton, page.getElementById("captions"_s).get()));
    EXPECT_TRUE(matchesSelector(onYahooButton, page.getElementById("captions"_s).get()));

    EXPECT_TRUE(matchesSelector(onOutlookSuggestions, page.getElementById("suggestion"_s).get()));
    EXPECT_FALSE(matchesSelector(onOutlookSuggestions, page.getElementById("elsewhere"_s).get()));
}

TEST_F(QuirksTest, InstagramReelsQuirkOnlyAppliesToElementsContainingAVideo)
{
    auto quirks = resolveQuirksForTopURL("https://www.instagram.com/"_s);
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::NeedsInstagramResizingReelsQuirk), Vector<String> { String { onElementContainingVideo } });

    auto page = TestPageHarness::create();
    page.loadHTML("<!DOCTYPE html><div id='reel'><div><video></video></div></div><div id='empty'><img></div>"_s);

    EXPECT_TRUE(matchesSelector(onElementContainingVideo, page.getElementById("reel"_s).get()));
    EXPECT_FALSE(matchesSelector(onElementContainingVideo, page.getElementById("empty"_s).get()));
}

#if ENABLE(VIDEO)
struct ReelWidths {
    int withVideo;
    int withoutVideo;
};

static ReelWidths reelWidthsForTopURL(ASCIILiteral topURLString)
{
    auto page = TestPageHarness::create({ .configureSettings = [](auto& settings) {
        settings.setNeedsSiteSpecificQuirks(true);
    } });
    page.loadHTML("<!DOCTYPE html><style>"
        "body { margin: 0 } .container { display: flex; width: 50% } .item { overflow: hidden }"
        "video, .placeholder { display: block; width: 100px; height: 50px }"
        "</style>"
        "<div class='container'><div id='reel' class='item'><video></video></div></div>"
        "<div class='container'><div id='empty' class='item'><div class='placeholder'></div></div></div>"_s);

    Ref document = page.document();
    document->quirks().setTopDocumentURLForTesting(URL { topURLString });
    document->scheduleFullStyleRebuild();

    return { page.getElementById("reel"_s)->offsetWidth(), page.getElementById("empty"_s)->offsetWidth() };
}

TEST_F(QuirksTest, InstagramReelsGrowToFillTheirFlexContainerOnlyWhenTheyContainAVideo)
{
    auto instagram = reelWidthsForTopURL("https://www.instagram.com/"_s);
    EXPECT_EQ(instagram.withVideo, 400);
    EXPECT_EQ(instagram.withoutVideo, 100);

    auto elsewhere = reelWidthsForTopURL("https://www.example.com/"_s);
    EXPECT_EQ(elsewhere.withVideo, 100);
    EXPECT_EQ(elsewhere.withoutVideo, 100);
}
#endif // ENABLE(VIDEO)

} // namespace TestWebKitAPI
