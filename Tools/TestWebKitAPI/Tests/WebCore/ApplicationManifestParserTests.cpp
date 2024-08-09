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

#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ApplicationManifestParser.h>
#include <wtf/RunLoop.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebCore {
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

        m_manifestURL = URL { "https://example.com/manifest.json"_s };
        m_documentURL = URL { "https://example.com/"_s };
    }

    ApplicationManifest parseString(const String& data)
    {
        return ApplicationManifestParser::parse(data, m_manifestURL, m_documentURL);
    }

    ApplicationManifest parseTopLevelProperty(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \""_s, key, "\" : "_s, value, " }"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseIconFirstTopLevelProperty(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"icons\": [{\""_s, key, "\": "_s, value, ", \"src\": \"icon/example.png\" }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseIconFirstTopLevelPropertyForSrc(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"icons\": [{\""_s, key, "\": "_s, value, " }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutFirstTopLevelProperty(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\""_s, key, "\": "_s, value, ", \"url\": \"example\" }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutFirstTopLevelPropertyForURL(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\""_s, key, "\": "_s, value, " }]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutIconFirstTopLevelProperty(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\"url\": \"example\", \"icons\": [{\""_s, key, "\": "_s, value, ", \"src\": \"icon/example.png\" }]}]}"_s);
        return parseString(manifestContent);
    }

    ApplicationManifest parseShortcutIconFirstTopLevelPropertyForSrc(const String& key, const String& value)
    {
        auto manifestContent = makeString("{ \"shortcuts\": [{\"url\": \"example\", \"icons\": [{\""_s, key, "\": "_s, value, " }]}]}"_s);
        return parseString(manifestContent);
    }

    void testRawJSON(const String& rawJSON, bool isValidJSON)
    {
        auto manifest = parseString(rawJSON);
        auto value = manifest.rawJSON;
        if (isValidJSON)
            EXPECT_STREQ(rawJSON.utf8().data(), value.utf8().data());
        else
            ASSERT_STREQ(rawJSON.utf8().data(), value.utf8().data());
    }

    void testManifestURL(const String& expectedValue)
    {
        testManifestURL(URL { expectedValue });
    }

    void testManifestURL(const URL& expectedValue)
    {
        auto manifest = ApplicationManifestParser::parse("{ \"name\": \"Example\" }"_s, expectedValue, m_documentURL);
        auto value = manifest.manifestURL;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());
    }

    void testStartURL(const String& rawJSON, const String& expectedValue)
    {
        testStartURL(rawJSON, { { }, expectedValue });
    }

    void testStartURL(const String& rawJSON, const URL& expectedValue)
    {
        auto manifest = parseTopLevelProperty("start_url"_s, rawJSON);
        auto value = manifest.startURL;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());
    }

    void testDisplay(const String& rawJSON, ApplicationManifest::Display expectedValue)
    {
        auto manifest = parseTopLevelProperty("display"_s, rawJSON);
        auto value = manifest.display;
        EXPECT_EQ(expectedValue, value);
    }

    void testOrientation(const String& rawJSON, std::optional<WebCore::ScreenOrientationLockType> expectedValue)
    {
        auto manifest = parseTopLevelProperty("orientation"_s, rawJSON);
        auto value = manifest.orientation;
        EXPECT_EQ(expectedValue, value);
    }

    void testName(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("name"_s, rawJSON);
        auto value = manifest.name;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testDescription(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("description"_s, rawJSON);
        auto value = manifest.description;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testShortName(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("short_name"_s, rawJSON);
        auto value = manifest.shortName;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testScope(const String& rawJSON, const String& startURL, const String& expectedValue, bool expectedIsDefaultScope)
    {
        auto manifestContent = makeString("{ \"scope\" : "_s, rawJSON, ", \"start_url\" : \""_s, startURL, "\" }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.scope;
        EXPECT_STREQ(expectedValue.utf8().data(), value.string().utf8().data());
        EXPECT_EQ(expectedIsDefaultScope, manifest.isDefaultScope);
    }

    void testScope(const String& rawJSON, const String& expectedValue, bool expectedIsDefaultScope)
    {
        testScope(rawJSON, m_startURL.string(), expectedValue, expectedIsDefaultScope);
    }

    void testBackgroundColor(const String& rawJSON, const Color& expectedValue)
    {
        auto manifest = parseTopLevelProperty("background_color"_s, rawJSON);
        auto value = manifest.backgroundColor;
        EXPECT_EQ(expectedValue, value);
    }

    void testThemeColor(const String& rawJSON, const Color& expectedValue)
    {
        auto manifest = parseTopLevelProperty("theme_color"_s, rawJSON);
        auto value = manifest.themeColor;
        EXPECT_EQ(expectedValue, value);
    }

    void testCategories(const Vector<String>& expectedValue)
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
        String manifestContent = makeString("{ \"categories\" : "_s, categoriesValue, " }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.categories;
        EXPECT_EQ(expectedValue, value);
    }

    void testIconsSrc(const String& rawJSON, const URL& expectedValue)
    {
        auto manifest = parseIconFirstTopLevelPropertyForSrc("src"_s, rawJSON);
        auto value = manifest.icons[0].src;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());

        auto shortcutManifest = parseShortcutIconFirstTopLevelPropertyForSrc("src"_s, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].src;
        EXPECT_STREQ(expectedValue.string().utf8().data(), shortcutValue.string().utf8().data());
    }

    void testIconsType(const String &rawJSON, const String& expectedValue)
    {
        auto manifest = parseIconFirstTopLevelProperty("type"_s, rawJSON);
        auto value = manifest.icons[0].type;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("type"_s, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].type;
        EXPECT_STREQ(expectedValue.utf8().data(), shortcutValue.utf8().data());
    }

    void testIconsSizes(const String &rawJSON, size_t expectedCount, size_t testIndex, const String& expectedValue)
    {
        auto manifest = parseIconFirstTopLevelProperty("sizes"_s, rawJSON);
        auto value = manifest.icons[0].sizes;
        EXPECT_EQ(expectedCount, value.size());
        EXPECT_TRUE(testIndex < value.size());
        EXPECT_STREQ(expectedValue.utf8().data(), value[testIndex].utf8().data());

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("sizes"_s, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].sizes;
        EXPECT_EQ(expectedCount, shortcutValue.size());
        EXPECT_TRUE(testIndex < shortcutValue.size());
        EXPECT_STREQ(expectedValue.utf8().data(), shortcutValue[testIndex].utf8().data());
    }

    void testIconsPurposes(const String &rawJSON, OptionSet<ApplicationManifest::Icon::Purpose> expectedValues)
    {
        auto manifest = parseIconFirstTopLevelProperty("purpose"_s, rawJSON);
        auto value = manifest.icons[0].purposes;
        EXPECT_EQ(expectedValues, value);

        auto shortcutManifest = parseShortcutIconFirstTopLevelProperty("purpose"_s, rawJSON);
        auto shortcutValue = shortcutManifest.shortcuts[0].icons[0].purposes;
        EXPECT_EQ(expectedValues, shortcutValue);
    }

    void testShortcutsURL(const String& rawJSON, const URL& expectedValue)
    {
        auto manifest = parseShortcutFirstTopLevelPropertyForURL("url"_s, rawJSON);
        auto value = manifest.shortcuts[0].url;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());
    }

    void testShortcutsName(const String &rawJSON, const String& expectedValue)
    {
        auto manifest = parseShortcutFirstTopLevelProperty("name"_s, rawJSON);
        auto value = manifest.shortcuts[0].name;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testId(const String& rawJSON, const URL& expectedValue)
    {
        auto manifest = parseTopLevelProperty("id"_s, rawJSON);
        auto value = manifest.id;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());
    }

    void testId(const String& rawJSON, const URL& startURL, const String& expectedValue)
    {
        auto manifestContent = makeString("{ \"id\" : \""_s, rawJSON, "\", \"start_url\" : \""_s, startURL.string(), "\" }"_s);
        auto manifest = parseString(manifestContent);
        auto value = manifest.id;
        EXPECT_STREQ(expectedValue.utf8().data(), value.string().utf8().data());
    }

};

static void assertManifestHasDefaultValues(const URL& manifestURL, const URL& documentURL, const ApplicationManifest& manifest)
{
    EXPECT_TRUE(manifest.name.isNull());
    EXPECT_TRUE(manifest.shortName.isNull());
    EXPECT_TRUE(manifest.description.isNull());
    EXPECT_STREQ("https://example.com/", manifest.scope.string().utf8().data());
    EXPECT_STREQ(documentURL.string().utf8().data(), manifest.startURL.string().utf8().data());
    EXPECT_STREQ(manifest.id.string().utf8().data(), manifest.startURL.string().utf8().data());
}

TEST_F(ApplicationManifestParserTest, DefaultManifest)
{
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(String()));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(""_s));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("{ }"_s));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("This is 100% not JSON."_s));
}

