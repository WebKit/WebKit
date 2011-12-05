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
#include "WebURLLoaderOptions.h"
#include "WebView.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebURLLoader.h"
#include "platform/WebURLLoaderClient.h"
#include "platform/WebURLRequest.h"
#include "platform/WebURLResponse.h"

#include <googleurl/src/gurl.h>
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class TestWebFrameClient : public WebFrameClient {
    // Return a non-null cancellation error so the WebFrame loaders can shut down without asserting.
    // Make 'reason' non-zero so WebURLError isn't considered null.
    WebURLError cancelledError(WebFrame*, const WebURLRequest& request)
    {
        WebURLError error;
        error.reason = 1;
        error.unreachableURL = request.url();
        return error;
    }
};

class AssociatedURLLoaderTest : public testing::Test,
                                public WebURLLoaderClient {
public:
    AssociatedURLLoaderTest()
        :  m_willSendRequest(false)
        ,  m_didSendData(false)
        ,  m_didReceiveResponse(false)
        ,  m_didReceiveData(false)
        ,  m_didReceiveCachedMetadata(false)
        ,  m_didFinishLoading(false)
        ,  m_didFail(false)
    {
        // Reuse one of the test files from WebFrameTest.
        std::string filePath = webkit_support::GetWebKitRootDir().utf8();
        filePath += "/Source/WebKit/chromium/tests/data/iframes_test.html";
        m_frameFilePath = WebString::fromUTF8(filePath);
    }

    void SetUp()
    {
        m_webView = WebView::create(0);
        m_webView->initializeMainFrame(&m_webFrameClient);

        // Load the frame before trying to load resources.
        GURL url = GURL("http://www.test.com/iframes_test.html");
        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");
        webkit_support::RegisterMockedURL(url, response, m_frameFilePath);

        WebURLRequest request;
        request.initialize();
        request.setURL(url);
        m_webView->mainFrame()->loadRequest(request);
        serveRequests();

        webkit_support::UnregisterMockedURL(url);
    }

    void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
        m_webView->close();
    }

    void serveRequests()
    {
        webkit_support::ServeAsynchronousMockedRequests();
    }

    WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions options = WebURLLoaderOptions())
    {
        return m_webView->mainFrame()->createAssociatedURLLoader(options);
    }

    // WebURLLoaderClient implementation.
    void willSendRequest(WebURLLoader* loader, WebURLRequest& newRequest, const WebURLResponse& redirectResponse)
    {
        m_willSendRequest = true;
        EXPECT_EQ(m_expectedLoader, loader);
        EXPECT_EQ(m_expectedNewRequest.url(), newRequest.url());
        EXPECT_EQ(m_expectedRedirectResponse.url(), redirectResponse.url());
        EXPECT_EQ(m_expectedRedirectResponse.mimeType(), redirectResponse.mimeType());
        EXPECT_EQ(m_expectedRedirectResponse.httpStatusCode(), redirectResponse.httpStatusCode());
    }

    void didSendData(WebURLLoader* loader, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
    {
        m_didSendData = true;
        EXPECT_EQ(m_expectedLoader, loader);
    }

    void didReceiveResponse(WebURLLoader* loader, const WebURLResponse& response)
    {
        m_didReceiveResponse = true;
        EXPECT_EQ(m_expectedLoader, loader);
        EXPECT_EQ(m_expectedResponse.url(), response.url());
        EXPECT_EQ(m_expectedResponse.httpStatusCode(), response.httpStatusCode());
    }

    void didDownloadData(WebURLLoader* loader, int dataLength)
    {
        m_didDownloadData = true;
        EXPECT_EQ(m_expectedLoader, loader);
    }

    void didReceiveData(WebURLLoader* loader, const char* data, int dataLength, int encodedDataLength)
    {
        m_didReceiveData = true;
        EXPECT_EQ(m_expectedLoader, loader);
        EXPECT_TRUE(data);
        EXPECT_GT(dataLength, 0);
    }

    void didReceiveCachedMetadata(WebURLLoader* loader, const char* data, int dataLength)
    {
        m_didReceiveCachedMetadata = true;
        EXPECT_EQ(m_expectedLoader, loader);
    }

    void didFinishLoading(WebURLLoader* loader, double finishTime)
    {
        m_didFinishLoading = true;
        EXPECT_EQ(m_expectedLoader, loader);
    }

    void didFail(WebURLLoader* loader, const WebURLError& error)
    {
        m_didFail = true;
        EXPECT_EQ(m_expectedLoader, loader);
        webkit_support::QuitMessageLoop();
    }

    void CheckMethodFails(const char* unsafeMethod)
    {
        WebURLRequest request;
        request.initialize();
        request.setURL(GURL("http://www.test.com/success.html"));
        request.setHTTPMethod(WebString::fromUTF8(unsafeMethod));
        WebURLLoaderOptions options;
        options.untrustedHTTP = true;
        CheckFails(request, options);
    }

    void CheckHeaderFails(const char* headerField)
    {
        CheckHeaderFails(headerField, "foo");
    }

    void CheckHeaderFails(const char* headerField, const char* headerValue)
    {
        WebURLRequest request;
        request.initialize();
        request.setURL(GURL("http://www.test.com/success.html"));
        request.setHTTPHeaderField(WebString::fromUTF8(headerField), WebString::fromUTF8(headerValue));
        WebURLLoaderOptions options;
        options.untrustedHTTP = true;
        CheckFails(request, options);
    }

    void CheckFails(const WebURLRequest& request, WebURLLoaderOptions options = WebURLLoaderOptions())
    {
        m_expectedLoader = createAssociatedURLLoader(options);
        EXPECT_TRUE(m_expectedLoader);
        m_didFail = false;
        m_expectedLoader->loadAsynchronously(request, this);
        // Failure should not be reported synchronously.
        EXPECT_FALSE(m_didFail);
        // Allow the loader to return the error.
        webkit_support::RunMessageLoop();
        EXPECT_TRUE(m_didFail);
    }

protected:
    WebString m_frameFilePath;
    TestWebFrameClient m_webFrameClient;
    WebView* m_webView;

    WebURLLoader* m_expectedLoader;
    WebURLResponse m_expectedResponse;
    WebURLRequest m_expectedNewRequest;
    WebURLResponse m_expectedRedirectResponse;
    bool m_willSendRequest;
    bool m_didSendData;
    bool m_didReceiveResponse;
    bool m_didDownloadData;
    bool m_didReceiveData;
    bool m_didReceiveCachedMetadata;
    bool m_didFinishLoading;
    bool m_didFail;
};

