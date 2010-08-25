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

#include <googleurl/src/gurl.h>
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebView.h"

using namespace WebKit;

namespace {

class WebFrameTest : public testing::Test {
public:
    WebFrameTest() {}

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

    void registerMockedURLLoad(const WebURL& url, const WebURLResponse& response, const WebString& fileName)
    {
        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath.append("/WebKit/chromium/tests/data/");
        filePath.append(fileName.utf8());
        webkit_support::RegisterMockedURL(url, response, WebString::fromUTF8(filePath));
    }

    void serveRequests()
    {
        webkit_support::ServeAsynchronousMockedRequests();
    }
};

class TestWebFrameClient : public WebFrameClient {
};

TEST_F(WebFrameTest, ContentText)
{
    // Register our resources.
    WebURLResponse response;
    response.initialize();
    response.setMIMEType("text/html");
    std::string rootURL = "http://www.test.com/";
    const char* files[] = { "iframes_test.html", "visible_iframe.html",
                            "invisible_iframe.html", "zero_sized_iframe.html" };
    for (int i = 0; i < (sizeof(files) / sizeof(char*)); ++i) {
        WebURL webURL = GURL(rootURL + files[i]);
        registerMockedURLLoad(webURL, response, WebString::fromUTF8(files[i]));
    }

    // Create and initialize the WebView.    
    TestWebFrameClient webFrameClient;
    WebView* webView = WebView::create(0, 0);
    webView->initializeMainFrame(&webFrameClient);

    // Load the main frame URL.
    WebURL testURL(GURL(rootURL + files[0]));
    WebURLRequest urlRequest;
    urlRequest.initialize();
    urlRequest.setURL(testURL);
    webView->mainFrame()->loadRequest(urlRequest);

    // Load all pending asynchronous requests.
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

}
