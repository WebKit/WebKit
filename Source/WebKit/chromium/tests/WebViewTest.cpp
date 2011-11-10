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
#include "WebView.h"

#include "Document.h"
#include "FrameTestHelpers.h"
#include "HTMLDocument.h"
#include "WebDocument.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebViewClient.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class WebViewTest : public testing::Test {
public:
    WebViewTest()
        : m_baseURL("http://www.test.com/")
    {
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

protected:
    std::string m_baseURL;
};

TEST_F(WebViewTest, FocusIsInactive)
{
    FrameTestHelpers::registerMockedURLLoadAsHTML(m_baseURL, "visible_iframe.html");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "visible_iframe.html");

    webView->setFocus(true);
    WebFrameImpl* frame = static_cast<WebFrameImpl*>(webView->mainFrame());
    EXPECT_TRUE(frame->frame()->document()->isHTMLDocument());

    WebCore::HTMLDocument* document = static_cast<WebCore::HTMLDocument*>(frame->frame()->document());
    EXPECT_TRUE(document->hasFocus());
    webView->setFocus(false);
    EXPECT_FALSE(document->hasFocus());
    webView->setFocus(true);
    EXPECT_TRUE(document->hasFocus());

    webView->close();
}

class TestWebViewClient : public WebViewClient {
public:
    TestWebViewClient() : m_didStartLoading(false), m_didStopLoading(false), m_loadProgress(0) { }
    virtual void didStartLoading() { m_didStartLoading = true; }
    virtual void didStopLoading() { m_didStopLoading = true; }
    virtual void didChangeLoadProgress(WebFrame*, double loadProgress) { m_loadProgress = loadProgress; }

    bool loadingStarted() const { return m_didStartLoading; }
    bool loadingStopped() const { return m_didStopLoading; }
    double loadProgress() const { return m_loadProgress; }

private:
    bool m_didStartLoading;
    bool m_didStopLoading;
    double m_loadProgress;
};

TEST_F(WebViewTest, MHTMLWithMissingResourceFinishesLoading)
{
    TestWebViewClient webViewClient;

    std::string fileName = "page_with_image.mht";
    std::string fileDir = webkit_support::GetWebKitRootDir().utf8();
    fileDir.append("/Source/WebKit/chromium/tests/data/");
    // Making file loading works in unit-tests would require some additional work.
    // Mocking them as regular URLs works fine in the meantime.
    FrameTestHelpers::registerMockedURLLoad("file://" + fileDir, fileName, "multipart/related");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad("file://" + fileDir + fileName, true, &webViewClient);

    EXPECT_TRUE(webViewClient.loadingStarted());
    EXPECT_TRUE(webViewClient.loadingStopped());
    EXPECT_EQ(1.0, webViewClient.loadProgress());

    // Close the WebView after checking the loading state and progress, as the close() call triggers a stop loading callback.
    webView->close();
}

}
