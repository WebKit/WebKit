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
#include "FrameTestHelpers.h"

#include "StdLibExtras.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebSettings.h"
#include "platform/WebString.h"
#include "platform/WebURLRequest.h"
#include "platform/WebURLResponse.h"
#include "WebView.h"
#include "WebViewClient.h"
#include <googleurl/src/gurl.h>
#include <webkit/support/webkit_support.h>

namespace WebKit {
namespace FrameTestHelpers {

void registerMockedURLLoad(const std::string& base, const std::string& fileName)
{
    WebURLResponse response;
    response.initialize();
    response.setMIMEType("text/html");

    std::string filePath = webkit_support::GetWebKitRootDir().utf8();
    filePath += "/Source/WebKit/chromium/tests/data/";
    filePath += fileName;

    webkit_support::RegisterMockedURL(GURL(base + fileName), response, WebString::fromUTF8(filePath));
}

void loadFrame(WebFrame* frame, const std::string& url)
{
    WebURLRequest urlRequest;
    urlRequest.initialize();
    urlRequest.setURL(GURL(url));
    frame->loadRequest(urlRequest);
}

class TestWebFrameClient : public WebFrameClient {
};

static WebFrameClient* defaultWebFrameClient()
{
    DEFINE_STATIC_LOCAL(TestWebFrameClient, client, ());
    return &client;
}

class TestWebViewClient : public WebViewClient {
};

static WebViewClient* defaultWebViewClient()
{
    DEFINE_STATIC_LOCAL(TestWebViewClient,  client, ());
    return &client;
}

WebView* createWebViewAndLoad(const std::string& url, bool enableJavascript, WebFrameClient* webFrameClient, WebViewClient* webViewClient)
{
    if (!webFrameClient)
        webFrameClient = defaultWebFrameClient();
    if (!webViewClient)
        webViewClient = defaultWebViewClient();
    WebView* webView = WebView::create(webViewClient);
    webView->settings()->setJavaScriptEnabled(enableJavascript);
    webView->initializeMainFrame(webFrameClient);

    loadFrame(webView->mainFrame(), url);
    webkit_support::ServeAsynchronousMockedRequests();

    return webView;
}

} // namespace FrameTestHelpers
} // namespace WebKit
