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

#include "FrameTestHelpers.h"
#include "ResourceError.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSearchableFormData.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "v8.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class WebFrameTest : public testing::Test {
public:
    WebFrameTest()
        : m_baseURL("http://www.test.com/"),
          m_chromeURL("chrome://")
    {
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        FrameTestHelpers::registerMockedURLLoad(m_baseURL, fileName);
    }

    void registerMockedChromeURLLoad(const std::string& fileName)
    {
        FrameTestHelpers::registerMockedURLLoad(m_chromeURL, fileName);
    }

protected:
    std::string m_baseURL;
    std::string m_chromeURL;
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html");

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

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html", true);

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

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html");

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

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL.
    WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
    FrameTestHelpers::loadFrame(webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Now retrieve the frames text and see if it was clobbered.
    std::string content = webView->mainFrame()->contentAsText(1024).utf8();
    EXPECT_NE(std::string::npos, content.find("Simulated Chromium History Page"));
    EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

#if ENABLE(VIEWPORT)

class FixedLayoutTestWebViewClient : public WebViewClient {
 public:
    virtual WebRect windowRect() OVERRIDE { return m_windowRect; }
    virtual WebScreenInfo screenInfo() OVERRIDE { return m_screenInfo; }

    WebRect m_windowRect;
    WebScreenInfo m_screenInfo;
};

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag)
{
    registerMockedHttpURLLoad("no_viewport_tag.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.horizontalDPI = 160;
    client.m_windowRect = WebRect(0, 0, viewportWidth, viewportHeight);

    WebView* webView = static_cast<WebView*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client));

    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->settings()->setViewportEnabled(true);
    webView->settings()->setDefaultDeviceScaleFactor(2);
    webView->enableFixedLayoutMode(true);
    webView->layout();

    EXPECT_EQ(2, webView->deviceScaleFactor());
}
#endif

#if ENABLE(GESTURE_EVENTS)
TEST_F(WebFrameTest, FAILS_DivAutoZoomParamsTest)
{
    registerMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_for_auto_zoom_into_div_test.html", true));
    int pageWidth = 640;
    int pageHeight = 480;
    int divPosX = 200;
    int divPosY = 200;
    int divWidth = 200;
    int divHeight = 150;
    WebRect doubleTapPoint(250, 250, 0, 0);
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    float scale;
    WebPoint scroll;

    // Test for Doubletap scaling

    // Tests for zooming in and out without clamping.
    // Set device scale and scale limits so we dont get clamped.
    webViewImpl->setDeviceScaleFactor(4);
    webViewImpl->setPageScaleFactorLimits(0, 4 / webViewImpl->deviceScaleFactor());

    // Test zooming into div.
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPoint, WebViewImpl::DoubleTap, scale, scroll);
    float scaledDivWidth = divWidth * scale;
    float scaledDivHeight = divHeight * scale;
    int hScroll = ((divPosX * scale) - ((pageWidth - scaledDivWidth) / 2)) / scale;
    int vScroll = ((divPosY * scale) - ((pageHeight - scaledDivHeight) / 2)) / scale;
    EXPECT_NEAR(pageWidth / divWidth, scale, 0.1);
    EXPECT_EQ(hScroll, scroll.x);
    EXPECT_EQ(vScroll, scroll.y);

    // Test zoom out to overview scale.
    webViewImpl->applyScrollAndScale(WebSize(scroll.x, scroll.y), scale / webViewImpl->pageScaleFactor());
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPoint, WebViewImpl::DoubleTap, scale, scroll);
    EXPECT_FLOAT_EQ(1, scale);
    EXPECT_EQ(WebPoint(0, 0), scroll);

    // Tests for clamped scaling.
    // Test clamp to device scale:
    webViewImpl->applyScrollAndScale(WebSize(scroll.x, scroll.y), scale / webViewImpl->pageScaleFactor());
    webViewImpl->setDeviceScaleFactor(2.5);
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPoint, WebViewImpl::DoubleTap, scale, scroll);
    EXPECT_FLOAT_EQ(2.5, scale);

    // Test clamp to minimum scale:
    webViewImpl->applyScrollAndScale(WebSize(scroll.x, scroll.y), scale / webViewImpl->pageScaleFactor());
    webViewImpl->setPageScaleFactorLimits(1.5 / webViewImpl->deviceScaleFactor(), 4 / webViewImpl->deviceScaleFactor());
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPoint, WebViewImpl::DoubleTap, scale, scroll);
    EXPECT_FLOAT_EQ(1.5, scale);
    EXPECT_EQ(WebPoint(0, 0), scroll);

    // Test clamp to maximum scale:
    webViewImpl->applyScrollAndScale(WebSize(scroll.x, scroll.y), scale / webViewImpl->pageScaleFactor());
    webViewImpl->setDeviceScaleFactor(4);
    webViewImpl->setPageScaleFactorLimits(0, 3 / webViewImpl->deviceScaleFactor());
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPoint, WebViewImpl::DoubleTap, scale, scroll);
    EXPECT_FLOAT_EQ(3, scale);


    // Test for Non-doubletap scaling
    webViewImpl->setPageScaleFactor(1, WebPoint(0, 0));
    webViewImpl->setDeviceScaleFactor(4);
    webViewImpl->setPageScaleFactorLimits(0, 4 / webViewImpl->deviceScaleFactor());
    // Test zooming into div.
    webViewImpl->computeScaleAndScrollForHitRect(WebRect(250, 250, 10, 10), WebViewImpl::FindInPage, scale, scroll);
    EXPECT_NEAR(pageWidth / divWidth, scale, 0.1);
}
#endif

