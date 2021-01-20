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
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>

static bool hasTriggerInfo;
static bool wasTriggered;
static uint64_t elementID;
static uint64_t documentID;
static uint64_t pageID;

@interface TestSystemPreviewTriggeredHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestSystemPreviewTriggeredHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body[@"message"] isEqualToString:@"loaded"]) {
        elementID = [message.body[@"elementID"] unsignedIntValue];
        documentID = [message.body[@"documentID"] unsignedIntValue];
        pageID = [message.body[@"pageID"] unsignedIntValue];
        hasTriggerInfo = true;
    } else if ([message.body[@"message"] isEqualToString:@"triggered"]) {
        EXPECT_TRUE([message.body[@"action"] isEqualToString:@"_apple_ar_quicklook_button_tapped"]);
        wasTriggered = true;
    }
}

@end

namespace TestWebKitAPI {

TEST(WebKit, SystemPreviewTriggered)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto messageHandler = adoptNS([[TestSystemPreviewTriggeredHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testSystemPreview"];

    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
    [webView synchronouslyLoadTestPageNamed:@"system-preview-trigger"];
    Util::run(&hasTriggerInfo);

    [webView _triggerSystemPreviewActionOnElement:elementID document:documentID page:pageID];
    Util::run(&wasTriggered);

}

}

#endif // PLATFORM(IOS) && USE(SYSTEM_PREVIEW)
