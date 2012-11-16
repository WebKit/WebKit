/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "WebPluginContainer.h"

#include "Element.h"
#include "FakeWebPlugin.h"
#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginParams.h"
#include "WebSettings.h"
#include "WebView.h"
#include "WebViewImpl.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class WebPluginContainerTest : public testing::Test {
public:
    WebPluginContainerTest()
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

class TestPluginWebFrameClient : public WebFrameClient {
    virtual WebPlugin* createPlugin(WebFrame* frame, const WebPluginParams& params) OVERRIDE
    {
        if (params.mimeType == WebString::fromUTF8("application/x-webkit-test-webplugin"))
            return new FakeWebPlugin(frame, params);
        return WebFrameClient::createPlugin(frame, params);
    }
};

WebPluginContainer* getWebPluginContainer(WebView* webView, const WebString& id)
{
    WebElement element = webView->mainFrame()->document().getElementById(id);
    return element.pluginContainer();
}

TEST_F(WebPluginContainerTest, WindowToLocalPointTest)
{
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("plugin_container.html"));
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "plugin_container.html", true, new TestPluginWebFrameClient());
    ASSERT(webView);
    webView->settings()->setPluginsEnabled(true);
    webView->resize(WebSize(300, 300));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    WebPluginContainer* pluginContainerOne = getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin"));
    ASSERT(pluginContainerOne);
    WebPoint point1 = pluginContainerOne->windowToLocalPoint(WebPoint(10, 10));
    ASSERT_EQ(0, point1.x);
    ASSERT_EQ(0, point1.y);
    WebPoint point2 = pluginContainerOne->windowToLocalPoint(WebPoint(100, 100));
    ASSERT_EQ(90, point2.x);
    ASSERT_EQ(90, point2.y);

    WebPluginContainer* pluginContainerTwo = getWebPluginContainer(webView, WebString::fromUTF8("rotated-plugin"));
    ASSERT(pluginContainerTwo);
    WebPoint point3 = pluginContainerTwo->windowToLocalPoint(WebPoint(0, 10));
    ASSERT_EQ(10, point3.x);
    ASSERT_EQ(0, point3.y);
    WebPoint point4 = pluginContainerTwo->windowToLocalPoint(WebPoint(-10, 10));
    ASSERT_EQ(10, point4.x);
    ASSERT_EQ(10, point4.y);

    webView->close();
}

}