class TestReloadDoesntRedirectWebFrameClient : public WebFrameClient {
public:
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, const WebURLRequest&, WebNavigationType,
        const WebNode& originatingNode,
        WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        EXPECT_FALSE(isRedirect);
        return WebNavigationPolicyCurrentTab;
    }

    virtual WebURLError cancelledError(WebFrame*, const WebURLRequest& request)
    {
        // Return a dummy error so the DocumentLoader doesn't assert when
        // the reload cancels it.
        WebURLError webURLError;
        webURLError.domain = "";
        webURLError.reason = 1;
        webURLError.isCancellation = true;
        webURLError.unreachableURL = WebURL();
        return webURLError;
    }
};

TEST_F(WebFrameTest, ReloadDoesntSetRedirect)
{
    // Test for case in http://crbug.com/73104. Reloading a frame very quickly
    // would sometimes call decidePolicyForNavigation with isRedirect=true
    registerMockedHttpURLLoad("form.html");

    TestReloadDoesntRedirectWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html", false, &webFrameClient);

    webView->mainFrame()->reload(true);
    // start reload before request is delivered.
    webView->mainFrame()->reload(true);
    webkit_support::ServeAsynchronousMockedRequests();
}

TEST_F(WebFrameTest, ClearFocusedNodeTest)
{
    registerMockedHttpURLLoad("iframe_clear_focused_node_test.html");
    registerMockedHttpURLLoad("autofocus_input_field_iframe.html");

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframe_clear_focused_node_test.html", true));

    // Clear the focused node.
    webViewImpl->clearFocusedNode();

    // Now retrieve the FocusedNode and test it should be null.
    EXPECT_EQ(0, webViewImpl->focusedWebCoreNode());

    webViewImpl->close();
}

// Implementation of WebFrameClient that tracks the v8 contexts that are created
// and destroyed for verification.
class ContextLifetimeTestWebFrameClient : public WebFrameClient {
public:
    struct Notification {
    public:
        Notification(WebFrame* frame, v8::Handle<v8::Context> context, int worldId)
            : frame(frame) ,
              context(v8::Persistent<v8::Context>::New(context)),
              worldId(worldId)
        {
        }

        ~Notification()
        {
            context.Dispose();
        }

        bool Equals(Notification* other)
        {
            return other && frame == other->frame && context == other->context && worldId == other->worldId;
        }

        WebFrame* frame;
        v8::Persistent<v8::Context> context;
        int worldId;
    };

    ~ContextLifetimeTestWebFrameClient()
    {
        reset();
    }

    void reset()
    {
        for (size_t i = 0; i < createNotifications.size(); ++i)
            delete createNotifications[i];

        for (size_t i = 0; i < releaseNotifications.size(); ++i)
            delete releaseNotifications[i];

        createNotifications.clear();
        releaseNotifications.clear();
    }

    std::vector<Notification*> createNotifications;
    std::vector<Notification*> releaseNotifications;

 private:
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup, int worldId) OVERRIDE
    {
        createNotifications.push_back(new Notification(frame, context, worldId));
    }

    virtual void willReleaseScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int worldId) OVERRIDE
    {
        releaseNotifications.push_back(new Notification(frame, context, worldId));
    }
};