TEST_F(ApplicationManifestParserTest, RawJSON)
{
    testRawJSON("{ \"name\" : \"Example\", \"start_url\" : \"https://example.com\"}"_s, true);
    testRawJSON("{ \"start_url\" : \"https://example.com\"}"_s, true);
    testRawJSON("This is 100% not JSON."_s, false);
}

TEST_F(ApplicationManifestParserTest, Id)
{
    m_documentURL = URL { "https://example.com/home"_s };
    m_manifestURL = URL { "https://example.com/manifest.json"_s };

    testId("123"_s, m_documentURL);
    testId("null"_s, m_documentURL);
    testId("true"_s, m_documentURL);
    testId("{ }"_s, m_documentURL);
    testId("[ ]"_s, m_documentURL);
    testId("[ \"http://example.com/somepage\" ]"_s, m_documentURL);
    testId("\"\""_s, m_documentURL);
    testId("\"http:?\""_s, m_documentURL);

    testId("\"https://other-domain.com\""_s, m_documentURL);
    testId("\"https://invalid.com:a\""_s, m_documentURL);

    m_startURL = URL { "https://example.com/my-app/start?query=q#fragment"_s };
    testId(""_s, m_startURL , m_startURL.string());
    testId("/"_s, m_startURL, "https://example.com/"_s);
    testId("foo"_s, m_startURL, "https://example.com/foo"_s);
    testId("./foo"_s, m_startURL, "https://example.com/foo"_s);
    testId("foo/"_s, m_startURL, "https://example.com/foo/"_s);
    testId("../../foo/bar"_s, m_startURL, "https://example.com/foo/bar"_s);
    testId("../../foo/bar?query=hi#fragment"_s, m_startURL, "https://example.com/foo/bar?query=hi"_s);
    testId("../../foo/bar?query=hi#"_s, m_startURL, "https://example.com/foo/bar?query=hi"_s);

    testId("https://example.com/foo"_s, m_startURL, "https://example.com/foo"_s);
    testId("https://example.com/foo#"_s, m_startURL, "https://example.com/foo"_s);
    testId("https://example.com/foo#fragment"_s, m_startURL, "https://example.com/foo"_s);
    testId("https://anothersite.com/foo"_s, m_startURL, m_startURL.string());
    testId("https://invalid.com:a"_s, m_startURL, m_startURL.string());
}

