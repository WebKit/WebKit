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

#import "config.h"
#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#import "BundleRangeHandleProtocol.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool didGetTextFromBodyRange;
static bool didGetBodyInnerHTMLAfterDetectingData;

@interface BundleRangeHandleRemoteObject : NSObject <BundleRangeHandleProtocol>
@end

@implementation BundleRangeHandleRemoteObject

- (void)textFromBodyRange:(NSString *)text
{
    didGetTextFromBodyRange = true;
    EXPECT_WK_STREQ("Visit webkit.org or email webkit-dev@lists.webkit.org.", text);
}

- (void)bodyInnerHTMLAfterDetectingData:(NSString *)text
{
    didGetBodyInnerHTMLAfterDetectingData = true;
    EXPECT_WK_STREQ("Visit <a href=\"http://webkit.org\" dir=\"ltr\" x-apple-data-detectors=\"true\" x-apple-data-detectors-type=\"link\" x-apple-data-detectors-result=\"0\">webkit.org</a>   <em>  or</em> email <a href=\"mailto:webkit-dev@lists.webkit.org\" dir=\"ltr\" x-apple-data-detectors=\"true\" x-apple-data-detectors-type=\"link\" x-apple-data-detectors-result=\"1\">webkit-dev@lists.webkit.org</a>.", text);
}

@end

// FIXME: Re-enable this test once webkit.org/b/167594 is fixed.
TEST(WebKit, DISABLED_WKWebProcessPlugInRangeHandle)
{
    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleRangeHandlePlugIn"]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<BundleRangeHandleRemoteObject> object = adoptNS([[BundleRangeHandleRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleRangeHandleProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView loadHTMLString:@"Visit webkit.org   <em>  or</em> email webkit-dev@lists.webkit.org." baseURL:nil];

    TestWebKitAPI::Util::run(&didGetTextFromBodyRange);
#if PLATFORM(IOS_FAMILY)
    TestWebKitAPI::Util::run(&didGetBodyInnerHTMLAfterDetectingData);
#endif
}

#endif
