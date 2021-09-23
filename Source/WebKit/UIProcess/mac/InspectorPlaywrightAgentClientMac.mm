/*
 * Copyright (C) 2019 Microsoft Corporation.
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

#import "config.h"
#import "InspectorPlaywrightAgentClientMac.h"

#import <wtf/RefPtr.h>
#import <wtf/text/WTFString.h>
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebsiteDataStore.h"
#import "_WKBrowserInspector.h"
#import "WKProcessPoolInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"

namespace WebKit {

InspectorPlaywrightAgentClientMac::InspectorPlaywrightAgentClientMac(_WKBrowserInspectorDelegate* delegate)
  : delegate_(delegate)
{
}

RefPtr<WebPageProxy> InspectorPlaywrightAgentClientMac::createPage(WTF::String& error, const BrowserContext& browserContext)
{
    auto sessionID = browserContext.dataStore->sessionID();
    WKWebView *webView = [delegate_ createNewPage:sessionID.toUInt64()];
    if (!webView) {
        error = "Internal error: can't create page in given context"_s;
        return nil;
    }
    return [webView _page].get();
}

void InspectorPlaywrightAgentClientMac::closeBrowser()
{
    [delegate_ quit];
}

std::unique_ptr<BrowserContext> InspectorPlaywrightAgentClientMac::createBrowserContext(WTF::String& error, const WTF::String& proxyServer, const WTF::String& proxyBypassList)
{
    _WKBrowserContext* wkBrowserContext = [[delegate_ createBrowserContext:proxyServer WithBypassList:proxyBypassList] autorelease];
    auto browserContext = std::make_unique<BrowserContext>();
    browserContext->processPool = &static_cast<WebProcessPool&>([[wkBrowserContext processPool] _apiObject]);
    browserContext->dataStore = &static_cast<WebsiteDataStore&>([[wkBrowserContext dataStore] _apiObject]);
    return browserContext;
}

void InspectorPlaywrightAgentClientMac::deleteBrowserContext(WTF::String& error, PAL::SessionID sessionID)
{
    [delegate_ deleteBrowserContext:sessionID.toUInt64()];
}

} // namespace WebKit