TEST_F(ApplicationManifestParserTest, ManifestURL)
{
    m_documentURL = URL { "https://example.com/home"_s };

    testManifestURL("https://example.com/manifest.json"_s);
    testManifestURL("https://example.com/test/manifest.json"_s);
}

TEST_F(ApplicationManifestParserTest, StartURL)
{
    m_documentURL = URL { "https://example.com/home"_s };
    m_manifestURL = URL { "https://example.com/manifest.json"_s };

    testStartURL("123"_s, m_documentURL);
    testStartURL("null"_s, m_documentURL);
    testStartURL("true"_s, m_documentURL);
    testStartURL("{ }"_s, m_documentURL);
    testStartURL("[ ]"_s, m_documentURL);
    testStartURL("[ \"http://example.com/somepage\" ]"_s, m_documentURL);
    testStartURL("\"\""_s, m_documentURL);
    testStartURL("\"http:?\""_s, m_documentURL);

    testStartURL("\"appstartpage\""_s, "https://example.com/appstartpage"_s);
    testStartURL("\"a/b/cdefg\""_s, "https://example.com/a/b/cdefg"_s);

    m_documentURL = URL { "https://example.com/subfolder/home"_s };
    m_manifestURL = URL { "https://example.com/resources/manifest.json"_s };

    testStartURL("\"resource-relative-to-manifest-url\""_s, "https://example.com/resources/resource-relative-to-manifest-url"_s);
    testStartURL("\"http://different-page.com/12/34\""_s, m_documentURL);

    m_documentURL = URL { "https://example.com/home"_s };
    m_manifestURL = URL { "https://other-domain.com/manifiest.json"_s };

    testStartURL("\"resource_on_other_domain\""_s, m_documentURL);
    testStartURL("\"http://example.com/scheme-does-not-match-document\""_s, m_documentURL);
    testStartURL("\"https://example.com:123/port-does-not-match-document"_s, m_documentURL);
    testStartURL("\"https://example.com/page2\""_s, "https://example.com/page2"_s);
    testStartURL("\"//example.com/page2\""_s, "https://example.com/page2"_s);

    m_documentURL = URL { "https://example.com/a"_s };
    m_manifestURL = URL { "https://example.com/z/manifest.json"_s };

    testStartURL("\"b/c\""_s, "https://example.com/z/b/c"_s);
    testStartURL("\"/b/c\""_s, "https://example.com/b/c"_s);
    testStartURL("\"?query\""_s, "https://example.com/z/manifest.json?query"_s);

    m_documentURL = URL { "https://example.com/dir1/dir2/page1"_s };
    m_manifestURL = URL { "https://example.com/dir3/manifest.json"_s };

    testStartURL("\"../page2\""_s, "https://example.com/page2"_s);
}