// Test a successful URL load.
TEST_F(AssociatedURLLoaderTest, Success)
{
    GURL url = GURL("http://www.test.com/success.html");
    WebURLRequest request;
    request.initialize();
    request.setURL(url);

    m_expectedResponse = WebURLResponse();
    m_expectedResponse.initialize();
    m_expectedResponse.setMIMEType("text/html");
    webkit_support::RegisterMockedURL(url, m_expectedResponse, m_frameFilePath);

    m_expectedLoader = createAssociatedURLLoader();
    EXPECT_TRUE(m_expectedLoader);
    m_expectedLoader->loadAsynchronously(request, this);
    serveRequests();
    EXPECT_TRUE(m_didReceiveResponse);
    EXPECT_TRUE(m_didReceiveData);
    EXPECT_TRUE(m_didFinishLoading);
}

// Test that the same-origin restriction is the default.
TEST_F(AssociatedURLLoaderTest, SameOriginRestriction)
{
    // This is cross-origin since the frame was loaded from www.test.com.
    GURL url = GURL("http://www.other.com/failure.html");
    WebURLRequest request;
    request.initialize();
    request.setURL(url);
    CheckFails(request);
}

// Test a successful cross-origin load.
TEST_F(AssociatedURLLoaderTest, CrossOriginSuccess)
{
    // This is cross-origin since the frame was loaded from www.test.com.
    GURL url = GURL("http://www.other.com/success.html");
    WebURLRequest request;
    request.initialize();
    request.setURL(url);

    m_expectedResponse = WebURLResponse();
    m_expectedResponse.initialize();
    m_expectedResponse.setMIMEType("text/html");
    webkit_support::RegisterMockedURL(url, m_expectedResponse, m_frameFilePath);

    WebURLLoaderOptions options;
    options.crossOriginRequestPolicy = WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
    m_expectedLoader = createAssociatedURLLoader(options);
    EXPECT_TRUE(m_expectedLoader);
    m_expectedLoader->loadAsynchronously(request, this);
    serveRequests();
    EXPECT_TRUE(m_didReceiveResponse);
    EXPECT_TRUE(m_didReceiveData);
    EXPECT_TRUE(m_didFinishLoading);
}

// Test that untrusted loads can't use a forbidden method.
TEST_F(AssociatedURLLoaderTest, UntrustedCheckMethods)
{
    // Check non-token method fails.
    CheckMethodFails("GET()");
    CheckMethodFails("POST\x0d\x0ax-csrf-token:\x20test1234");

    // Forbidden methods should fail regardless of casing.
    CheckMethodFails("CoNneCt");
    CheckMethodFails("TrAcK");
    CheckMethodFails("TrAcE");
}

// Test that untrusted loads can't use a forbidden header field.
TEST_F(AssociatedURLLoaderTest, UntrustedCheckHeaders)
{
    // Check non-token header fails.
    CheckHeaderFails("foo()");

    // Check forbidden headers fail.
    CheckHeaderFails("accept-charset");
    CheckHeaderFails("accept-encoding");
    CheckHeaderFails("connection");
    CheckHeaderFails("content-length");
    CheckHeaderFails("cookie");
    CheckHeaderFails("cookie2");
    CheckHeaderFails("content-transfer-encoding");
    CheckHeaderFails("date");
    CheckHeaderFails("expect");
    CheckHeaderFails("host");
    CheckHeaderFails("keep-alive");
    CheckHeaderFails("origin");
    CheckHeaderFails("referer");
    CheckHeaderFails("te");
    CheckHeaderFails("trailer");
    CheckHeaderFails("transfer-encoding");
    CheckHeaderFails("upgrade");
    CheckHeaderFails("user-agent");
    CheckHeaderFails("via");

    CheckHeaderFails("proxy-");
    CheckHeaderFails("proxy-foo");
    CheckHeaderFails("sec-");
    CheckHeaderFails("sec-foo");

    // Check that validation is case-insensitive.
    CheckHeaderFails("AcCePt-ChArSeT");
    CheckHeaderFails("ProXy-FoO");

    // Check invalid header values.
    CheckHeaderFails("foo", "bar\x0d\x0ax-csrf-token:\x20test1234");
}

}
