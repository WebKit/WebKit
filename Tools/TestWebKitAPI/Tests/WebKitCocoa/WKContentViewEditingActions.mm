/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestUIMenuBuilder.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/LocalizedStrings.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, WKContentViewEditingActions)
{
    [UIPasteboard generalPasteboard].items = @[];

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"rich-and-plain-text"];

    [webView stringByEvaluatingJavaScript:@"selectPlainText()"];

    UIView *contentView = [webView wkContentView];

    __block bool done = false;

    [webView _doAfterNextPresentationUpdate:^ {
        if ([contentView canPerformAction:@selector(copy:) withSender:nil])
            [contentView copy:nil];

        [webView _doAfterNextPresentationUpdate:^ {
            done = true;
        }];
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("Hello world", [[UIPasteboard generalPasteboard] string]);
}

TEST(WebKit, InvokeShareWithoutSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView shareSelection];
    [webView waitForNextPresentationUpdate];
}

TEST(WebKit, CopyInAutoFilledAndViewablePasswordField)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get()]);
    auto *contentView = [webView textInputContentView];
    [webView synchronouslyLoadHTMLString:@"<input type='password' value='hunter2' autofocus /><input type='password' value='hunter2' id='autofill' />"];
    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([contentView canPerformAction:@selector(copy:) withSender:nil]);

    [webView objectByEvaluatingJavaScript:@(R"script(
        let field = document.getElementById('autofill');
        internals.setAutofilledAndViewable(field, true);
        field.select())script")];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([contentView canPerformAction:@selector(copy:) withSender:nil]);
}

#if ENABLE(IMAGE_ANALYSIS)

#if ENABLE(APP_HIGHLIGHTS)

// FIXME when rdar://135224110 is resolved.
TEST(WebKit, DISABLED_AppHighlightsInImageOverlays)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    [configuration _setAppHighlightsEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay" asStringWithBaseURL:[NSURL URLWithString:@"http://webkit.org/simple-image-overlay.html"]];
    [webView stringByEvaluatingJavaScript:@"selectImageOverlay()"];
    [webView waitForNextPresentationUpdate];

    auto menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTagAddHighlightToNewQuickNote()]);
    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTagAddHighlightToCurrentQuickNote()]);

    [webView synchronouslyLoadTestPageNamed:@"simple" asStringWithBaseURL:[NSURL URLWithString:@"http://webkit.org/simple.html"]];
    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTagAddHighlightToNewQuickNote()]);
    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTagAddHighlightToCurrentQuickNote()]);
}

#endif // ENABLE(APP_HIGHLIGHTS)

static BOOL gCanPerformActionWithSenderResult = NO;
static BOOL canPerformActionWithSender(id /* instance */, SEL, SEL /* action */, id /* sender */)
{
    return gCanPerformActionWithSenderResult;
}

TEST(WebKit, CaptureTextFromCamera)
{
    gCanPerformActionWithSenderResult = YES;
    InstanceMethodSwizzler swizzler { UIResponder.class, @selector(canPerformAction:withSender:), reinterpret_cast<IMP>(canPerformActionWithSender) };

    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input style='display: block; font-size: 100px;' value='foo' autofocus><input value='bar' readonly>"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([webView targetForAction:@selector(captureTextFromCamera:) withSender:nil], [webView textInputContentView]);
    EXPECT_TRUE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);

    RetainPtr command = [UIKeyCommand keyCommandWithInput:@"a" modifierFlags:UIKeyModifierCommand action:@selector(captureTextFromCamera:)];
    EXPECT_FALSE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:command.get()]);

    [webView collapseToEnd];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);

    gCanPerformActionWithSenderResult = NO;
    EXPECT_FALSE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);

    gCanPerformActionWithSenderResult = YES;
    [webView objectByEvaluatingJavaScript:@"document.querySelector('input[readonly]').focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);

#if HAVE(UI_EDIT_MENU_INTERACTION)
    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(20, 20) completion:^(BOOL shouldPresentMenu) {
        EXPECT_TRUE(shouldPresentMenu);
        EXPECT_FALSE([webView canPerformAction:@selector(captureTextFromCamera:) withSender:nil]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
#endif
}

#endif // ENABLE(IMAGE_ANALYSIS)

#endif // PLATFORM(IOS_FAMILY)
