/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPageSerializer.h"

#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebView.h"
#include <public/WebString.h>
#include <public/WebURL.h>
#include <public/WebURLRequest.h>
#include <public/WebURLResponse.h>

#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;
using WebKit::URLTestHelpers::toKURL;

namespace {

class TestWebFrameClient : public WebFrameClient {
};

class WebPageSerializerTest : public testing::Test {
public:
    WebPageSerializerTest() : m_webView(0), m_supportedSchemes(static_cast<size_t>(3))
    {
        m_supportedSchemes[0] = "http";
        m_supportedSchemes[1] = "https";
        m_supportedSchemes[2] = "file";
    }

protected:
    virtual void SetUp()
    {
        // Create and initialize the WebView.
        m_webView = WebView::create(0);
        m_webView->initializeMainFrame(&m_webFrameClient);
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
        m_webView->close();
    }

    void registerMockedURLLoad(const std::string& url, const WebString& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(toKURL(url), fileName, WebString::fromUTF8("pageserialization/"), WebString::fromUTF8("text/html"));
    }

    void loadURLInTopFrame(const WebURL& url)
    {
        WebURLRequest urlRequest;
        urlRequest.initialize();
        urlRequest.setURL(url);
        m_webView->mainFrame()->loadRequest(urlRequest);
        // Make sure any pending request get served.
        webkit_support::ServeAsynchronousMockedRequests();
    }

    static bool webVectorContains(const WebVector<WebURL>& vector, const char* url)
    {
        return vector.contains(WebURL(toKURL(std::string(url))));
    }

    // Useful for debugging.
    static void printWebURLs(const WebVector<WebURL>& urls)
    {
        for (size_t i = 0; i < urls.size(); i++)
            printf("%s\n", urls[i].spec().data());
    }

    WebView* m_webView;
    WebVector<WebCString> m_supportedSchemes;

private:
    TestWebFrameClient m_webFrameClient;
};

TEST_F(WebPageSerializerTest, HTMLNodes)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad("http://www.test.com", WebString::fromUTF8("simple_page.html"));
    loadURLInTopFrame(topFrameURL);

    // Retrieve all resources.
    WebVector<WebURL> frames;
    WebVector<WebURL> resources;
    ASSERT_TRUE(WebPageSerializer::retrieveAllResources(
        m_webView, m_supportedSchemes, &resources, &frames));

    // Tests that all resources from the frame have been retrieved.
    EXPECT_EQ(1U, frames.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com"));

    EXPECT_EQ(14U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(resources, "http://www.example.com/beautifull.css"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.js"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/bodyBackground.jpg"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/imageButton.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/tableBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/trBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/tdBackground.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.evene.fr/citations/auteur.php?ida=46"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.brainyquote.com/quotes/authors/c/charles_darwin.html"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/why_deleted.html"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/why_inserted.html"));
    EXPECT_TRUE(webVectorContains(resources, "https://www.secure.com/https.gif"));
    EXPECT_TRUE(webVectorContains(resources, "file://c/my_folder/file.gif"));
}

TEST_F(WebPageSerializerTest, MultipleFrames)
{
    // Register the mocked frames.
    WebURL topFrameURL = toKURL("http://www.test.com");
    registerMockedURLLoad("http://www.test.com", WebString::fromUTF8("top_frame.html"));
    registerMockedURLLoad("http://www.test.com/simple_iframe.html",
                          WebString::fromUTF8("simple_iframe.html"));
    registerMockedURLLoad("http://www.test.com/object_iframe.html",
                          WebString::fromUTF8("object_iframe.html"));
    registerMockedURLLoad("http://www.test.com/embed_iframe.html",
                          WebString::fromUTF8("embed_iframe.html"));
    // If we don't register a mocked resource for awesome.png, it causes the
    // document loader of the iframe that has it as its src to assert on close,
    // not sure why.
    registerMockedURLLoad("http://www.test.com/awesome.png",
                          WebString::fromUTF8("awesome.png"));

    loadURLInTopFrame(topFrameURL);

    // Retrieve all resources.
    WebVector<WebURL> frames;
    WebVector<WebURL> resources;
    ASSERT_TRUE(WebPageSerializer::retrieveAllResources(
        m_webView, m_supportedSchemes, &resources, &frames));

    // Tests that all resources from the frame have been retrieved.
    EXPECT_EQ(4U, frames.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/simple_iframe.html"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/object_iframe.html"));
    EXPECT_TRUE(webVectorContains(frames, "http://www.test.com/embed_iframe.html"));

    EXPECT_EQ(5U, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/awesome.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/innerFrame.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/flash.swf"));
    // FIXME: for some reason the following resources is missing on one of the bot
    //        causing the test to fail. Probably a plugin issue.
    // EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/music.mid"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/object.png"));
    EXPECT_TRUE(webVectorContains(resources, "http://www.test.com/embed.png"));
}

}
