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

#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebPageSerializer.h"
#include "WebScriptSource.h"
#include "WebSettings.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebView.h"

#include <googleurl/src/gurl.h>
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class TestWebFrameClient : public WebFrameClient {
public:
    TestWebFrameClient() : m_scriptEnabled(false) { }
    virtual bool allowScript(WebFrame*, bool /* enabledPerSettings */) { return m_scriptEnabled; }
    bool m_scriptEnabled;
};

class WebPageNewSerializeTest : public testing::Test {
public:
    WebPageNewSerializeTest()
        : m_htmlMimeType(WebString::fromUTF8("text/html"))
        , m_xhtmlMimeType(WebString::fromUTF8("application/xhtml+xml"))
        , m_cssMimeType(WebString::fromUTF8("text/css"))
        , m_pngMimeType(WebString::fromUTF8("image/png"))
    {
    }

protected:
    virtual void SetUp()
    {
        // Create and initialize the WebView.
        m_webView = WebView::create(0);

        // We want the images to load.
        WebSettings* settings = m_webView->settings();
        settings->setImagesEnabled(true);
        settings->setLoadsImagesAutomatically(true);

        m_webView->initializeMainFrame(&m_webFrameClient);
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
        m_webView->close();
    }

    void registerMockedURLLoad(const WebURL& url, const WebString& fileName, const WebString& mimeType)
    {
        WebURLResponse response;
        response.initialize();
        response.setMIMEType(mimeType);
        response.setHTTPStatusCode(200);
        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath.append("/Source/WebKit/chromium/tests/data/pageserializer/");
        filePath.append(fileName.utf8());
        webkit_support::RegisterMockedURL(url, response, WebString::fromUTF8(filePath));
    }

    void loadURLInTopFrame(const GURL& url)
    {
        WebURLRequest urlRequest;
        urlRequest.initialize();
        urlRequest.setURL(WebURL(url));
        m_webView->mainFrame()->loadRequest(urlRequest);
        // Make sure any pending request get served.
        webkit_support::ServeAsynchronousMockedRequests();
        // Some requests get delayed, run the timer.
        webkit_support::RunAllPendingMessages();
        // Server the delayed resources.
        webkit_support::ServeAsynchronousMockedRequests();
    }

    void enableJS()
    {
        m_webFrameClient.m_scriptEnabled = true;
    }

    void runOnLoad()
    {
        m_webView->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8("onLoad()")));
    }

    const WebString& htmlMimeType() const { return m_htmlMimeType; }
    const WebString& xhtmlMimeType() const { return m_xhtmlMimeType; }
    const WebString& cssMimeType() const { return m_cssMimeType; }
    const WebString& pngMimeType() const { return m_pngMimeType; }
    
    static bool resourceVectorContains(const WebVector<WebPageSerializer::Resource>& resources, char* url, char* mimeType)
    {
        WebURL webURL = WebURL(GURL(url));
        for (size_t i = 0; i < resources.size(); ++i) {
            const WebPageSerializer::Resource& resource = resources[i];
            if (resource.url == webURL && !resource.data.isEmpty() && !resource.mimeType.compare(WebCString(mimeType)))
                return true;
        }
        return false;
    }
    
    WebView* m_webView;

private:
    WebString m_htmlMimeType;
    WebString m_xhtmlMimeType;
    WebString m_cssMimeType;
    WebString m_pngMimeType;
    TestWebFrameClient m_webFrameClient;
};

// Tests that a page with resources and sub-frame is reported with all its resources.
// FIXME: reenable these tests once the Chromium part of the test_webkit_client have landed.
TEST_F(WebPageNewSerializeTest, DISABLED_PageWithFrames)
{
    // Register the mocked frames.
    WebURL topFrameURL = GURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("top_frame.html"), htmlMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/iframe.html"), WebString::fromUTF8("iframe.html"), htmlMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/iframe2.html"), WebString::fromUTF8("iframe2.html"), htmlMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/green_background.png"), WebString::fromUTF8("green_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), pngMimeType());

    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(m_webView, &resources);
    ASSERT_FALSE(resources.isEmpty());
    
    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == GURL("http://www.test.com"));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(6, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/green_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/iframe.html", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/iframe2.html", "text/html"));
}

