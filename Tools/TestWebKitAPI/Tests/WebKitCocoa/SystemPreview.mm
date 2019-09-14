/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && USE(SYSTEM_PREVIEW)

#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKViewPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

static bool isLoaded;
static bool isTriggered;
static uint64_t frameID;
static uint64_t pageID;

@interface TestSystemPreviewTriggeredHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestSystemPreviewTriggeredHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body[@"message"] isEqualToString:@"loaded"]) {
        frameID = [message.body[@"frameID"] unsignedIntValue];
        pageID = [message.body[@"pageID"] unsignedIntValue];
        isLoaded = true;
    } else if ([message.body[@"message"] isEqualToString:@"triggered"]) {
        EXPECT_TRUE([message.body[@"action"] isEqualToString:@"_apple_ar_quicklook_button_tapped"]);
        isTriggered = true;
    }
}

@end

namespace TestWebKitAPI {

TEST(WebKit, DISABLED_SystemPreviewTriggered)
{
    auto messageHandler = adoptNS([[TestSystemPreviewTriggeredHandler alloc] init]);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() name:@"testSystemPreview"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"system-preview-trigger" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&isLoaded);

    [webView _triggerSystemPreviewActionOnFrame:frameID page:pageID];
    Util::run(&isTriggered);
}

}

#endif // PLATFORM(IOS) && USE(SYSTEM_PREVIEW)