TEST_F(ApplicationManifestParserTest, Display)
{
    testDisplay("123"_s, ApplicationManifest::Display::Browser);
    testDisplay("null"_s, ApplicationManifest::Display::Browser);
    testDisplay("true"_s, ApplicationManifest::Display::Browser);
    testDisplay("{ }"_s, ApplicationManifest::Display::Browser);
    testDisplay("[ ]"_s, ApplicationManifest::Display::Browser);
    testDisplay("\"\""_s, ApplicationManifest::Display::Browser);
    testDisplay("\"garbage string\""_s, ApplicationManifest::Display::Browser);

    testDisplay("\"browser\""_s, ApplicationManifest::Display::Browser);
    testDisplay("\"standalone\""_s, ApplicationManifest::Display::Standalone);
    testDisplay("\"minimal-ui\""_s, ApplicationManifest::Display::MinimalUI);
    testDisplay("\"fullscreen\""_s, ApplicationManifest::Display::Fullscreen);
    testDisplay("\"\t\nMINIMAL-UI \""_s, ApplicationManifest::Display::MinimalUI);
}

TEST_F(ApplicationManifestParserTest, Orientation)
{
    testOrientation(""_s, std::nullopt);
    testOrientation("123"_s, std::nullopt);
    testOrientation("null"_s, std::nullopt);
    testOrientation("true"_s, std::nullopt);
    testOrientation("{ }"_s, std::nullopt);
    testOrientation("[ ]"_s, std::nullopt);
    testOrientation("\"\""_s, std::nullopt);
    testOrientation("\"garbage string\""_s, std::nullopt);

    testOrientation("\"any\""_s, WebCore::ScreenOrientationLockType::Any);
    testOrientation("\"natural\""_s, WebCore::ScreenOrientationLockType::Natural);
    testOrientation("\"landscape\""_s, WebCore::ScreenOrientationLockType::Landscape);
    testOrientation("\"landscape-primary\""_s, WebCore::ScreenOrientationLockType::LandscapePrimary);
    testOrientation("\"landscape-secondary\""_s, WebCore::ScreenOrientationLockType::LandscapeSecondary);
    testOrientation("\"portrait\""_s, WebCore::ScreenOrientationLockType::Portrait);
    testOrientation("\"portrait-primary\""_s, WebCore::ScreenOrientationLockType::PortraitPrimary);
    testOrientation("\"portrait-secondary\""_s, WebCore::ScreenOrientationLockType::PortraitSecondary);
}

TEST_F(ApplicationManifestParserTest, Name)
{
    testName("123"_s, String());
    testName("null"_s, String());
    testName("true"_s, String());
    testName("{ }"_s, String());
    testName("[ ]"_s, String());
    testName("\"\""_s, emptyString());
    testName("\"example\""_s, "example"_s);
    testName("\"\\t Hello\\nWorld\\t \""_s, "Hello\nWorld"_s);
}

TEST_F(ApplicationManifestParserTest, Description)
{
    testDescription("123"_s, String());
    testDescription("null"_s, String());
    testDescription("true"_s, String());
    testDescription("{ }"_s, String());
    testDescription("[ ]"_s, String());
    testDescription("\"\""_s, emptyString());
    testDescription("\"example\""_s, "example"_s);
    testDescription("\"\\t Hello\\nWorld\\t \""_s, "Hello\nWorld"_s);
}

TEST_F(ApplicationManifestParserTest, ShortName)
{
    testShortName("123"_s, String());
    testShortName("null"_s, String());
    testShortName("true"_s, String());
    testShortName("{ }"_s, String());
    testShortName("[ ]"_s, String());
    testShortName("\"\""_s, ""_s);
    testShortName("\"example\""_s, "example"_s);
    testShortName("\"\\t Hello\\nWorld\\t \""_s, "Hello\nWorld"_s);
}

