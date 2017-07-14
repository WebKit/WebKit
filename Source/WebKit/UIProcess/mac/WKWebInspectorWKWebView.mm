/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
#import "WKWebInspectorWKWebView.h"

#if PLATFORM(MAC) && WK_API_ENABLED

#import "WKAPICast.h"
#import "WKInspectorPrivateMac.h"
#import "WKMutableArray.h"
#import "WKOpenPanelParametersRef.h"
#import "WKOpenPanelResultListener.h"
#import "WKRetainPtr.h"
#import "WKURLCF.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"

using namespace WebKit;

namespace WebKit {

static WKRect getWindowFrame(WKPageRef, const void* clientInfo)
{
    WKWebInspectorWKWebView *inspectorWKWebView = static_cast<WKWebInspectorWKWebView *>(const_cast<void*>(clientInfo));
    ASSERT(inspectorWKWebView);

    if (!inspectorWKWebView.window)
        return WKRectMake(0, 0, 0, 0);

    NSRect frame = inspectorWKWebView.frame;
    return WKRectMake(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
}

static void setWindowFrame(WKPageRef, WKRect frame, const void* clientInfo)
{
    WKWebInspectorWKWebView *inspectorWKWebView = static_cast<WKWebInspectorWKWebView *>(const_cast<void*>(clientInfo));
    ASSERT(inspectorWKWebView);

    if (!inspectorWKWebView.window)
        return;

    [inspectorWKWebView.window setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height) display:YES];
}

static unsigned long long exceededDatabaseQuota(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKStringRef, WKStringRef, unsigned long long, unsigned long long, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, const void*)
{
    return std::max<unsigned long long>(expectedUsage, currentDatabaseUsage * 1.25);
}

static void runOpenPanel(WKPageRef page, WKFrameRef frame, WKOpenPanelParametersRef parameters, WKOpenPanelResultListenerRef listener, const void* clientInfo)
{
    WKWebInspectorWKWebView *inspectorWKWebView = static_cast<WKWebInspectorWKWebView *>(const_cast<void*>(clientInfo));
    ASSERT(inspectorWKWebView);

    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection:WKOpenPanelParametersGetAllowsMultipleFiles(parameters)];

    WKRetain(listener);

    auto completionHandler = ^(NSInteger result) {
        if (result == NSModalResponseOK) {
            WKRetainPtr<WKMutableArrayRef> fileURLs = adoptWK(WKMutableArrayCreate());

            for (NSURL* nsURL in [openPanel URLs]) {
                WKRetainPtr<WKURLRef> wkURL = adoptWK(WKURLCreateWithCFURL(reinterpret_cast<CFURLRef>(nsURL)));
                WKArrayAppendItem(fileURLs.get(), wkURL.get());
            }

            WKOpenPanelResultListenerChooseFiles(listener, fileURLs.get());
        } else
            WKOpenPanelResultListenerCancel(listener);

        WKRelease(listener);
    };

    if (inspectorWKWebView.window)
        [openPanel beginSheetModalForWindow:inspectorWKWebView.window completionHandler:completionHandler];
    else
        completionHandler([openPanel runModal]);
}

} // namespace WebKit

@implementation WKWebInspectorWKWebView

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    [self _setAutomaticallyAdjustsContentInsets:NO];

    WKPageUIClientV2 uiClient = {
        { 2, self },
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // closePage
        0, // takeFocus
        0, // focus
        0, // unfocus
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked_deprecatedForUseWithV0
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // areToolbarsVisible
        0, // setToolbarsVisible
        0, // isMenuBarVisible
        0, // setMenuBarVisible
        0, // isStatusBarVisible
        0, // setStatusBarVisible
        0, // isResizable
        0, // setResizable
        getWindowFrame,
        setWindowFrame,
        0, // runBeforeUnloadConfirmPanel
        0, // didDraw
        0, // pageDidScroll
        exceededDatabaseQuota,
        runOpenPanel,
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // runModal
        0, // unused
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createPage
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        0, // showColorPicker
        0, // hideColorPicker
        0, // unavailablePluginButtonClicked
    };

    WebPageProxy* inspectorPage = self->_page.get();
    WKPageSetPageUIClient(toAPI(inspectorPage), &uiClient.base);

    return self;
}

- (NSInteger)tag
{
    return WKInspectorViewTag;
}

@end

#endif // PLATFORM(MAC)
