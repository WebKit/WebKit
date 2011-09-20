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
#include "WebScriptSource.h"
#include "WebSearchableFormData.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebURLResponse.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
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

class TestWebViewClient : public WebViewClient {
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
        EXPECT_FALSE(isRedirect);
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

TEST_F(WebFrameTest, ClearFocusedNodeTest)
{
    registerMockedHttpURLLoad("iframe_clear_focused_node_test.html");
    registerMockedHttpURLLoad("autofocus_input_field_iframe.html");

    // Create and initialize the WebView.
    TestWebFrameClient webFrameClient;
    TestWebViewClient webviewClient;
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(WebView::create(&webviewClient));
    webViewImpl->settings()->setJavaScriptEnabled(true);
    webViewImpl->initializeMainFrame(&webFrameClient);

    loadHttpFrame(webViewImpl->mainFrame(), "iframe_clear_focused_node_test.html");
    serveRequests();

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
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int worldId)
    {
        createNotifications.push_back(new Notification(frame, context, worldId));
    }

    virtual void willReleaseScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int worldId)
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
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);
    loadHttpFrame(webView->mainFrame(), "context_notifications_test.html");
    serveRequests();

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
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);
    loadHttpFrame(webView->mainFrame(), "context_notifications_test.html");
    serveRequests();

    // Refresh, we should get two release notifications and two more create notifications.
    webView->mainFrame()->reload(false);
    serveRequests();
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
    WebView* webView = WebView::create(0);
    webView->settings()->setJavaScriptEnabled(true);
    webView->initializeMainFrame(&webFrameClient);
    loadHttpFrame(webView->mainFrame(), "context_notifications_test.html");
    serveRequests();

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

} // namespace
