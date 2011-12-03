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
#include "FrameView.h"
#include "HTMLDocument.h"
#include "WebDocument.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebSize.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class TestData {
public:
    void setWebView(WebView* webView) { m_webView = static_cast<WebViewImpl*>(webView); }
    void setSize(const WebSize& newSize) { m_size = newSize; }
    bool hasHorizontalScrollbar() const { return m_webView->hasHorizontalScrollbar(); }
    bool hasVerticalScrollbar() const  { return m_webView->hasVerticalScrollbar(); }
    int width() const { return m_size.width; }
    int height() const { return m_size.height; }

private:
    WebSize m_size;
    WebViewImpl* m_webView;
};

class AutoResizeWebViewClient : public WebViewClient {
public:
    // WebViewClient methods
    virtual void didAutoResize(const WebSize& newSize) { m_testData.setSize(newSize); }

    // Local methods
    TestData& testData() { return m_testData; }

private:
    TestData m_testData;
};

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
    FrameTestHelpers::registerMockedURLLoad(m_baseURL, "visible_iframe.html");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "visible_iframe.html");

    webView->setFocus(true);
    webView->setIsActive(true);
    WebFrameImpl* frame = static_cast<WebFrameImpl*>(webView->mainFrame());
    EXPECT_TRUE(frame->frame()->document()->isHTMLDocument());

    WebCore::HTMLDocument* document = static_cast<WebCore::HTMLDocument*>(frame->frame()->document());
    EXPECT_TRUE(document->hasFocus());
    webView->setFocus(false);
    webView->setIsActive(false);
    EXPECT_FALSE(document->hasFocus());
    webView->setFocus(true);
    webView->setIsActive(true);
    EXPECT_TRUE(document->hasFocus());
    webView->setFocus(true);
    webView->setIsActive(false);
    EXPECT_FALSE(document->hasFocus());
    webView->setFocus(false);
    webView->setIsActive(true);
    EXPECT_TRUE(document->hasFocus());

    webView->close();
}

TEST_F(WebViewTest, AutoResizeMinimumSize)
{
    AutoResizeWebViewClient client;
    FrameTestHelpers::registerMockedURLLoad(m_baseURL, "specify_size.html");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "specify_size.html", true, 0, &client);
    client.testData().setWebView(webView);
    FrameTestHelpers::loadFrame(webView->mainFrame(), "javascript:document.getElementById('sizer').style.height = '56px';");
    FrameTestHelpers::loadFrame(webView->mainFrame(), "javascript:document.getElementById('sizer').style.width = '91px';");

    WebFrameImpl* frame = static_cast<WebFrameImpl*>(webView->mainFrame());
    WebCore::FrameView* frameView = frame->frame()->view();
    EXPECT_FALSE(frameView->layoutPending());
    EXPECT_FALSE(frameView->needsLayout());

    WebSize minSize(91, 56);
    WebSize maxSize(403, 302);
    webView->enableAutoResizeMode(true, minSize, maxSize);
    EXPECT_TRUE(frameView->layoutPending());
    EXPECT_TRUE(frameView->needsLayout());
    frameView->layout();

    EXPECT_TRUE(frame->frame()->document()->isHTMLDocument());

    EXPECT_EQ(91, client.testData().width());
    EXPECT_EQ(56, client.testData().height());
    EXPECT_FALSE(client.testData().hasHorizontalScrollbar());
    EXPECT_FALSE(client.testData().hasVerticalScrollbar());

    webView->close();
}

}
