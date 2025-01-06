/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <WebKit/WKContextMenuItem.h>
#import <WebKit/WKDownloadClient.h>
#import <WebKit/WKDownloadRef.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPageContextMenuClient.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/_WKDownload.h>
#import <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

static bool didFinishLoad;
static bool didDecideDownloadDestination;
static WKPageRef expectedOriginatingPage;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void getContextMenuFromProposedMenu(WKPageRef page, WKArrayRef proposedMenu, WKArrayRef* newMenu, WKHitTestResultRef hitTestResult, WKTypeRef userData, const void* clientInfo)
{
    size_t count = WKArrayGetSize(proposedMenu);
    for (size_t i = 0; i < count; ++i) {
        WKContextMenuItemRef contextMenuItem = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(proposedMenu, i));
        switch (WKContextMenuItemGetTag(contextMenuItem)) {
        case kWKContextMenuItemTagDownloadLinkToDisk:
            // Click "Download Linked File" context menu entry.
            WKPageSelectContextMenuItem(page, contextMenuItem);
            break;
        default:
            break;
        }
    }
}

static WKStringRef decideDestinationWithSuggestedFilename(WKDownloadRef download, WKURLResponseRef, WKStringRef suggestedFilename, const void*)
{
    // Make sure the suggested filename is provided and matches the value of the download attribute in the HTML.
    EXPECT_WK_STREQ("downloadAttributeValue.txt", suggestedFilename);

    WKDownloadCancel(download, nullptr, nullptr);
    didDecideDownloadDestination = true;

    EXPECT_EQ(expectedOriginatingPage, WKDownloadGetOriginatingPage(download));
    EXPECT_TRUE(WKDownloadGetWasUserInitiated(download)); // Download was started via context menu so it is user initiated.

    return Util::toWK("/tmp/WebKitAPITest/ContextMenuDownload").leakRef();
}

static void contextMenuDidCreateDownload(WKPageRef page, WKDownloadRef download, const void* clientInfo)
{
    WKDownloadClientV0 client;
    zeroBytes(client);
    client.base.version = 0;
    client.decideDestinationWithResponse = decideDestinationWithSuggestedFilename;
    WKDownloadSetClient(download, &client.base);
}

// Checks that the HTML download attribute is used as suggested filename when selecting
// the "Download Linked File" item in the context menu.
TEST(WebKit, ContextMenuDownloadHTMLDownloadAttribute)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    zeroBytes(loaderClient);
    loaderClient.base.version = 3;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.contextMenuDidCreateDownload = contextMenuDidCreateDownload;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageContextMenuClientV3 contextMenuClient;
    zeroBytes(contextMenuClient);
    contextMenuClient.base.version = 3;
    contextMenuClient.getContextMenuFromProposedMenu = getContextMenuFromProposedMenu;
    WKPageSetPageContextMenuClient(webView.page(), &contextMenuClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("link-with-download-attribute", "html"));

    expectedOriginatingPage = webView.page();
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    // Right click on link.
    webView.simulateButtonClick(kWKEventMouseButtonRightButton, 50, 50, 0);
    Util::run(&didDecideDownloadDestination);
}

static WKStringRef decideDestinationWithSuggestedFilenameContainingSlashes(WKDownloadRef download, WKURLResponseRef, WKStringRef suggestedFilename, const void*)
{
    // Make sure the suggested filename is provided and matches the value of the download attribute in the HTML, after sanitization.
    EXPECT_WK_STREQ("test1_test2_downloadAttributeValue.txt", suggestedFilename);

    WKDownloadCancel(download, nullptr, nullptr);
    didDecideDownloadDestination = true;

    return Util::toWK("/tmp/WebKitAPITest/ContextMenuDownload").leakRef();
}

static void contextMenuDidCreateDownloadWithSuggestedFilenameContainingSlashes(WKPageRef page, WKDownloadRef download, const void* clientInfo)
{
    WKDownloadClientV0 client;
    zeroBytes(client);
    client.base.version = 0;
    client.decideDestinationWithResponse = decideDestinationWithSuggestedFilenameContainingSlashes;
    WKDownloadSetClient(download, &client.base);
}

TEST(WebKit, ContextMenuDownloadHTMLDownloadAttributeWithSlashes)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    zeroBytes(loaderClient);
    loaderClient.base.version = 3;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.contextMenuDidCreateDownload = contextMenuDidCreateDownloadWithSuggestedFilenameContainingSlashes;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageContextMenuClientV3 contextMenuClient;
    zeroBytes(contextMenuClient);
    contextMenuClient.base.version = 3;
    contextMenuClient.getContextMenuFromProposedMenu = getContextMenuFromProposedMenu;
    WKPageSetPageContextMenuClient(webView.page(), &contextMenuClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("link-with-download-attribute-with-slashes", "html"));

    expectedOriginatingPage = webView.page();
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    // Right click on link.
    webView.simulateButtonClick(kWKEventMouseButtonRightButton, 50, 50, 0);
    Util::run(&didDecideDownloadDestination);
}

}
