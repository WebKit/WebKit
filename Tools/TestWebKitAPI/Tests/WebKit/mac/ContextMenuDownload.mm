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

#include "config.h"
#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit/WKContextMenuItem.h>
#include <WebKit/WKDownload.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageContextMenuClient.h>
#include <WebKit/WKPreferencesPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/_WKDownload.h>

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

static WKStringRef decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef download, WKStringRef suggestedFilename, bool*, const void*)
{
    // Make sure the suggested filename is provided and matches the value of the download attribute in the HTML.
    EXPECT_WK_STREQ("downloadAttributeValue.txt", suggestedFilename);

    WKDownloadCancel(download);
    didDecideDownloadDestination = true;

    EXPECT_EQ(expectedOriginatingPage, WKDownloadGetOriginatingPage(download));
    EXPECT_TRUE(WKDownloadGetWasUserInitiated(download)); // Download was started via context menu so it is user initiated.

    return Util::toWK("/tmp/WebKitAPITest/ContextMenuDownload").leakRef();
}

// Checks that the HTML download attribute is used as suggested filename when selecting
// the "Download Linked File" item in the context menu.
TEST(WebKit, ContextMenuDownloadHTMLDownloadAttribute)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    WKContextDownloadClientV0 client;
    memset(&client, 0, sizeof(client));
    client.base.version = 0;
    client.decideDestinationWithSuggestedFilename = decideDestinationWithSuggestedFilename;
    WKContextSetDownloadClient(context.get(), &client.base);

    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("MyGroup").get()));
    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageContextMenuClientV3 contextMenuClient;
    memset(&contextMenuClient, 0, sizeof(contextMenuClient));
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

static WKStringRef decideDestinationWithSuggestedFilenameContainingSlashes(WKContextRef, WKDownloadRef download, WKStringRef suggestedFilename, bool*, const void*)
{
    // Make sure the suggested filename is provided and matches the value of the download attribute in the HTML, after sanitization.
    EXPECT_WK_STREQ("test1_test2_downloadAttributeValue.txt", suggestedFilename);

    WKDownloadCancel(download);
    didDecideDownloadDestination = true;

    return Util::toWK("/tmp/WebKitAPITest/ContextMenuDownload").leakRef();
}

TEST(WebKit, ContextMenuDownloadHTMLDownloadAttributeWithSlashes)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    WKContextDownloadClientV0 client;
    memset(&client, 0, sizeof(client));
    client.base.version = 0;
    client.decideDestinationWithSuggestedFilename = decideDestinationWithSuggestedFilenameContainingSlashes;
    WKContextSetDownloadClient(context.get(), &client.base);

    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("MyGroup").get()));
    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageContextMenuClientV3 contextMenuClient;
    memset(&contextMenuClient, 0, sizeof(contextMenuClient));
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