// Test that when serializing a page, all CSS resources are reported, including url()'s
// and imports and links. Note that we don't test the resources contents, we only make sure
// they are all reported with the right mime type and that they contain some data.
TEST_F(WebPageNewSerializeTest, DISABLED_CSSResources)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = GURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("css_test_page.html"), htmlMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/link_styles.css"), WebString::fromUTF8("link_styles.css"), cssMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/import_style_from_link.css"), WebString::fromUTF8("import_style_from_link.css"), cssMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/import_styles.css"), WebString::fromUTF8("import_styles.css"), cssMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/orange_background.png"), WebString::fromUTF8("orange_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/yellow_background.png"), WebString::fromUTF8("yellow_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/green_background.png"), WebString::fromUTF8("green_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/purple_background.png"), WebString::fromUTF8("purple_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/ul-dot.png"), WebString::fromUTF8("ul-dot.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/ol-dot.png"), WebString::fromUTF8("ol-dot.png"), pngMimeType());

    enableJS();
    loadURLInTopFrame(topFrameURL);
    runOnLoad();

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(m_webView, &resources);
    ASSERT_FALSE(resources.isEmpty());
    
    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == GURL("http://www.test.com"));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(12, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/link_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/import_styles.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/import_style_from_link.css", "text/css"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/orange_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/yellow_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/green_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/purple_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/ul-dot.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/ol-dot.png", "image/png"));
}

// Tests that when serializing a page with blank frames these are reported with their resources.
TEST_F(WebPageNewSerializeTest, DISABLED_BlankFrames)
{
    // Register the mocked frame and load it.
    WebURL topFrameURL = GURL("http://www.test.com");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("blank_frames.html"), htmlMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/red_background.png"), WebString::fromUTF8("red_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/orange_background.png"), WebString::fromUTF8("orange_background.png"), pngMimeType());
    registerMockedURLLoad(GURL("http://www.test.com/blue_background.png"), WebString::fromUTF8("blue_background.png"), pngMimeType());

    enableJS();
    loadURLInTopFrame(topFrameURL);
    runOnLoad();

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(m_webView, &resources);
    ASSERT_FALSE(resources.isEmpty());
    
    // The first resource should be the main-frame.
    const WebPageSerializer::Resource& resource = resources[0];
    EXPECT_TRUE(resource.url == GURL("http://www.test.com"));
    EXPECT_EQ(0, resource.mimeType.compare(WebCString("text/html")));
    EXPECT_FALSE(resource.data.isEmpty());

    EXPECT_EQ(7, resources.size()); // There should be no duplicates.
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/orange_background.png", "image/png"));
    EXPECT_TRUE(resourceVectorContains(resources, "http://www.test.com/blue_background.png", "image/png"));
    // The blank frames should have got a magic URL.
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/0", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/1", "text/html"));
    EXPECT_TRUE(resourceVectorContains(resources, "wyciwyg://frame/2", "text/html"));
}

TEST_F(WebPageNewSerializeTest, DISABLED_SerializeXMLHasRightDeclaration)
{
    WebURL topFrameURL = GURL("http://www.test.com/simple.xhtml");
    registerMockedURLLoad(topFrameURL, WebString::fromUTF8("simple.xhtml"), xhtmlMimeType());

    loadURLInTopFrame(topFrameURL);

    WebVector<WebPageSerializer::Resource> resources;
    WebPageSerializer::serialize(m_webView, &resources);
    ASSERT_FALSE(resources.isEmpty());
    
    // We expect only one resource, the XML.
    ASSERT_EQ(1, resources.size());
    std::string xml = resources[0].data;

    // We should have one and only one instance of the XML declaration.
    size_t pos = xml.find("<?xml version=");
    ASSERT_TRUE(pos != std::string::npos);

    pos = xml.find("<?xml version=", pos + 1);
    ASSERT_TRUE(pos == std::string::npos);
}

}
