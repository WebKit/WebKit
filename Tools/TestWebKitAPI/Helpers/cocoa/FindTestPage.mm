/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Helpers/cocoa/FindTestPage.h"

#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

static String childPath(const String& parentPath, size_t index)
{
    if (parentPath == "/"_s)
        return makeString("/frame"_s, index);
    return makeString(parentPath, '_', index);
}

static void addFrameSpecRoutes(const FrameSpec& frame, const String& path, HTTPServer::ResponseMap& routes)
{
    StringBuilder html;
    html.append("<!DOCTYPE html>"_s, frame.body);
    for (size_t i = 0; i < frame.children.size(); ++i) {
        auto& child = frame.children[i];
        auto pathForChild = childPath(path, i);
        html.append("<iframe src='https://"_s, child.host, pathForChild, "' "_s, child.iframeAttributes, "></iframe>"_s);
        addFrameSpecRoutes(child, pathForChild, routes);
    }
    routes.add(path, HTTPResponse { { { "Content-Type"_s, "text/html"_s } }, html.toString() });
}

static HTTPServer::ResponseMap routesForFrameTree(const FrameSpec& root, HTTPServer::ResponseMap&& resources)
{
    HTTPServer::ResponseMap routes = WTF::move(resources);
    addFrameSpecRoutes(root, "/"_s, routes);
    return routes;
}

FindTestPage::FindTestPage(const FrameSpec& root, SiteIsolation siteIsolation, RetainPtr<WKWebViewConfiguration> configuration, HTTPServer::ResponseMap&& resources)
    : m_server(routesForFrameTree(root, WTF::move(resources)), HTTPServer::Protocol::HttpsProxy)
{
    load(makeString("https://"_s, root.host, '/'), siteIsolation, WTF::move(configuration));
}

FindTestPage::FindTestPage(const String& path, SiteIsolation siteIsolation, RetainPtr<WKWebViewConfiguration> configuration, HTTPServer::ResponseMap&& resources)
    : m_server(WTF::move(resources), HTTPServer::Protocol::HttpsProxy)
{
    load(makeString("https://a.com"_s, path), siteIsolation, WTF::move(configuration));
}

void FindTestPage::load(const String& url, SiteIsolation siteIsolation, RetainPtr<WKWebViewConfiguration> configuration)
{
    if (configuration)
        [configuration setWebsiteDataStore:m_server.httpsProxyConfiguration().websiteDataStore];
    else
        configuration = m_server.httpsProxyConfiguration();
    std::tie(m_webView, m_navigationDelegate) = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600), siteIsolation == SiteIsolation::On);

    [m_webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url.createNSString().get()]]];
    [m_navigationDelegate waitForDidFinishNavigation];
    [m_webView waitForNextPresentationUpdate];
}

FindTestPage::~FindTestPage() = default;

} // namespace TestWebKitAPI
