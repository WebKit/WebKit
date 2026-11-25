/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLICATION_MANIFEST)

#include "Test.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ApplicationManifestParser.h>
#include <wtf/RunLoop.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebCore {
static inline std::ostream& operator<<(std::ostream& os, const ApplicationManifest::Direction& direction)
{
    using enum ApplicationManifest::Direction;

    switch (direction) {
    case Auto:
        return os << "ApplicationManifest::Direction::Auto";
    case LTR:
        return os << "ApplicationManifest::Direction::LTR";
    case RTL:
        return os << "ApplicationManifest::Direction::RTL";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline std::ostream& operator<<(std::ostream& os, const ApplicationManifest::Display& display)
{
    switch (display) {
    case ApplicationManifest::Display::Browser:
        return os << "ApplicationManifest::Display::Browser";
    case ApplicationManifest::Display::MinimalUI:
        return os << "ApplicationManifest::Display::MinimalUI";
    case ApplicationManifest::Display::Standalone:
        return os << "ApplicationManifest::Display::Standalone";
    case ApplicationManifest::Display::Fullscreen:
        return os << "ApplicationManifest::Display::Fullscreen";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline std::ostream& operator<<(std::ostream& os, const ScreenOrientationLockType& orientation)
{
    switch (orientation) {
    case WebCore::ScreenOrientationLockType::Any:
        return os << "WebCore::ScreenOrientationLockType::Any";
    case WebCore::ScreenOrientationLockType::Landscape:
        return os << "WebCore::ScreenOrientationLockType::Landscape";
    case WebCore::ScreenOrientationLockType::LandscapePrimary:
        return os << "WebCore::ScreenOrientationLockType::LandscapePrimary";
    case WebCore::ScreenOrientationLockType::LandscapeSecondary:
        return os << "WebCore::ScreenOrientationLockType::LandscapeSecondary";
    case WebCore::ScreenOrientationLockType::Natural:
        return os << "WebCore::ScreenOrientationLockType::Natural";
    case WebCore::ScreenOrientationLockType::Portrait:
        return os << "WebCore::ScreenOrientationLockType::Portrait";
    case WebCore::ScreenOrientationLockType::PortraitPrimary:
        return os << "WebCore::ScreenOrientationLockType::PortraitPrimary";
    case WebCore::ScreenOrientationLockType::PortraitSecondary:
        return os << "WebCore::ScreenOrientationLockType::PortraitSecondary";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

class ApplicationManifestParserTest : public testing::Test {
public:
    URL m_manifestURL;
    URL m_documentURL;
    URL m_startURL;

    virtual void SetUp()
    {
        JSC::initialize();
        WTF::initializeMainThread();

        m_manifestURL = URL { "https://example.com/manifest.json"_si };
        m_documentURL = URL { "https://example.com/"_si };
    }

    ApplicationManifest parseString(StringImpl* data)
    {
        return ApplicationManifestParser::parse(data, m_manifestURL, m_documentURL);
    }

    ApplicationManifest parseTopLevelProperty(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \""_s, key, "\" : "_s, value, " }"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseIconFirstTopLevelProperty(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"icons\": [{\""_s, key, "\": "_s, value, ", \"src\": \"icon/example.png\" }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseIconFirstTopLevelPropertyForSrc(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"icons\": [{\""_s, key, "\": "_s, value, " }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutFirstTopLevelProperty(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\""_s, key, "\": "_s, value, ", \"url\": \"example\" }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutFirstTopLevelPropertyForURL(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\""_s, key, "\": "_s, value, " }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutIconFirstTopLevelProperty(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\"url\": \"example\", \"icons\": [{\""_s, key, "\": "_s, value, ", \"src\": \"icon/example.png\" }]}]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutIconFirstTopLevelPropertyForSrc(StringImpl* key, StringImpl* value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\"url\": \"example\", \"icons\": [{\""_s, key, "\": "_s, value, " }]}]}"_s);
        return parseString(manifestContent);
    }

    void testRawJSON(StringImpl* rawJSON, bool isValidJSON)
    {
        auto manifest = parseString(rawJSON);
        auto value = manifest.rawJSON;
        if (isValidJSON)
            EXPECT_EQ(rawJSON, value);
        else
            ASSERT_EQ(rawJSON, value);
    }

    void testManifestURL(StringImpl* expectedValue)
    {
        testManifestURL(URL { expectedValue });
    }

    void testManifestURL(const URL& expectedValue)
    {
        auto manifest = ApplicationManifestParser::parse("{ \"name\": \"Example\" }"_si, expectedValue, m_documentURL);
        auto value = manifest.manifestURL;
        EXPECT_EQ(expectedValue.string(), value.string());
    }

    void testStartURL(StringImpl* rawJSON, StringImpl* expectedValue)
    {
        testStartURL(rawJSON, { { }, expectedValue });
    }

    void testStartURL(StringImpl* rawJSON, const URL& expectedValue)
    {
        auto manifest = parseTopLevelProperty("start_url"_si, rawJSON);
        auto value = manifest.startURL;
        EXPECT_EQ(expectedValue.string(), value.string());
    }

    void testDir(StringImpl* rawJSON, ApplicationManifest::Direction expectedValue)
    {
        EXPECT_EQ(expectedValue, parseTopLevelProperty("dir"_si, rawJSON).dir);
    }

    void testDisplay(StringImpl* rawJSON, ApplicationManifest::Display expectedValue)
    {
        auto manifest = parseTopLevelProperty("display"_si, rawJSON);
        auto value = manifest.display;
        EXPECT_EQ(expectedValue, value);
    }

    void testOrientation(StringImpl* rawJSON, std::optional<WebCore::ScreenOrientationLockType> expectedValue)
    {
        auto manifest = parseTopLevelProperty("orientation"_si, rawJSON);
        auto value = manifest.orientation;
        EXPECT_EQ(expectedValue, value);
    }

    void testLang(StringImpl* rawJSON, StringImpl* expectedValue)
    {
        EXPECT_EQ(expectedValue, parseTopLevelProperty("lang"_si, rawJSON).lang);
    }

    void testName(StringImpl* rawJSON, StringImpl* expectedValue)
    {
        auto manifest = parseTopLevelProperty("name"_si, rawJSON);
        auto value = manifest.name;
        EXPECT_EQ(expectedValue, value);
    }

    void testDescription(StringImpl* rawJSON, StringImpl* expectedValue)
    {
        auto manifest = parseTopLevelProperty("description"_si, rawJSON);
        auto value = manifest.description;
        EXPECT_EQ(expectedValue, value);
    }

    void testShortName(StringImpl* rawJSON, StringImpl* expectedValue)
    {
        auto manifest = parseTopLevelProperty("short_name"_si, rawJSON);
        auto value = manifest.shortName;
        EXPECT_EQ(expectedValue, value);
    }

    void testScope(StringImpl* rawJSON, StringImpl* startURL, StringImpl* expectedValue, bool expectedIsDefaultScope)
    {
        auto manifestContent = makeString("{ \"scope\" : "_s, rawJSON, ", \"start_url\" : \""_s, startURL, "\" }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.scope;
        EXPECT_EQ(expectedValue, value.string());
        EXPECT_EQ(expectedIsDefaultScope, manifest.isDefaultScope);
    }

    void testScope(StringImpl* rawJSON, StringImpl* expectedValue, bool expectedIsDefaultScope)
    {
        testScope(rawJSON, m_startURL.string(), expectedValue, expectedIsDefaultScope);
    }

    void testBackgroundColor(StringImpl* rawJSON, const Color& expectedValue)
    {
        auto manifest = parseTopLevelProperty("background_color"_si, rawJSON);
        auto value = manifest.backgroundColor;
        EXPECT_EQ(expectedValue, value);
    }

    void testBackgroundColorDark(StringImpl* rawJSON, const Color& expectedValue, bool nested = true)
    {
        auto manifest = parseTopLevelProperty("color_scheme_dark"_si, nested ? makeString("{ \"background_color\" : "_s, rawJSON, " }"_s) : RefPtr { rawJSON });
        auto value = manifest.backgroundColorDark;
        EXPECT_EQ(expectedValue, value);
    }

    void testThemeColor(StringImpl* rawJSON, const Color& expectedValue)
    {
        auto manifest = parseTopLevelProperty("theme_color"_si, rawJSON);
        auto value = manifest.themeColor;
        EXPECT_EQ(expectedValue, value);
    }

    void testThemeColorDark(StringImpl* rawJSON, const Color& expectedValue, bool nested = true)
    {
        auto manifest = parseTopLevelProperty("color_scheme_dark"_si, nested ? makeString("{ \"theme_color\" : "_s, rawJSON, " }"_s) : RefPtr { rawJSON });
        auto value = manifest.themeColorDark;
        EXPECT_EQ(expectedValue, value);
    }

    void testCategories(const Vector<RefPtr<StringImpl>>& expectedValue)
    {
        StringBuilder builder;
        builder.append('[');
        for (auto& value : expectedValue) {
            if (builder.length() > 1)
                builder.append(", "_s);

            builder.append('\"');
            builder.append(value);
            builder.append('\"');
        }
        builder.append(']');
        auto categoriesValue = builder.toString();
        RefPtr<StringImpl> manifestContent = makeString("{ \"categories\" : "_s, categoriesValue, " }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.categories;
        EXPECT_EQ(expectedValue, value);
    }

    void testIconsSrc(StringImpl* rawJSON, const URL& expectedValue)
    {
        auto manifest = parseIconFirstTopLevelPropertyForSrc("src"_si, rawJSON);
        auto value = manifest.icons[0].src;
        EXPECT_EQ(expectedValue.string(), value.string());

        auto shortcutManifest = parseShortcutIconFirstTopLevelPropertyForSrc("src"_si, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].src;
        EXPECT_EQ(expectedValue.string(), shortcutValue.string());
    }

    void testIconsType(StringImpl*rawJSON, StringImpl* expectedValue)
    {
        auto manifest = parseIconFirstTopLevelProperty("type"_si, rawJSON);
        auto value = manifest.icons[0].type;
        EXPECT_EQ(expectedValue, value);

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("type"_si, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].type;
        EXPECT_EQ(expectedValue, shortcutValue);
    }

    void testIconsSizes(StringImpl*rawJSON, size_t expectedCount, size_t testIndex, StringImpl* expectedValue)
    {
        auto manifest = parseIconFirstTopLevelProperty("sizes"_si, rawJSON);
        auto value = manifest.icons[0].sizes;
        EXPECT_EQ(expectedCount, value.size());
        EXPECT_TRUE(testIndex < value.size());
        EXPECT_EQ(expectedValue, value[testIndex]);

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("sizes"_si, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].sizes;
        EXPECT_EQ(expectedCount, shortcutValue.size());
        EXPECT_TRUE(testIndex < shortcutValue.size());
        EXPECT_EQ(expectedValue, shortcutValue[testIndex]);
    }

    void testIconsPurposes(StringImpl*rawJSON, OptionSet<ApplicationManifest::Icon::Purpose> expectedValues)
    {
        auto manifest = parseIconFirstTopLevelProperty("purpose"_si, rawJSON);
        auto value = manifest.icons[0].purposes;
        EXPECT_EQ(expectedValues, value);

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("purpose"_si, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].purposes;
        EXPECT_EQ(expectedValues, shortcutValue);
    }

    void testShortcutsURL(StringImpl* rawJSON, const URL& expectedValue)
    {
        auto manifest = parseShortcutFirstTopLevelPropertyForURL("url"_si, rawJSON);
        auto value = manifest.shortcuts[0].url;
        EXPECT_EQ(expectedValue.string(), value.string());
    }

    void testShortcutsName(StringImpl*rawJSON, StringImpl* expectedValue)
    {
        auto manifest = parseShortcutFirstTopLevelProperty("name"_si, rawJSON);
        auto value = manifest.shortcuts[0].name;
        EXPECT_EQ(expectedValue, value);
    }

    void testId(StringImpl* rawJSON, const URL& expectedValue)
    {
        auto manifest = parseTopLevelProperty("id"_si, rawJSON);
        auto value = manifest.id;
        EXPECT_EQ(expectedValue.string(), value.string());
    }

    void testId(StringImpl* rawJSON, const URL& startURL, StringImpl* expectedValue)
    {
        auto manifestContent = makeString("{ \"id\" : \""_s, rawJSON, "\", \"start_url\" : \""_s, startURL.string(), "\" }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.id;
        EXPECT_EQ(expectedValue, value.string());
    }
};

static void assertManifestHasDefaultValues(const URL& manifestURL, const URL& documentURL, const ApplicationManifest& manifest)
{
    EXPECT_TRUE(!manifest.name);
    EXPECT_TRUE(!manifest.shortName);
    EXPECT_TRUE(!manifest.description);
    EXPECT_EQ("https://example.com/"_s, manifest.scope.string());
    EXPECT_EQ(documentURL.string(), manifest.startURL.string());
    EXPECT_EQ(manifest.id.string(), manifest.startURL.string());
}

TEST_F(ApplicationManifestParserTest, DefaultManifest)
{
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(nullString()));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(""_si));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("{ }"_si));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("This is 100% not JSON."_si));
}

TEST_F(ApplicationManifestParserTest, RawJSON)
{
    testRawJSON("{ \"name\" : \"Example\", \"start_url\" : \"https://example.com\"}"_si, true);
    testRawJSON("{ \"start_url\" : \"https://example.com\"}"_si, true);
    testRawJSON("This is 100% not JSON."_si, false);
}

TEST_F(ApplicationManifestParserTest, Id)
{
    m_documentURL = URL { "https://example.com/home"_si };
    m_manifestURL = URL { "https://example.com/manifest.json"_si };

    testId("123"_si, m_documentURL);
    testId("null"_si, m_documentURL);
    testId("true"_si, m_documentURL);
    testId("{ }"_si, m_documentURL);
    testId("[ ]"_si, m_documentURL);
    testId("[ \"http://example.com/somepage\" ]"_si, m_documentURL);
    testId("\"\""_si, m_documentURL);
    testId("\"http:?\""_si, m_documentURL);

    testId("\"https://other-domain.com\""_si, m_documentURL);
    testId("\"https://invalid.com:a\""_si, m_documentURL);

    m_startURL = URL { "https://example.com/my-app/start?query=q#fragment"_si };
    testId(""_si, m_startURL , m_startURL.string());
    testId("/"_si, m_startURL, "https://example.com/"_si);
    testId("foo"_si, m_startURL, "https://example.com/foo"_si);
    testId("./foo"_si, m_startURL, "https://example.com/foo"_si);
    testId("foo/"_si, m_startURL, "https://example.com/foo/"_si);
    testId("../../foo/bar"_si, m_startURL, "https://example.com/foo/bar"_si);
    testId("../../foo/bar?query=hi#fragment"_si, m_startURL, "https://example.com/foo/bar?query=hi"_si);
    testId("../../foo/bar?query=hi#"_si, m_startURL, "https://example.com/foo/bar?query=hi"_si);

    testId("https://example.com/foo"_si, m_startURL, "https://example.com/foo"_si);
    testId("https://example.com/foo#"_si, m_startURL, "https://example.com/foo"_si);
    testId("https://example.com/foo#fragment"_si, m_startURL, "https://example.com/foo"_si);
    testId("https://anothersite.com/foo"_si, m_startURL, m_startURL.string());
    testId("https://invalid.com:a"_si, m_startURL, m_startURL.string());
}

TEST_F(ApplicationManifestParserTest, ManifestURL)
{
    m_documentURL = URL { "https://example.com/home"_si };

    testManifestURL("https://example.com/manifest.json"_si);
    testManifestURL("https://example.com/test/manifest.json"_si);
}

TEST_F(ApplicationManifestParserTest, StartURL)
{
    m_documentURL = URL { "https://example.com/home"_si };
    m_manifestURL = URL { "https://example.com/manifest.json"_si };

    testStartURL("123"_si, m_documentURL);
    testStartURL("null"_si, m_documentURL);
    testStartURL("true"_si, m_documentURL);
    testStartURL("{ }"_si, m_documentURL);
    testStartURL("[ ]"_si, m_documentURL);
    testStartURL("[ \"http://example.com/somepage\" ]"_si, m_documentURL);
    testStartURL("\"\""_si, m_documentURL);
    testStartURL("\"http:?\""_si, m_documentURL);

    testStartURL("\"appstartpage\""_si, "https://example.com/appstartpage"_si);
    testStartURL("\"a/b/cdefg\""_si, "https://example.com/a/b/cdefg"_si);

    m_documentURL = URL { "https://example.com/subfolder/home"_si };
    m_manifestURL = URL { "https://example.com/resources/manifest.json"_si };

    testStartURL("\"resource-relative-to-manifest-url\""_si, "https://example.com/resources/resource-relative-to-manifest-url"_si);
    testStartURL("\"http://different-page.com/12/34\""_si, m_documentURL);

    m_documentURL = URL { "https://example.com/home"_si };
    m_manifestURL = URL { "https://other-domain.com/manifiest.json"_si };

    testStartURL("\"resource_on_other_domain\""_si, m_documentURL);
    testStartURL("\"http://example.com/scheme-does-not-match-document\""_si, m_documentURL);
    testStartURL("\"https://example.com:123/port-does-not-match-document"_si, m_documentURL);
    testStartURL("\"https://example.com/page2\""_si, "https://example.com/page2"_si);
    testStartURL("\"//example.com/page2\""_si, "https://example.com/page2"_si);

    m_documentURL = URL { "https://example.com/a"_si };
    m_manifestURL = URL { "https://example.com/z/manifest.json"_si };

    testStartURL("\"b/c\""_si, "https://example.com/z/b/c"_si);
    testStartURL("\"/b/c\""_si, "https://example.com/b/c"_si);
    testStartURL("\"?query\""_si, "https://example.com/z/manifest.json?query"_si);

    m_documentURL = URL { "https://example.com/dir1/dir2/page1"_si };
    m_manifestURL = URL { "https://example.com/dir3/manifest.json"_si };

    testStartURL("\"../page2\""_si, "https://example.com/page2"_si);
}

TEST_F(ApplicationManifestParserTest, Dir)
{
    using enum ApplicationManifest::Direction;

    testDir("123"_si, Auto);
    testDir("null"_si, Auto);
    testDir("true"_si, Auto);
    testDir("{ }"_si, Auto);
    testDir("[ ]"_si, Auto);
    testDir("\"\""_si, Auto);
    testDir("\"garbage string\""_si, Auto);
    testDir("\"\vltr\""_si, Auto);

    testDir("\"auto\""_si, Auto);
    testDir("\"ltr\""_si, LTR);
    testDir("\"rtl\""_si, RTL);
    testDir("\"\t\nLTR \""_si, LTR);
}

TEST_F(ApplicationManifestParserTest, Display)
{
    testDisplay("123"_si, ApplicationManifest::Display::Browser);
    testDisplay("null"_si, ApplicationManifest::Display::Browser);
    testDisplay("true"_si, ApplicationManifest::Display::Browser);
    testDisplay("{ }"_si, ApplicationManifest::Display::Browser);
    testDisplay("[ ]"_si, ApplicationManifest::Display::Browser);
    testDisplay("\"\""_si, ApplicationManifest::Display::Browser);
    testDisplay("\"garbage string\""_si, ApplicationManifest::Display::Browser);

    testDisplay("\"browser\""_si, ApplicationManifest::Display::Browser);
    testDisplay("\"standalone\""_si, ApplicationManifest::Display::Standalone);
    testDisplay("\"minimal-ui\""_si, ApplicationManifest::Display::MinimalUI);
    testDisplay("\"fullscreen\""_si, ApplicationManifest::Display::Fullscreen);
    testDisplay("\"\t\nMINIMAL-UI \""_si, ApplicationManifest::Display::MinimalUI);
    testDisplay("\"\vbminimal-ui\""_si, ApplicationManifest::Display::Browser);
}

TEST_F(ApplicationManifestParserTest, Orientation)
{
    testOrientation(""_si, std::nullopt);
    testOrientation("123"_si, std::nullopt);
    testOrientation("null"_si, std::nullopt);
    testOrientation("true"_si, std::nullopt);
    testOrientation("{ }"_si, std::nullopt);
    testOrientation("[ ]"_si, std::nullopt);
    testOrientation("\"\""_si, std::nullopt);
    testOrientation("\"garbage string\""_si, std::nullopt);
    testOrientation("\"\vbportrait-secondary\""_si, std::nullopt);

    testOrientation("\"any\""_si, WebCore::ScreenOrientationLockType::Any);
    testOrientation("\"natural\""_si, WebCore::ScreenOrientationLockType::Natural);
    testOrientation("\"landscape\""_si, WebCore::ScreenOrientationLockType::Landscape);
    testOrientation("\"landscape-primary\""_si, WebCore::ScreenOrientationLockType::LandscapePrimary);
    testOrientation("\"landscape-secondary\""_si, WebCore::ScreenOrientationLockType::LandscapeSecondary);
    testOrientation("\"portrait\""_si, WebCore::ScreenOrientationLockType::Portrait);
    testOrientation("\"portrait-primary\""_si, WebCore::ScreenOrientationLockType::PortraitPrimary);
    testOrientation("\"portrait-secondary\""_si, WebCore::ScreenOrientationLockType::PortraitSecondary);
}

TEST_F(ApplicationManifestParserTest, Lang)
{
    testLang("123"_si, nullString());
    testLang("null"_si, nullString());
    testLang("true"_si, nullString());
    testLang("{ }"_si, nullString());
    testLang("[ ]"_si, nullString());
    testLang("\"\""_si, nullString());

    // Invalid language tags should be ignored.
    testLang("\"invalid-language-tag\""_si, nullString());

    testLang("\"en\""_si, "en"_si);
    testLang("\" en \""_si, "en"_si);
    testLang("\"en-AU\""_si, "en-AU"_si);
    testLang("\"zh-Hans-CN\""_si, "zh-Hans-CN"_si);
    testLang("\"en-x-custom\""_si, "en-x-custom"_si);

    // Language tags should be canonicalized.
    testLang("\"DE-DE\""_si, "de-DE"_si);
    testLang("\" DE-DE \""_si, "de-DE"_si);

    // ISO-8859-1 characters.
    testLang(StringImpl::fromUTF8("\"en-Latn-US-àéîöü\""), nullString());
    testLang(StringImpl::fromUTF8("\"frçñch\""), nullString());

    // Unicode characters beyond ISO-8859-1.
    testLang(StringImpl::fromUTF8("\"zh-中文\""), nullString());
    testLang(StringImpl::fromUTF8("\"ja-日本語\""), nullString());
    testLang(StringImpl::fromUTF8("\"ko-한국어\""), nullString());
    testLang(StringImpl::fromUTF8("\"ar-العربية\""), nullString());
}

TEST_F(ApplicationManifestParserTest, Name)
{
    testName("123"_si, nullString());
    testName("null"_si, nullString());
    testName("true"_si, nullString());
    testName("{ }"_si, nullString());
    testName("[ ]"_si, nullString());
    testName("\"\""_si, emptyString());
    testName("\"example\""_si, "example"_si);
    testName("\"\\t Hello\\nWorld\\t \""_si, "Hello\nWorld"_si);
}

TEST_F(ApplicationManifestParserTest, Description)
{
    testDescription("123"_si, nullString());
    testDescription("null"_si, nullString());
    testDescription("true"_si, nullString());
    testDescription("{ }"_si, nullString());
    testDescription("[ ]"_si, nullString());
    testDescription("\"\""_si, emptyString());
    testDescription("\"example\""_si, "example"_si);
    testDescription("\"\\t Hello\\nWorld\\t \""_si, "Hello\nWorld"_si);
}

TEST_F(ApplicationManifestParserTest, ShortName)
{
    testShortName("123"_si, nullString());
    testShortName("null"_si, nullString());
    testShortName("true"_si, nullString());
    testShortName("{ }"_si, nullString());
    testShortName("[ ]"_si, nullString());
    testShortName("\"\""_si, ""_si);
    testShortName("\"example\""_si, "example"_si);
    testShortName("\"\\t Hello\\nWorld\\t \""_si, "Hello\nWorld"_si);
}

TEST_F(ApplicationManifestParserTest, Scope)
{
    // If the scope is not a string or not a valid URL, return the default scope (the parent path of the start URL).
    m_documentURL = URL { "https://example.com/a/page?queryParam=value#fragment"_si };
    m_startURL = URL { "https://example.com/a/page?queryParam=value#fragment"_si };
    m_manifestURL = URL { "https://example.com/manifest.json"_si };
    testScope("123"_si, "https://example.com/a/"_si, true);
    testScope("null"_si, "https://example.com/a/"_si, true);
    testScope("true"_si, "https://example.com/a/"_si, true);
    testScope("{ }"_si, "https://example.com/a/"_si, true);
    testScope("[ ]"_si, "https://example.com/a/"_si, true);
    testScope("\"\""_si, "https://example.com/a/"_si, true);
    testScope("\"http:?\""_si, "https://example.com/a/"_si, true);

    m_documentURL = URL { "https://example.com/a/pageEndingWithSlash/"_si };
    m_startURL = URL { "https://example.com/a/pageEndingWithSlash/"_si };
    testScope("null"_si, "https://example.com/a/pageEndingWithSlash/"_si, true);

    // If scope URL is not same origin as document URL, return the default scope.
    m_documentURL = URL { "https://example.com/home"_si };
    m_startURL = URL { "https://example.com/home"_si };
    m_manifestURL = URL { "https://other-site.com/manifest.json"_si };
    testScope("\"https://other-site.com/some-scope\""_si, "https://example.com/"_si, true);

    m_documentURL = URL { "https://example.com/app/home"_si };
    m_startURL = URL { "https://example.com/app/home"_si };
    m_manifestURL = URL { "https://example.com/app/manifest.json"_si };

    // If start URL is not within scope of scope URL, return the default scope.
    testScope("\"https://example.com/subdirectory\""_si, "https://example.com/app/"_si, true);
    testScope("\"https://example.com/app\""_si, "https://example.com/app"_si, false);
    testScope("\"https://example.com/APP\""_si, "https://example.com/app/"_si, true);
    testScope("\"https://example.com/a\""_si, "https://example.com/a"_si, false);

    m_documentURL = URL { "https://example.com/a/b/c/index"_si };
    m_startURL = URL { "https://example.com/a/b/c/index"_si };
    m_manifestURL = URL { "https://example.com/a/manifest.json"_si };

    testScope("\"./b/c/index\""_si, "https://example.com/a/b/c/index"_si, false);
    testScope("\"b/somewhere-else/../c\""_si, "https://example.com/a/b/c"_si, false);
    testScope("\"b\""_si, "https://example.com/a/b"_si, false);
    testScope("\"b/\""_si, "https://example.com/a/b/"_si, false);

    m_documentURL = URL { "https://example.com/documents/home"_si };
    m_startURL = URL { "https://example.com/documents/home"_si };
    m_manifestURL = URL { "https://example.com/resources/manifest.json"_si };

    // Removes query
    testScope("\"https://example.com/documents/home?query\""_si, "https://example.com/documents/home"_si, false);
    testScope("\"https://example.com/documents/home?query=whatever\""_si, "https://example.com/documents/home"_si, false);

    // Removes fragment
    testScope("\"https://example.com/documents/home#\""_si, "https://example.com/documents/home"_si, false);
    testScope("\"https://example.com/documents/home#fragment\""_si, "https://example.com/documents/home"_si, false);

    // Removes query and fragment
    testScope("\"https://example.com/documents/home?#\""_si, "https://example.com/documents/home"_si, false);
    testScope("\"https://example.com/documents/home?query#fragment\""_si, "https://example.com/documents/home"_si, false);

    // It's fine if the document URL or manifest URL aren't within the application scope - only the start URL needs to be.
    testScope("\"https://example.com/other\""_si, "https://example.com/other/start-url"_si, "https://example.com/other"_si, false);
}

TEST_F(ApplicationManifestParserTest, BackgroundColor)
{
    testBackgroundColor("123"_si, Color());
    testBackgroundColor("null"_si, Color());
    testBackgroundColor("true"_si, Color());
    testBackgroundColor("{ }"_si, Color());
    testBackgroundColor("[ ]"_si, Color());
    testBackgroundColor("\"\""_si, Color());
    testBackgroundColor("\"garbage string\""_si, Color());

    testBackgroundColor("\"red\""_si, Color::red);
    testBackgroundColor("\"#f00\""_si, Color::red);
    testBackgroundColor("\"#ff0000\""_si, Color::red);
    testBackgroundColor("\"#ff0000ff\""_si, Color::red);
    testBackgroundColor("\"rgb(255, 0, 0)\""_si, Color::red);
    testBackgroundColor("\"rgba(255, 0, 0, 1)\""_si, Color::red);
    testBackgroundColor("\"hsl(0, 100%, 50%)\""_si, Color::red);
    testBackgroundColor("\"hsla(0, 100%, 50%, 1)\""_si, Color::red);
}

TEST_F(ApplicationManifestParserTest, BackgroundColorDark)
{
    // nested inside "color_scheme_dark" object
    testBackgroundColorDark("123"_si, Color());
    testBackgroundColorDark("null"_si, Color());
    testBackgroundColorDark("true"_si, Color());
    testBackgroundColorDark("{ }"_si, Color());
    testBackgroundColorDark("[ ]"_si, Color());
    testBackgroundColorDark("\"\""_si, Color());
    testBackgroundColorDark("\"garbage string\""_si, Color());
    testBackgroundColorDark("\"red\""_si, Color::red);
    testBackgroundColorDark("\"#f00\""_si, Color::red);
    testBackgroundColorDark("\"#ff0000\""_si, Color::red);
    testBackgroundColorDark("\"#ff0000ff\""_si, Color::red);
    testBackgroundColorDark("\"rgb(255, 0, 0)\""_si, Color::red);
    testBackgroundColorDark("\"rgba(255, 0, 0, 1)\""_si, Color::red);
    testBackgroundColorDark("\"hsl(0, 100%, 50%)\""_si, Color::red);
    testBackgroundColorDark("\"hsla(0, 100%, 50%, 1)\""_si, Color::red);

    // set at top-level
    testBackgroundColorDark("123"_si, Color(), false);
    testBackgroundColorDark("null"_si, Color(), false);
    testBackgroundColorDark("true"_si, Color(), false);
    testBackgroundColorDark("{ }"_si, Color(), false);
    testBackgroundColorDark("[ ]"_si, Color(), false);
    testBackgroundColorDark("\"\""_si, Color(), false);
    testBackgroundColorDark("\"garbage string\""_si, Color(), false);
    testBackgroundColorDark("\"red\""_si, Color(), false);
    testBackgroundColorDark("\"#f00\""_si, Color(), false);
    testBackgroundColorDark("\"#ff0000\""_si, Color(), false);
    testBackgroundColorDark("\"#ff0000ff\""_si, Color(), false);
    testBackgroundColorDark("\"rgb(255, 0, 0)\""_si, Color(), false);
    testBackgroundColorDark("\"rgba(255, 0, 0, 1)\""_si, Color(), false);
    testBackgroundColorDark("\"hsl(0, 100%, 50%)\""_si, Color(), false);
    testBackgroundColorDark("\"hsla(0, 100%, 50%, 1)\""_si, Color(), false);
}

TEST_F(ApplicationManifestParserTest, ThemeColor)
{
    testThemeColor("123"_si, Color());
    testThemeColor("null"_si, Color());
    testThemeColor("true"_si, Color());
    testThemeColor("{ }"_si, Color());
    testThemeColor("[ ]"_si, Color());
    testThemeColor("\"\""_si, Color());
    testThemeColor("\"garbage string\""_si, Color());

    testThemeColor("\"red\""_si, Color::red);
    testThemeColor("\"#f00\""_si, Color::red);
    testThemeColor("\"#ff0000\""_si, Color::red);
    testThemeColor("\"#ff0000ff\""_si, Color::red);
    testThemeColor("\"rgb(255, 0, 0)\""_si, Color::red);
    testThemeColor("\"rgba(255, 0, 0, 1)\""_si, Color::red);
    testThemeColor("\"hsl(0, 100%, 50%)\""_si, Color::red);
    testThemeColor("\"hsla(0, 100%, 50%, 1)\""_si, Color::red);
}

TEST_F(ApplicationManifestParserTest, ThemeColorDark)
{
    // nested inside "color_scheme_dark" object
    testThemeColorDark("123"_si, Color());
    testThemeColorDark("null"_si, Color());
    testThemeColorDark("true"_si, Color());
    testThemeColorDark("{ }"_si, Color());
    testThemeColorDark("[ ]"_si, Color());
    testThemeColorDark("\"\""_si, Color());
    testThemeColorDark("\"garbage string\""_si, Color());
    testThemeColorDark("\"red\""_si, Color::red);
    testThemeColorDark("\"#f00\""_si, Color::red);
    testThemeColorDark("\"#ff0000\""_si, Color::red);
    testThemeColorDark("\"#ff0000ff\""_si, Color::red);
    testThemeColorDark("\"rgb(255, 0, 0)\""_si, Color::red);
    testThemeColorDark("\"rgba(255, 0, 0, 1)\""_si, Color::red);
    testThemeColorDark("\"hsl(0, 100%, 50%)\""_si, Color::red);
    testThemeColorDark("\"hsla(0, 100%, 50%, 1)\""_si, Color::red);

    // set at top-level
    testThemeColorDark("123"_si, Color(), false);
    testThemeColorDark("null"_si, Color(), false);
    testThemeColorDark("true"_si, Color(), false);
    testThemeColorDark("{ }"_si, Color(), false);
    testThemeColorDark("[ ]"_si, Color(), false);
    testThemeColorDark("\"\""_si, Color(), false);
    testThemeColorDark("\"garbage string\""_si, Color(), false);
    testThemeColorDark("\"red\""_si, Color(), false);
    testThemeColorDark("\"#f00\""_si, Color(), false);
    testThemeColorDark("\"#ff0000\""_si, Color(), false);
    testThemeColorDark("\"#ff0000ff\""_si, Color(), false);
    testThemeColorDark("\"rgb(255, 0, 0)\""_si, Color(), false);
    testThemeColorDark("\"rgba(255, 0, 0, 1)\""_si, Color(), false);
    testThemeColorDark("\"hsl(0, 100%, 50%)\""_si, Color(), false);
    testThemeColorDark("\"hsla(0, 100%, 50%, 1)\""_si, Color(), false);
}

TEST_F(ApplicationManifestParserTest, Categories)
{
    testCategories({ });
    testCategories({ "health"_s });
    testCategories({ "music"_s });
    testCategories({ "video"_s });
    testCategories({ "lifestyle"_s, "magazine"_s });
    testCategories({ "books & reference"_s, "social network"_s, "education"_s });
}

TEST_F(ApplicationManifestParserTest, Whitespace)
{
    auto manifest = parseString("  { \"name\": \"PASS\" }\n"_si);

    EXPECT_EQ("PASS"_s, manifest.name);
}

TEST_F(ApplicationManifestParserTest, Icons)
{
    URL srcURL = URL { "https://example.com/icon.jpg"_si };
    testIconsSrc("\"icon.jpg\""_si, srcURL);
    testIconsType("\"image/webp\""_si, "image/webp"_si);
    testIconsSizes("\"256x256\""_si, 1, 0, "256x256"_si);
    testIconsSizes("\"72x72 96x96\""_si, 2, 0, "72x72"_si);
    testIconsSizes("\"72x72 96x96\""_si, 2, 1, "96x96"_si);

    OptionSet<ApplicationManifest::Icon::Purpose> purposeAny { ApplicationManifest::Icon::Purpose::Any };
    OptionSet<ApplicationManifest::Icon::Purpose> purposeMonochrome { ApplicationManifest::Icon::Purpose::Monochrome };
    OptionSet<ApplicationManifest::Icon::Purpose> purposeMaskable { ApplicationManifest::Icon::Purpose::Maskable };

    testIconsPurposes("\"monochrome\""_si, purposeMonochrome);
    testIconsPurposes("\"maskable\""_si, purposeMaskable);
    testIconsPurposes("\"any\""_si, purposeAny);
    testIconsPurposes("\"\tMONOCHROME\""_si, purposeMonochrome);

    testIconsPurposes("123"_si, purposeAny);
    testIconsPurposes("null"_si, purposeAny);
    testIconsPurposes("true"_si, purposeAny);
    testIconsPurposes("{ }"_si, purposeAny);
    testIconsPurposes("[ ]"_si, purposeAny);

    OptionSet<ApplicationManifest::Icon::Purpose> purposeMonochromeAny { ApplicationManifest::Icon::Purpose::Monochrome, ApplicationManifest::Icon::Purpose::Any };

    testIconsPurposes("\"monochrome any\""_si, purposeMonochromeAny);
}

TEST_F(ApplicationManifestParserTest, Shortcuts)
{
    testShortcutsURL("\"example1\""_si, URL { "https://example.com/example1"_si });
    testShortcutsURL("\"/example2\""_si, URL { "https://example.com/example2"_si });
    testShortcutsURL("\"/example3/\""_si, URL { "https://example.com/example3/"_si });
    testShortcutsURL("\"https://example.com/example4\""_si, URL { "https://example.com/example4"_si });

    testShortcutsName("\"Example\""_si, "Example"_si);
    testShortcutsName("\" \""_si, ""_si);
    testShortcutsName("\"\""_si, ""_si);
}

#endif
