/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
} // namespace WebCore

class ApplicationManifestParserTest : public testing::Test {
public:
    URL m_manifestURL;
    URL m_documentURL;

    virtual void SetUp()
    {
        JSC::initializeThreading();
        WTF::initializeMainThread();

        m_manifestURL = { { }, "https://example.com/manifest.json" };
        m_documentURL = { { }, "https://example.com/" };
    }

    ApplicationManifest parseString(const String& data)
    {
        return ApplicationManifestParser::parse(data, m_manifestURL, m_documentURL);
    }

    ApplicationManifest parseTopLevelProperty(const String& key, const String& value)
    {
        String manifestContent = "{ \"" + key + "\" : " + value + " }";
        return parseString(manifestContent);
    }

    void testStartURL(const String& rawJSON, const String& expectedValue)
    {
        testStartURL(rawJSON, { { }, expectedValue });
    }

    void testStartURL(const String& rawJSON, const URL& expectedValue)
    {
        auto manifest = parseTopLevelProperty("start_url", rawJSON);
        auto value = manifest.startURL;
        EXPECT_STREQ(expectedValue.string().utf8().data(), value.string().utf8().data());
    }

    void testDisplay(const String& rawJSON, ApplicationManifest::Display expectedValue)
    {
        auto manifest = parseTopLevelProperty("display", rawJSON);
        auto value = manifest.display;
        EXPECT_EQ(expectedValue, value);
    }

    void testName(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("name", rawJSON);
        auto value = manifest.name;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testDescription(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("description", rawJSON);
        auto value = manifest.description;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testShortName(const String& rawJSON, const String& expectedValue)
    {
        auto manifest = parseTopLevelProperty("short_name", rawJSON);
        auto value = manifest.shortName;
        EXPECT_STREQ(expectedValue.utf8().data(), value.utf8().data());
    }

    void testScope(const String& rawJSON, const String& startURL, const String& expectedValue)
    {
        String manifestContent = "{ \"scope\" : " + rawJSON + ", \"start_url\" : \"" + startURL + "\" }";
        auto manifest = parseString(manifestContent);
        auto value = manifest.scope;
        EXPECT_STREQ(expectedValue.utf8().data(), value.string().utf8().data());
    }

    void testScope(const String& rawJSON, const String& expectedValue)
    {
        testScope(rawJSON, String(), expectedValue);
    }

};

static void assertManifestHasDefaultValues(const URL& manifestURL, const URL& documentURL, const ApplicationManifest& manifest)
{
    EXPECT_TRUE(manifest.name.isNull());
    EXPECT_TRUE(manifest.shortName.isNull());
    EXPECT_TRUE(manifest.description.isNull());
    EXPECT_STREQ("https://example.com/", manifest.scope.string().utf8().data());
    EXPECT_STREQ(documentURL.string().utf8().data(), manifest.startURL.string().utf8().data());
}

TEST_F(ApplicationManifestParserTest, DefaultManifest)
{
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(String()));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString(""));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("{ }"));
    assertManifestHasDefaultValues(m_manifestURL, m_documentURL, parseString("This is 100% not JSON."));
}

TEST_F(ApplicationManifestParserTest, StartURL)
{
    m_documentURL = { { }, "https://example.com/home" };
    m_manifestURL = { { }, "https://example.com/manifest.json" };

    testStartURL("123", m_documentURL);
    testStartURL("null", m_documentURL);
    testStartURL("true", m_documentURL);
    testStartURL("{ }", m_documentURL);
    testStartURL("[ ]", m_documentURL);
    testStartURL("[ \"http://example.com/somepage\" ]", m_documentURL);
    testStartURL("\"\"", m_documentURL);
    testStartURL("\"http:?\"", m_documentURL);

    testStartURL("\"appstartpage\"", "https://example.com/appstartpage");
    testStartURL("\"a/b/cdefg\"", "https://example.com/a/b/cdefg");

    m_documentURL = { { }, "https://example.com/subfolder/home" };
    m_manifestURL = { { }, "https://example.com/resources/manifest.json" };

    testStartURL("\"resource-relative-to-manifest-url\"", "https://example.com/resources/resource-relative-to-manifest-url");
    testStartURL("\"http://different-page.com/12/34\"", m_documentURL);

    m_documentURL = { { }, "https://example.com/home" };
    m_manifestURL = { { }, "https://other-domain.com/manifiest.json" };

    testStartURL("\"resource_on_other_domain\"", m_documentURL);
    testStartURL("\"http://example.com/scheme-does-not-match-document\"", m_documentURL);
    testStartURL("\"https://example.com:123/port-does-not-match-document", m_documentURL);
    testStartURL("\"https://example.com/page2\"", "https://example.com/page2");
    testStartURL("\"//example.com/page2\"", "https://example.com/page2");

    m_documentURL = { { }, "https://example.com/a" };
    m_manifestURL = { { }, "https://example.com/z/manifest.json" };

    testStartURL("\"b/c\"", "https://example.com/z/b/c");
    testStartURL("\"/b/c\"", "https://example.com/b/c");
    testStartURL("\"?query\"", "https://example.com/z/manifest.json?query");

    m_documentURL = { { }, "https://example.com/dir1/dir2/page1" };
    m_manifestURL = { { }, "https://example.com/dir3/manifest.json" };

    testStartURL("\"../page2\"", "https://example.com/page2");
}

