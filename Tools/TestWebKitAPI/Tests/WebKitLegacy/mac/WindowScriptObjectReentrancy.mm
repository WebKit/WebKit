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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebScriptObject.h>
#import <WebKit/WebView.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad;
static bool didClearWindowObject;
static RetainPtr<WebScriptObject> clearedWindowObject;

@interface WindowScriptObjectReentrancyDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation WindowScriptObjectReentrancyDelegate

- (void)webView:(WebView *)webView didClearWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame
{
    didClearWindowObject = true;
    clearedWindowObject = windowObject;
}

- (void)webView:(WebView *)webView didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, WindowScriptObjectReentrancy)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr delegate = adoptNS([WindowScriptObjectReentrancyDelegate new]);
    [webView setFrameLoadDelegate:delegate.get()];

    didFinishLoad = false;
    [[webView mainFrame] loadHTMLString:@"<p>No script</p>" baseURL:nil];
    Util::run(&didFinishLoad);

    didClearWindowObject = false;
    clearedWindowObject = nil;

    RetainPtr windowObject = [[webView mainFrame] windowObject];
    EXPECT_TRUE(didClearWindowObject);
    EXPECT_NOT_NULL(windowObject.get());
    EXPECT_EQ(windowObject.get(), clearedWindowObject.get());
    EXPECT_EQ(windowObject.get(), [[webView mainFrame] windowObject]);

    [webView setFrameLoadDelegate:nil];
    clearedWindowObject = nil;
}

} // namespace TestWebKitAPI
