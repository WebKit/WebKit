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
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool done;
static bool willSubmitFormValuesCalled;

@interface FormSubmissionDelegate : NSObject <_WKInputDelegate, WKURLSchemeHandler>
@end

@implementation FormSubmissionDelegate

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    EXPECT_TRUE(willSubmitFormValuesCalled);
    EXPECT_STREQ(task.request.URL.absoluteString.UTF8String, "test:///formtarget");
    EXPECT_NULL(task.request.HTTPBody);
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

@end

TEST(WebKit, FormSubmission)
{
    auto delegate = adoptNS([[FormSubmissionDelegate alloc] init]);
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

#endif // WK_API_ENABLED