TEST_F(ApplicationManifestParserTest, Display)
{
    testDisplay("123", ApplicationManifest::Display::Browser);
    testDisplay("null", ApplicationManifest::Display::Browser);
    testDisplay("true", ApplicationManifest::Display::Browser);
    testDisplay("{ }", ApplicationManifest::Display::Browser);
    testDisplay("[ ]", ApplicationManifest::Display::Browser);
    testDisplay("\"\"", ApplicationManifest::Display::Browser);
    testDisplay("\"garbage string\"", ApplicationManifest::Display::Browser);

    testDisplay("\"browser\"", ApplicationManifest::Display::Browser);
    testDisplay("\"standalone\"", ApplicationManifest::Display::Standalone);
    testDisplay("\"minimal-ui\"", ApplicationManifest::Display::MinimalUI);
    testDisplay("\"fullscreen\"", ApplicationManifest::Display::Fullscreen);
    testDisplay("\"\t\nMINIMAL-UI \"", ApplicationManifest::Display::MinimalUI);
}

TEST_F(ApplicationManifestParserTest, Name)
{
    testName("123", String());
    testName("null", String());
    testName("true", String());
    testName("{ }", String());
    testName("[ ]", String());
    testName("\"\"", "");
    testName("\"example\"", "example");
    testName("\"\\t Hello\\nWorld\\t \"", "Hello\nWorld");
}

TEST_F(ApplicationManifestParserTest, Description)
{
    testDescription("123", String());
    testDescription("null", String());
    testDescription("true", String());
    testDescription("{ }", String());
    testDescription("[ ]", String());
    testDescription("\"\"", "");
    testDescription("\"example\"", "example");
    testDescription("\"\\t Hello\\nWorld\\t \"", "Hello\nWorld");
}

TEST_F(ApplicationManifestParserTest, ShortName)
{
    testShortName("123", String());
    testShortName("null", String());
    testShortName("true", String());
    testShortName("{ }", String());
    testShortName("[ ]", String());
    testShortName("\"\"", "");
    testShortName("\"example\"", "example");
    testShortName("\"\\t Hello\\nWorld\\t \"", "Hello\nWorld");
}

TEST_F(ApplicationManifestParserTest, Scope)
{
    // If the scope is not a string or not a valid URL, return the default scope (the parent path of the start URL).
    m_documentURL = { { }, "https://example.com/a/page?queryParam=value#fragment" };
    m_manifestURL = { { }, "https://example.com/manifest.json" };
    testScope("123", "https://example.com/a/");
    testScope("null", "https://example.com/a/");
    testScope("true", "https://example.com/a/");
    testScope("{ }", "https://example.com/a/");
    testScope("[ ]", "https://example.com/a/");
    testScope("\"\"", "https://example.com/a/");
    testScope("\"http:?\"", "https://example.com/a/");

    m_documentURL = { { }, "https://example.com/a/pageEndingWithSlash/" };
    testScope("null", "https://example.com/a/pageEndingWithSlash/");

    // If scope URL is not same origin as document URL, return the default scope.
    m_documentURL = { { }, "https://example.com/home" };
    m_manifestURL = { { }, "https://other-site.com/manifest.json" };
    testScope("\"https://other-site.com/some-scope\"", "https://example.com/");

    m_documentURL = { { }, "https://example.com/app/home" };
    m_manifestURL = { { }, "https://example.com/app/manifest.json" };

    // If start URL is not within scope of scope URL, return the default scope.
    testScope("\"https://example.com/subdirectory\"", "https://example.com/app/");
    testScope("\"https://example.com/app\"", "https://example.com/app");
    testScope("\"https://example.com/APP\"", "https://example.com/app/");
    testScope("\"https://example.com/a\"", "https://example.com/a");

    m_documentURL = { { }, "https://example.com/a/b/c/index" };
    m_manifestURL = { { }, "https://example.com/a/manifest.json" };

    testScope("\"./b/c/index\"", "https://example.com/a/b/c/index");
    testScope("\"b/somewhere-else/../c\"", "https://example.com/a/b/c");
    testScope("\"b\"", "https://example.com/a/b");
    testScope("\"b/\"", "https://example.com/a/b/");

    m_documentURL = { { }, "https://example.com/documents/home" };
    m_manifestURL = { { }, "https://example.com/resources/manifest.json" };

    // It's fine if the document URL or manifest URL aren't within the application scope - only the start URL needs to be.
    testScope("\"https://example.com/other\"", String("https://example.com/other/start-url"), "https://example.com/other");
}

TEST_F(ApplicationManifestParserTest, Whitespace)
{
    auto manifest = parseString("  { \"name\": \"PASS\" }\n"_s);

    EXPECT_STREQ("PASS", manifest.name.utf8().data());
}

#endif
