/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(UIWEBVIEW)

IGNORE_WARNINGS_BEGIN("deprecated-implementations")

#import "DeprecatedGlobalValues.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "UIKitSPIForTesting.h"
#import "UserInterfaceSwizzler.h"
#import <wtf/RetainPtr.h>

@interface DateTimeInputsTestsLoadingDelegate : NSObject <UIWebViewDelegate>
@end

@implementation DateTimeInputsTestsLoadingDelegate

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    didFinishLoad = true;
}

@end

static UITableViewCell * cellForRowAtIndexPath(id, SEL)
{
    return adoptNS([[UITableViewCell alloc] init]).autorelease();
}

static void runTestWithInputType(NSString *type)
{
    TestWebKitAPI::IPhoneUserInterfaceSwizzler userInterfaceSwizzler;
    InstanceMethodSwizzler overrideCellForRowAtIndexPath { [UITableView class], @selector(cellForRowAtIndexPath:), reinterpret_cast<IMP>(cellForRowAtIndexPath) };

    NSInteger width = 800;
    NSInteger height = 600;

    auto uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, width, height)]);
    auto uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, width, height)]);
    [uiWindow addSubview:uiWebView.get()];

    [uiWebView setKeyboardDisplayRequiresUserAction:NO];

    auto delegate = adoptNS([[DateTimeInputsTestsLoadingDelegate alloc] init]);
    [uiWebView setDelegate:delegate.get()];

    NSString *elementString = [NSString stringWithFormat:@"<input type='%@'>", type];
    [uiWebView loadHTMLString:elementString baseURL:nil];
    TestWebKitAPI::Util::run(&didFinishLoad);

    [uiWebView stringByEvaluatingJavaScriptFromString:@"document.getElementsByTagName('input')[0].focus();"];
    EXPECT_TRUE([[[uiWebView _browserView] inputView] isKindOfClass:[UIDatePicker class]]);
}

TEST(WebKitLegacy, DateInputAccessoryViewTest)
{
    runTestWithInputType(@"date");
}

TEST(WebKitLegacy, DateTimeLocalInputAccessoryViewTest)
{
    runTestWithInputType(@"datetime-local");
}

TEST(WebKitLegacy, MonthInputAccessoryViewTest)
{
    runTestWithInputType(@"month");
}

TEST(WebKitLegacy, TimeInputAccessoryViewTest)
{
    runTestWithInputType(@"time");
}

IGNORE_WARNINGS_END

#endif // HAVE(UIWEBVIEW)