TEST_F(WebFrameTest, ContextNotificationsLoadUnload)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    // Load a frame with an iframe, make sure we get the right create notifications.
    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    WebFrame* mainFrame = webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();

    ASSERT_EQ(2u, webFrameClient.createNotifications.size());
    EXPECT_EQ(0u, webFrameClient.releaseNotifications.size());

    ContextLifetimeTestWebFrameClient::Notification* firstCreateNotification = webFrameClient.createNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondCreateNotification = webFrameClient.createNotifications[1];

    EXPECT_EQ(mainFrame, firstCreateNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstCreateNotification->context);
    EXPECT_EQ(0, firstCreateNotification->worldId);

    EXPECT_EQ(childFrame, secondCreateNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondCreateNotification->context);
    EXPECT_EQ(0, secondCreateNotification->worldId);

    // Close the view. We should get two release notifications that are exactly the same as the create ones, in reverse order.
    webView->close();

    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* firstReleaseNotification = webFrameClient.releaseNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondReleaseNotification = webFrameClient.releaseNotifications[1];

    ASSERT_TRUE(firstCreateNotification->Equals(secondReleaseNotification));
    ASSERT_TRUE(secondCreateNotification->Equals(firstReleaseNotification));
}

TEST_F(WebFrameTest, ContextNotificationsReload)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Refresh, we should get two release notifications and two more create notifications.
    webView->mainFrame()->reload(false);
    webkit_support::ServeAsynchronousMockedRequests();
    ASSERT_EQ(4u, webFrameClient.createNotifications.size());
    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());

    // The two release notifications we got should be exactly the same as the first two create notifications.
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      EXPECT_TRUE(webFrameClient.releaseNotifications[i]->Equals(
          webFrameClient.createNotifications[webFrameClient.createNotifications.size() - 3 - i]));
    }

    // The last two create notifications should be for the current frames and context.
    WebFrame* mainFrame = webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();
    ContextLifetimeTestWebFrameClient::Notification* firstRefreshNotification = webFrameClient.createNotifications[2];
    ContextLifetimeTestWebFrameClient::Notification* secondRefreshNotification = webFrameClient.createNotifications[3];

    EXPECT_EQ(mainFrame, firstRefreshNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstRefreshNotification->context);
    EXPECT_EQ(0, firstRefreshNotification->worldId);

    EXPECT_EQ(childFrame, secondRefreshNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondRefreshNotification->context);
    EXPECT_EQ(0, secondRefreshNotification->worldId);

    webView->close();
}

TEST_F(WebFrameTest, ContextNotificationsIsolatedWorlds)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Add an isolated world.
    webFrameClient.reset();

    int isolatedWorldId = 42;
    WebScriptSource scriptSource("hi!");
    int numSources = 1;
    int extensionGroup = 0;
    webView->mainFrame()->executeScriptInIsolatedWorld(isolatedWorldId, &scriptSource, numSources, extensionGroup);

    // We should now have a new create notification.
    ASSERT_EQ(1u, webFrameClient.createNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* notification = webFrameClient.createNotifications[0];
    ASSERT_EQ(isolatedWorldId, notification->worldId);
    ASSERT_EQ(webView->mainFrame(), notification->frame);

    // We don't have an API to enumarate isolated worlds for a frame, but we can at least assert that the context we got is *not* the main world's context.
    ASSERT_NE(webView->mainFrame()->mainWorldScriptContext(), notification->context);

    webView->close();

    // We should have gotten three release notifications (one for each of the frames, plus one for the isolated context).
    ASSERT_EQ(3u, webFrameClient.releaseNotifications.size());

    // And one of them should be exactly the same as the create notification for the isolated context.
    int matchCount = 0;
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      if (webFrameClient.releaseNotifications[i]->Equals(webFrameClient.createNotifications[0]))
        ++matchCount;
    }
    EXPECT_EQ(1, matchCount);
}

TEST_F(WebFrameTest, FindInPage)
{
    registerMockedHttpURLLoad("find.html");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find.html");
    WebFrame* frame = webView->mainFrame();
    const int findIdentifier = 12345;
    WebFindOptions options;

    // Find in a <div> element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar1"), options, false, 0));
    frame->stopFinding(false);
    WebRange range = frame->selectionRange();
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_TRUE(frame->document().focusedNode().isNull());

    // Find in an <input> value.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar2"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("INPUT"), frame->document().focusedNode().nodeName());

    // Find in a <textarea> content.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar3"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("TEXTAREA"), frame->document().focusedNode().nodeName());

    // Find in a contentEditable element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar4"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(0, range.startOffset());
    EXPECT_EQ(4, range.endOffset());
    // "bar4" is surrounded by <span>, but the focusable node should be the parent <div>.
    EXPECT_EQ(WebString::fromUTF8("DIV"), frame->document().focusedNode().nodeName());

    webView->close();
}

} // namespace
