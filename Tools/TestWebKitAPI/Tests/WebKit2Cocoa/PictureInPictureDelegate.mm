/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED && PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKPagePrivateMac.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKSerializedScriptValue.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLCF.h>
#import <WebKit/WKView.h>
#import <WebKit/WKViewPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool receivedLoadedMessage;

static bool hasVideoInPictureInPictureValue;
static bool hasVideoInPictureInPictureCalled;

static bool onLoadCompleted = false;
static bool fetchOnLoadedCompletedDone = false;

static void onLoadedCompletedCallback(WKSerializedScriptValueRef serializedResultValue, WKErrorRef error, void*)
{
    EXPECT_NULL(error);
    
    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    
    JSValueRef resultValue = WKSerializedScriptValueDeserialize(serializedResultValue, scriptContext, 0);
    EXPECT_TRUE(JSValueIsBoolean(scriptContext, resultValue));
    
    fetchOnLoadedCompletedDone = true;
    onLoadCompleted = JSValueToBoolean(scriptContext, resultValue);
    
    JSGlobalContextRelease(scriptContext);
}

static void waitUntilOnLoadIsCompleted(WKPageRef page)
{
    onLoadCompleted = false;
    while (!onLoadCompleted) {
        fetchOnLoadedCompletedDone = false;
        WKPageRunJavaScriptInMainFrame(page, TestWebKitAPI::Util::toWK("window.onloadcompleted !== undefined").get(), 0, onLoadedCompletedCallback);
        TestWebKitAPI::Util::run(&fetchOnLoadedCompletedDone);
    }
}

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    receivedLoadedMessage = true;
}

static void setHasVideoInPictureInPicture(WKPageRef, bool hasVideoInPictureInPicture, const void* context)
{
    hasVideoInPictureInPictureValue = hasVideoInPictureInPicture;
    hasVideoInPictureInPictureCalled = true;
}

namespace TestWebKitAPI {
    
TEST(PictureInPicture, WKPageUIClient)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreate());
    WKRetainPtr<WKPageGroupRef> pageGroup(AdoptWK, WKPageGroupCreateWithIdentifier(Util::toWK("PictureInPicture").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetFullScreenEnabled(preferences, true);
    WKPreferencesSetAllowsPictureInPictureMediaPlayback(preferences, true);
    
    PlatformWebView webView(context.get(), pageGroup.get());
    
    WKPageUIClientV10 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 10;
    uiClient.base.clientInfo = NULL;
    uiClient.setHasVideoInPictureInPicture = setHasVideoInPictureInPicture;
    WKPageSetPageUIClient(webView.page(), &uiClient.base);
    
    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0 , sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    WKPageSetPageLoaderClient(webView.page(), &loaderClient.base);
    
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [window.get() makeKeyAndOrderFront:nil];
    [window.get().contentView addSubview:webView.platformView()];
    
    receivedLoadedMessage = false;
    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("PictureInPictureDelegate", "html"));
    WKPageLoadURL(webView.page(), url.get());
    TestWebKitAPI::Util::run(&receivedLoadedMessage);
    waitUntilOnLoadIsCompleted(webView.page());
    
    hasVideoInPictureInPictureValue = false;
    hasVideoInPictureInPictureCalled = false;
    webView.simulateButtonClick(kWKEventMouseButtonLeftButton, 5, 5, 0);
    TestWebKitAPI::Util::run(&hasVideoInPictureInPictureCalled);
    ASSERT_TRUE(hasVideoInPictureInPictureValue);
    
    sleep(1); // Wait for PIPAgent to launch, or it won't call -pipDidClose: callback.
    
    hasVideoInPictureInPictureCalled = false;
    webView.simulateButtonClick(kWKEventMouseButtonLeftButton, 5, 5, 0);
    TestWebKitAPI::Util::run(&hasVideoInPictureInPictureCalled);
    ASSERT_FALSE(hasVideoInPictureInPictureValue);
}

} // namespace TestWebKitAPI

#endif
