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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFocusedElementInfo.h>
#import <WebKit/_WKFormInputSession.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool done;
static bool willSubmitFormValuesCalled;

@interface InputDelegate : NSObject <_WKInputDelegate, WKURLSchemeHandler>
@property (nonatomic, copy) BOOL(^shouldStartInputSessionHandler)(id <_WKFocusedElementInfo>);
@end

@implementation InputDelegate {
    BlockPtr<BOOL(id <_WKFocusedElementInfo>)> _shouldStartInputSessionHandler;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    EXPECT_TRUE(willSubmitFormValuesCalled);
    EXPECT_STREQ(task.request.URL.absoluteString.UTF8String, "test:///formtarget");
    EXPECT_NOT_NULL(task.request.HTTPBody);
    EXPECT_EQ(task.request.HTTPBody.length, 62u);
    EXPECT_STREQ(static_cast<const char*>(task.request.HTTPBody.bytes), "testname1=testvalue1&testname2=testvalue2&testname3=testvalue3");
    done = true;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

- (void)_webView:(WKWebView *)webView willSubmitFormValues:(NSDictionary *)values userObject:(NSObject <NSSecureCoding> *)userObject submissionHandler:(void (^)(void))submissionHandler
{
    EXPECT_EQ(values.count, 2u);
    EXPECT_STREQ([[values objectForKey:@"testname1"] UTF8String], "testvalue1");
    EXPECT_STREQ([[values objectForKey:@"testname2"] UTF8String], "testvalue2");
    willSubmitFormValuesCalled = true;
    submissionHandler();
}

- (BOOL)_webView:(WKWebView *)webView focusShouldStartInputSession:(id <_WKFocusedElementInfo>)info
{
    if (_shouldStartInputSessionHandler)
        return _shouldStartInputSessionHandler(info);
    return [info isUserInitiated];
}

- (BOOL(^)(id <_WKFocusedElementInfo>))shouldStartInputSessionHandler
{
    return _shouldStartInputSessionHandler.get();
}

- (void)setShouldStartInputSessionHandler:(BOOL(^)(id <_WKFocusedElementInfo>))handler
{
    _shouldStartInputSessionHandler = makeBlockPtr(handler);
}

@end

TEST(WebKit, FormSubmission)
{
    auto delegate = adoptNS([[InputDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setInputDelegate:delegate.get()];
    [webView loadHTMLString:@"<body onload='document.getElementById(\"formID\").submit()'><form id='formID' method='post' action='test:///formtarget'>"
        "<input type='text' name='testname1' value='testvalue1'/>"
        "<input type='password' name='testname2' value='testvalue2'/>"
        "<input type='hidden' name='testname3' value='testvalue3'/>"
    "</form></body>" baseURL:nil];
    TestWebKitAPI::Util::run(&done);
}

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, FocusedElementInfo)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[InputDelegate alloc] init]);
    [webView _setInputDelegate:delegate.get()];

    __block RetainPtr<id <_WKFocusedElementInfo>> currentElement;
    [delegate setShouldStartInputSessionHandler:^BOOL(id<_WKFocusedElementInfo> element) {
        currentElement = element;
        return NO;
    }];

    [webView synchronouslyLoadHTMLString:@"<label for='foo'>bar</label><input id='foo'>"];
    [webView stringByEvaluatingJavaScript:@"foo.focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("", [currentElement placeholder]);
    EXPECT_WK_STREQ("bar", [currentElement label]);

    [webView synchronouslyLoadHTMLString:@"<input placeholder='bar'>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('input').focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("bar", [currentElement placeholder]);
    EXPECT_WK_STREQ("", [currentElement label]);

    [webView synchronouslyLoadHTMLString:@"<label for='baz'>garply</label><select id='baz'></select>"];
    [webView stringByEvaluatingJavaScript:@"baz.focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("", [currentElement placeholder]);
    EXPECT_WK_STREQ("garply", [currentElement label]);

    [webView synchronouslyLoadHTMLString:@"<label for='foo' style='display: none'>bar</label><label for='foo'></label><input id='foo'><label for='foo'>garply</label>"];
    [webView stringByEvaluatingJavaScript:@"foo.focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("", [currentElement placeholder]);
    EXPECT_WK_STREQ("garply", [currentElement label]);
}

#endif // PLATFORM(IOS_FAMILY)

#endif // WK_API_ENABLED


