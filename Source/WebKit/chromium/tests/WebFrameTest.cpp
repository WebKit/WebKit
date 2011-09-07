/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "ResourceError.h"
#include "WebDocument.h"
#include "WebFormElement.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebSearchableFormData.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebView.h"
#include "v8.h"
#include <googleurl/src/gurl.h>
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class WebFrameTest : public testing::Test {
public:
    WebFrameTest()
        : baseURL("http://www.test.com/"),
          chromeURL("chrome://")
    {
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        registerMockedURLLoad(baseURL, fileName);
    }

    void registerMockedChromeURLLoad(const std::string& fileName)
    {
        registerMockedURLLoad(chromeURL, fileName);
    }

    void serveRequests()
    {
        webkit_support::ServeAsynchronousMockedRequests();
    }

    void loadHttpFrame(WebFrame* frame, const std::string& fileName)
    {
        loadFrame(frame, baseURL, fileName);
    }

    void loadChromeFrame(WebFrame* frame, const std::string& fileName)
    {
        loadFrame(frame, chromeURL, fileName);
    }

    void registerMockedURLLoad(const std::string& base, const std::string& fileName)
    {
        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");

        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath += "/Source/WebKit/chromium/tests/data/";
        filePath += fileName;

        webkit_support::RegisterMockedURL(WebURL(GURL(base + fileName)), response, WebString::fromUTF8(filePath));
    }

    void loadFrame(WebFrame* frame, const std::string& base, const std::string& fileName)
    {
        WebURLRequest urlRequest;
        urlRequest.initialize();
        urlRequest.setURL(WebURL(GURL(base + fileName)));
        frame->loadRequest(urlRequest);
    }

protected:
    std::string baseURL;
    std::string chromeURL;
};

class TestWebFrameClient : public WebFrameClient {
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    // Create and initialize the WebView.
    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->initializeMainFrame(&webFrameClient);

    loadHttpFrame(webView->mainFrame(), "iframes_test.html");
    serveRequests();

    // Now retrieve the frames text and test it only includes visible elements.
    std::string content = webView->mainFrame()->contentAsText(1024).utf8();
    EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
    EXPECT_NE(std::string::npos, content.find(" visible iframe"));
    EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
    EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
    EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));

    webView->close();
}

TEST_F(WebFrameTest, FrameForEnteredContext)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    // Create and initialize the WebView.
     TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);

    loadHttpFrame(webView->mainFrame(), "iframes_test.html");
    serveRequests();

    v8::HandleScope scope;
    EXPECT_EQ(webView->mainFrame(),
              WebFrame::frameForContext(
                  webView->mainFrame()->mainWorldScriptContext()));
    EXPECT_EQ(webView->mainFrame()->firstChild(),
              WebFrame::frameForContext(
                  webView->mainFrame()->firstChild()->mainWorldScriptContext()));

    webView->close();
}

TEST_F(WebFrameTest, FormWithNullFrame)
{
    registerMockedHttpURLLoad("form.html");

    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->initializeMainFrame(&webFrameClient);

    loadHttpFrame(webView->mainFrame(), "form.html");
    serveRequests();

    WebVector<WebFormElement> forms;
    webView->mainFrame()->document().forms(forms);
    webView->close();

    EXPECT_EQ(forms.size(), 1U);

    // This test passes if this doesn't crash.
    WebSearchableFormData searchableDataForm(forms[0]);
}

TEST_F(WebFrameTest, ChromePageNoJavascript)
{
    registerMockedChromeURLLoad("history.html");

    // Create and initialize the WebView.
    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);

    loadChromeFrame(webView->mainFrame(), "history.html");
    serveRequests();

    // Try to run JS against the chrome-style URL.
    WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
    loadFrame(webView->mainFrame(), "javascript:", "document.body.appendChild(document.createTextNode('Clobbered'))");

    // Now retrieve the frames text and see if it was clobbered.
    std::string content = webView->mainFrame()->contentAsText(1024).utf8();
    EXPECT_NE(std::string::npos, content.find("Simulated Chromium History Page"));
    EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

class TestReloadDoesntRedirectWebFrameClient : public WebFrameClient {
public:
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, const WebURLRequest&, WebNavigationType,
        const WebNode& originatingNode,
        WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        EXPECT_EQ(false, isRedirect);
        return WebNavigationPolicyCurrentTab;
    }

    virtual WebURLError cancelledError(WebFrame*, const WebURLRequest& request)
    {
        // Return a dummy error so the DocumentLoader doesn't assert when
        // the reload cancels it.
        return WebURLError(WebCore::ResourceError("", 1, "", "cancelled"));
    }
};

TEST_F(WebFrameTest, ReloadDoesntSetRedirect)
{
    // Test for case in http://crbug.com/73104. Reloading a frame very quickly
    // would sometimes call decidePolicyForNavigation with isRedirect=true
    registerMockedHttpURLLoad("form.html");

    TestReloadDoesntRedirectWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->initializeMainFrame(&webFrameClient);

    loadHttpFrame(webView->mainFrame(), "form.html");
    serveRequests();
    // Frame is loaded.

    webView->mainFrame()->reload(true);
    // start reload before request is delivered.
    webView->mainFrame()->reload(true);
    serveRequests();
}

} // namespace