TEST_F(ApplicationManifestParserTest, Scope)
{
    // If the scope is not a string or not a valid URL, return the default scope (the parent path of the start URL).
    m_documentURL = URL { "https://example.com/a/page?queryParam=value#fragment"_s };
    m_startURL = URL { "https://example.com/a/page?queryParam=value#fragment"_s };
    m_manifestURL = URL { "https://example.com/manifest.json"_s };
    testScope("123"_s, "https://example.com/a/"_s, true);
    testScope("null"_s, "https://example.com/a/"_s, true);
    testScope("true"_s, "https://example.com/a/"_s, true);
    testScope("{ }"_s, "https://example.com/a/"_s, true);
    testScope("[ ]"_s, "https://example.com/a/"_s, true);
    testScope("\"\""_s, "https://example.com/a/"_s, true);
    testScope("\"http:?\""_s, "https://example.com/a/"_s, true);

    m_documentURL = URL { "https://example.com/a/pageEndingWithSlash/"_s };
    m_startURL = URL { "https://example.com/a/pageEndingWithSlash/"_s };
    testScope("null"_s, "https://example.com/a/pageEndingWithSlash/"_s, true);

    // If scope URL is not same origin as document URL, return the default scope.
    m_documentURL = URL { "https://example.com/home"_s };
    m_startURL = URL { "https://example.com/home"_s };
    m_manifestURL = URL { "https://other-site.com/manifest.json"_s };
    testScope("\"https://other-site.com/some-scope\""_s, "https://example.com/"_s, true);

    m_documentURL = URL { "https://example.com/app/home"_s };
    m_startURL = URL { "https://example.com/app/home"_s };
    m_manifestURL = URL { "https://example.com/app/manifest.json"_s };

    // If start URL is not within scope of scope URL, return the default scope.
    testScope("\"https://example.com/subdirectory\""_s, "https://example.com/app/"_s, true);
    testScope("\"https://example.com/app\""_s, "https://example.com/app"_s, false);
    testScope("\"https://example.com/APP\""_s, "https://example.com/app/"_s, true);
    testScope("\"https://example.com/a\""_s, "https://example.com/a"_s, false);

    m_documentURL = URL { "https://example.com/a/b/c/index"_s };
    m_startURL = URL { "https://example.com/a/b/c/index"_s };
    m_manifestURL = URL { "https://example.com/a/manifest.json"_s };

    testScope("\"./b/c/index\""_s, "https://example.com/a/b/c/index"_s, false);
    testScope("\"b/somewhere-else/../c\""_s, "https://example.com/a/b/c"_s, false);
    testScope("\"b\""_s, "https://example.com/a/b"_s, false);
    testScope("\"b/\""_s, "https://example.com/a/b/"_s, false);

    m_documentURL = URL { "https://example.com/documents/home"_s };
    m_startURL = URL { "https://example.com/documents/home"_s };
    m_manifestURL = URL { "https://example.com/resources/manifest.json"_s };

    // Removes query
    testScope("\"https://example.com/documents/home?query\""_s, "https://example.com/documents/home"_s, false);
    testScope("\"https://example.com/documents/home?query=whatever\""_s, "https://example.com/documents/home"_s, false);

    // Removes fragment
    testScope("\"https://example.com/documents/home#\""_s, "https://example.com/documents/home"_s, false);
    testScope("\"https://example.com/documents/home#fragment\""_s, "https://example.com/documents/home"_s, false);

    // Removes query and fragment
    testScope("\"https://example.com/documents/home?#\""_s, "https://example.com/documents/home"_s, false);
    testScope("\"https://example.com/documents/home?query#fragment\""_s, "https://example.com/documents/home"_s, false);

    // It's fine if the document URL or manifest URL aren't within the application scope - only the start URL needs to be.
    testScope("\"https://example.com/other\""_s, "https://example.com/other/start-url"_s, "https://example.com/other"_s, false);
}

