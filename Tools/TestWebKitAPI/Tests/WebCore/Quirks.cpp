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

#include <WebCore/NodeInlines.h>
#include <WebCore/QuirkTable.h>
#include <WebCore/Quirks.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <array>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
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

        if (behavior.parameters->script.length() && (!behavior.parameters->scriptURLCondition || behavior.parameters->scriptURLCondition->matches(scriptURLContext)))
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
    static constexpr auto firstScriptURL = WebCore::URLMatch::host("first.example.com"_s);
    static constexpr auto secondScriptURL = WebCore::URLMatch::host("second.example.com"_s);

    static constexpr auto behaviors = WTF::toArray({
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("firstScript"_s, firstScriptURL)),
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("secondScript"_s, secondScriptURL)),
    });

    WebCore::QuirksData quirks;
    for (const auto& behavior : behaviors)
        quirks.addBehavior(behavior);

    EXPECT_EQ(scriptsForScriptURL(quirks, "https://first.example.com/a.js"_s), Vector<String> { "firstScript"_str });
    EXPECT_EQ(scriptsForScriptURL(quirks, "https://second.example.com/b.js"_s), Vector<String> { "secondScript"_str });

    EXPECT_TRUE(scriptsForScriptURL(quirks, "https://third.example.com/c.js"_s).isEmpty());
}

TEST_F(QuirksTest, EveryMatchingRowContributesWhenSeveralSupplyTheSameBehavior)
{
    static constexpr auto anyScriptURL = WebCore::URLMatch::anyURL();

    static constexpr auto behaviors = WTF::toArray({
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("firstScript"_s, anyScriptURL)),
        WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("secondScript"_s, anyScriptURL)),
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

TEST_F(QuirksTest, AScriptURLConditionTravelsWithTheParametersItScopes)
{
    static constexpr auto scriptURL = WebCore::URLMatch::host("cdn.example.com"_s);
    static constexpr auto behavior = WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("script"_s, scriptURL));

    ASSERT_TRUE(behavior.parameters.has_value());
    ASSERT_TRUE(behavior.parameters->scriptURLCondition.has_value());
    EXPECT_TRUE(behavior.parameters->scriptURLCondition->matches(WebCore::URLMatchContext { URL { "https://cdn.example.com/a.js"_s } }));
    EXPECT_FALSE(behavior.parameters->scriptURLCondition->matches(WebCore::URLMatchContext { URL { "https://other.example.com/a.js"_s } }));

    static constexpr auto unscoped = WebCore::QuirkBehaviors::needsScriptToEvaluateBeforeRunningScriptFromURLQuirk(WebCore::QuirkParameters::fromScript("script"_s));
    ASSERT_TRUE(unscoped.parameters.has_value());
    EXPECT_FALSE(unscoped.parameters->scriptURLCondition.has_value());
}

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
    quirks.addSite(WebCore::QuirkSite::Amazon);

    WebCore::QuirksData other;
    other.addBehavior(onSecond);
    other.addBehavior(WebCore::QuirkBehaviors::needsAirIndiaExpressLayeringQuirk);
    other.addSite(WebCore::QuirkSite::BankOfAmerica);

    quirks.merge(other);

    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk));
    EXPECT_TRUE(quirks.isBehaviorEnabled(WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk));
    EXPECT_TRUE(quirks.isSite(WebCore::QuirkSite::Amazon));
    EXPECT_TRUE(quirks.isSite(WebCore::QuirkSite::BankOfAmerica));
    EXPECT_EQ(elementSelectorsFor(quirks, WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk), (Vector<String> { ".first"_str, ".second"_str }));
}

static constexpr auto onSliderRole = "[role=slider], [role=slider] *"_s;
static constexpr auto onDockPanelTabBar = ".lm-DockPanel-tabBar, .lm-DockPanel-tabBar *"_s;

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
