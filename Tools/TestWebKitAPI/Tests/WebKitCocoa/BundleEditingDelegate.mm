/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) // FIXME: https://bugs.webkit.org/show_bug.cgi?id=165384 REGRESSION: [ios-simulator] API test WebKit2.WKWebProcessPlugInEditingDelegate crashing

#import "BundleEditingDelegateProtocol.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool shouldInsertTextCalled;
static bool willWriteToPasteboard;
static bool didWriteToPasteboard;

@interface BundleEditingDelegateRemoteObject : NSObject <BundleEditingDelegateProtocol>
@end

@implementation BundleEditingDelegateRemoteObject

- (void)shouldInsertText:(NSString *)text replacingRange:(NSString *)rangeAsString givenAction:(WKEditorInsertAction)action
{
    EXPECT_WK_STREQ("hello", text);
    EXPECT_WK_STREQ("something", rangeAsString);
    EXPECT_EQ(WKEditorInsertActionPasted, action);
    shouldInsertTextCalled = true;
}

- (void)willWriteToPasteboard:(NSString *)rangeAsString
{
    willWriteToPasteboard = true;
    EXPECT_STREQ("something", rangeAsString.UTF8String);
}

- (void)didWriteToPasteboard
{
    didWriteToPasteboard = true;
}

@end

TEST(WebKit, WKWebProcessPlugInEditingDelegate)
{
    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"]);
    [[configuration processPool] _setObject:[NSNumber numberWithBool:NO] forBundleParameter:@"EditingDelegateShouldInsertText"];
    
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='-webkit-user-modify: read-write-plaintext-only'>Just something to copy <script> var textNode = document.body.firstChild; document.getSelection().setBaseAndExtent(textNode, 5, textNode, 14) </script>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<BundleEditingDelegateRemoteObject> object = adoptNS([[BundleEditingDelegateRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleEditingDelegateProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView performSelector:@selector(copy:) withObject:nil];

    TestWebKitAPI::Util::run(&didWriteToPasteboard);

    NSString *copiedString = nil;
#if PLATFORM(MAC)
    copiedString = [[NSPasteboard generalPasteboard] stringForType:@"org.webkit.data"];
#elif PLATFORM(IOS_FAMILY)
    copiedString = [[[NSString alloc] initWithData:[[UIPasteboard generalPasteboard] dataForPasteboardType:@"org.webkit.data"] encoding:NSUTF8StringEncoding] autorelease];
#endif
    EXPECT_WK_STREQ("hello", copiedString);

#if PLATFORM(MAC)
    [[NSPasteboard generalPasteboard] setString:copiedString forType:@"public.utf8-plain-text"];
#elif PLATFORM(IOS_FAMILY)
    [[UIPasteboard generalPasteboard] setValue:copiedString forPasteboardType:@"public.utf8-plain-text"];
#endif
    [webView performSelector:@selector(paste:) withObject:nil];

    TestWebKitAPI::Util::run(&shouldInsertTextCalled);

    __block bool doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"document.body.firstChild.textContent" completionHandler:^(id _Nullable value, NSError * _Nullable error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ("Just something to copy ", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];

    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
}

#endif

#endif