TEST_F(ApplicationManifestParserTest, BackgroundColor)
{
    testBackgroundColor("123"_s, Color());
    testBackgroundColor("null"_s, Color());
    testBackgroundColor("true"_s, Color());
    testBackgroundColor("{ }"_s, Color());
    testBackgroundColor("[ ]"_s, Color());
    testBackgroundColor("\"\""_s, Color());
    testBackgroundColor("\"garbage string\""_s, Color());

    testBackgroundColor("\"red\""_s, Color::red);
    testBackgroundColor("\"#f00\""_s, Color::red);
    testBackgroundColor("\"#ff0000\""_s, Color::red);
    testBackgroundColor("\"#ff0000ff\""_s, Color::red);
    testBackgroundColor("\"rgb(255, 0, 0)\""_s, Color::red);
    testBackgroundColor("\"rgba(255, 0, 0, 1)\""_s, Color::red);
    testBackgroundColor("\"hsl(0, 100%, 50%)\""_s, Color::red);
    testBackgroundColor("\"hsla(0, 100%, 50%, 1)\""_s, Color::red);
}

TEST_F(ApplicationManifestParserTest, ThemeColor)
{
    testThemeColor("123"_s, Color());
    testThemeColor("null"_s, Color());
    testThemeColor("true"_s, Color());
    testThemeColor("{ }"_s, Color());
    testThemeColor("[ ]"_s, Color());
    testThemeColor("\"\""_s, Color());
    testThemeColor("\"garbage string\""_s, Color());

    testThemeColor("\"red\""_s, Color::red);
    testThemeColor("\"#f00\""_s, Color::red);
    testThemeColor("\"#ff0000\""_s, Color::red);
    testThemeColor("\"#ff0000ff\""_s, Color::red);
    testThemeColor("\"rgb(255, 0, 0)\""_s, Color::red);
    testThemeColor("\"rgba(255, 0, 0, 1)\""_s, Color::red);
    testThemeColor("\"hsl(0, 100%, 50%)\""_s, Color::red);
    testThemeColor("\"hsla(0, 100%, 50%, 1)\""_s, Color::red);
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
    auto manifest = parseString("  { \"name\": \"PASS\" }\n"_s);

    EXPECT_STREQ("PASS", manifest.name.utf8().data());
}

TEST_F(ApplicationManifestParserTest, Icons)
{
    URL srcURL = URL { "https://example.com/icon.jpg"_s };
    testIconsSrc("\"icon.jpg\""_s, srcURL);
    testIconsType("\"image/webp\""_s, "image/webp"_s);
    testIconsSizes("\"256x256\""_s, 1, 0, "256x256"_s);
    testIconsSizes("\"72x72 96x96\""_s, 2, 0, "72x72"_s);
    testIconsSizes("\"72x72 96x96\""_s, 2, 1, "96x96"_s);

    OptionSet<ApplicationManifest::Icon::Purpose> purposeAny { ApplicationManifest::Icon::Purpose::Any };
    OptionSet<ApplicationManifest::Icon::Purpose> purposeMonochrome { ApplicationManifest::Icon::Purpose::Monochrome };
    OptionSet<ApplicationManifest::Icon::Purpose> purposeMaskable { ApplicationManifest::Icon::Purpose::Maskable };

    testIconsPurposes("\"monochrome\""_s, purposeMonochrome);
    testIconsPurposes("\"maskable\""_s, purposeMaskable);
    testIconsPurposes("\"any\""_s, purposeAny);
    testIconsPurposes("\"\tMONOCHROME\""_s, purposeMonochrome);

    testIconsPurposes("123"_s, purposeAny);
    testIconsPurposes("null"_s, purposeAny);
    testIconsPurposes("true"_s, purposeAny);
    testIconsPurposes("{ }"_s, purposeAny);
    testIconsPurposes("[ ]"_s, purposeAny);

    OptionSet<ApplicationManifest::Icon::Purpose> purposeMonochromeAny { ApplicationManifest::Icon::Purpose::Monochrome, ApplicationManifest::Icon::Purpose::Any };

    testIconsPurposes("\"monochrome any\""_s, purposeMonochromeAny);
}

TEST_F(ApplicationManifestParserTest, Shortcuts)
{
    testShortcutsURL("\"example1\""_s, URL { "https://example.com/example1"_s });
    testShortcutsURL("\"/example2\""_s, URL { "https://example.com/example2"_s });
    testShortcutsURL("\"/example3/\""_s, URL { "https://example.com/example3/"_s });
    testShortcutsURL("\"https://example.com/example4\""_s, URL { "https://example.com/example4"_s });

    testShortcutsName("\"Example\""_s, "Example"_s);
    testShortcutsName("\" \""_s, ""_s);
    testShortcutsName("\"\""_s, ""_s);
}

#endif
