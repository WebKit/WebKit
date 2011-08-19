/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "SyntheticBackingScaleFactorWindow.h"
#include <WebKit2/WKURLCF.h>
#include <wtf/RetainPtr.h>

@interface FrameLoadDelegate : NSObject {
    bool* _didFinishLoad;
}

- (id)initWithDidFinishLoadBoolean:(bool*)didFinishLoad;

@end

@implementation FrameLoadDelegate

- (id)initWithDidFinishLoadBoolean:(bool*)didFinishLoad
{
    self = [super init];
    if (!self)
        return nil;

    _didFinishLoad = didFinishLoad;
    return self;
}

- (void)webView:(WebView *)webView didFinishLoadForFrame:(WebFrame *)webFrame
{
    *_didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void* context)
{
    *static_cast<bool*>(const_cast<void*>(context)) = true;
}

static void setPageLoaderClient(WKPageRef page, bool* didFinishLoad)
{
    WKPageLoaderClient loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.version = 0;
    loaderClient.clientInfo = didFinishLoad;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;

    WKPageSetPageLoaderClient(page, &loaderClient);
}

class DynamicDeviceScaleFactor : public ::testing::Test {
public:
    DynamicDeviceScaleFactor();

    template <typename View> void runTest(View);

    bool didFinishLoad;
    NSRect viewFrame;

private:
    RetainPtr<SyntheticBackingScaleFactorWindow> createWindow();

    void loadURL(WebView *, NSURL *);
    void loadURL(WKView *, NSURL *);
};

DynamicDeviceScaleFactor::DynamicDeviceScaleFactor()
    : didFinishLoad(false)
    , viewFrame(NSMakeRect(0, 0, 800, 600))
{
}

RetainPtr<SyntheticBackingScaleFactorWindow> DynamicDeviceScaleFactor::createWindow()
{
    RetainPtr<SyntheticBackingScaleFactorWindow> window(AdoptNS, [[SyntheticBackingScaleFactorWindow alloc] initWithContentRect:viewFrame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    [window.get() setReleasedWhenClosed:NO];
    return window;
}

template <typename View>
void DynamicDeviceScaleFactor::runTest(View view)
{
    EXPECT_FALSE(didFinishLoad);
    loadURL(view, [[NSBundle mainBundle] URLForResource:@"devicePixelRatio" withExtension:@"html"]);
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    EXPECT_JS_EQ(view, "window.devicePixelRatio", "1");
    EXPECT_JS_EQ(view, "devicePixelRatioFromStyle()", "1");

    RetainPtr<SyntheticBackingScaleFactorWindow> window1 = createWindow();
    [window1.get() setBackingScaleFactor:3];

    [[window1.get() contentView] addSubview:view];
    EXPECT_JS_EQ(view, "window.devicePixelRatio", "3");
    EXPECT_JS_EQ(view, "devicePixelRatioFromStyle()", "3");

    RetainPtr<SyntheticBackingScaleFactorWindow> window2 = createWindow();
    [window2.get() setBackingScaleFactor:4];

    [[window2.get() contentView] addSubview:view];
    EXPECT_JS_EQ(view, "window.devicePixelRatio", "4");
    EXPECT_JS_EQ(view, "devicePixelRatioFromStyle()", "4");

    [view removeFromSuperview];
    EXPECT_JS_EQ(view, "window.devicePixelRatio", "1");
    EXPECT_JS_EQ(view, "devicePixelRatioFromStyle()", "1");
}

void DynamicDeviceScaleFactor::loadURL(WebView *webView, NSURL *url)
{
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:url]];
}

void DynamicDeviceScaleFactor::loadURL(WKView *view, NSURL *url)
{
    WKPageLoadURL([view pageRef], adoptWK(WKURLCreateWithCFURL((CFURLRef)url)).get());
}

TEST_F(DynamicDeviceScaleFactor, WebKit)
{
    RetainPtr<WebView> webView(AdoptNS, [[WebView alloc] initWithFrame:viewFrame]);
    RetainPtr<FrameLoadDelegate> delegate(AdoptNS, [[FrameLoadDelegate alloc] initWithDidFinishLoadBoolean:&didFinishLoad]);
    [webView.get() setFrameLoadDelegate:delegate.get()];

    runTest(webView.get());
}

TEST_F(DynamicDeviceScaleFactor, WebKit2)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreate());
    RetainPtr<WKView> view(AdoptNS, [[WKView alloc] initWithFrame:viewFrame contextRef:context.get()]);
    setPageLoaderClient([view.get() pageRef], &didFinishLoad);

    runTest(view.get());
}

} // namespace TestWebKitAPI
