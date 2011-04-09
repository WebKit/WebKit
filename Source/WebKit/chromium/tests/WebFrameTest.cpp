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

#include <googleurl/src/gurl.h>
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebSettings.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebView.h"
#include "v8.h"

using namespace WebKit;

namespace {

class WebFrameTest : public testing::Test {
public:
    WebFrameTest()
        : baseURL("http://www.test.com/")
    {
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

    void registerMockedURLLoad(const std::string& fileName)
    {
        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");

        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath += "/Source/WebKit/chromium/tests/data/";
        filePath += fileName;

        webkit_support::RegisterMockedURL(WebURL(GURL(baseURL + fileName)), response, WebString::fromUTF8(filePath));
    }

    void serveRequests()
    {
        webkit_support::ServeAsynchronousMockedRequests();
    }

    void loadFrame(WebFrame* frame, const std::string& fileName)
    {
        WebURLRequest urlRequest;
        urlRequest.initialize();
        urlRequest.setURL(WebURL(GURL(baseURL + fileName)));
        frame->loadRequest(urlRequest);
    }

protected:
    std::string baseURL;
};

class TestWebFrameClient : public WebFrameClient {
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedURLLoad("iframes_test.html");
    registerMockedURLLoad("visible_iframe.html");
    registerMockedURLLoad("invisible_iframe.html");
    registerMockedURLLoad("zero_sized_iframe.html");

    // Create and initialize the WebView.
    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->initializeMainFrame(&webFrameClient);

    loadFrame(webView->mainFrame(), "iframes_test.html");
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
    registerMockedURLLoad("iframes_test.html");
    registerMockedURLLoad("visible_iframe.html");
    registerMockedURLLoad("invisible_iframe.html");
    registerMockedURLLoad("zero_sized_iframe.html");

    // Create and initialize the WebView.
    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);

    loadFrame(webView->mainFrame(), "iframes_test.html");
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

}
