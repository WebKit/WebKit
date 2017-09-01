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

#include "config.h"

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <atomic>
#import <thread>
#import <wtf/RetainPtr.h>

@interface WebScriptDescriptionTest : NSObject <WebFrameLoadDelegate>
@end

static bool didFinishLoad;
static bool failedObjCDescription { false };

void nsObjectDescriptionTest(NSObject *object)
{
    NSError *error = nullptr;
    NSRegularExpression *regexp = [NSRegularExpression regularExpressionWithPattern:@"<NSObject: 0x[0-9|a-f]+>" options:0 error:&error];
    for (unsigned i = 0; i < 100000; ++i) {
        NSString *description = [object description];
        if ([regexp numberOfMatchesInString:description options:0 range:NSMakeRange(0, [description length])] != 1)
            failedObjCDescription = true;
    }
}

@implementation WebScriptDescriptionTest

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, WebScriptObjectDescription)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<WebScriptDescriptionTest> testController = adoptNS([WebScriptDescriptionTest new]);
    NSObject *object = [[NSObject alloc] init];

    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle]
        URLForResource:@"WebScriptObjectDescription" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didFinishLoad);
    didFinishLoad = false;
    failedObjCDescription = false;

    std::thread thread1(nsObjectDescriptionTest, object);

    CFBooleanRef result = (CFBooleanRef)([[webView.get() windowScriptObject] callWebScriptMethod:@"foo" withArguments:@[ object ]]);
    EXPECT_EQ(CFBooleanGetValue(result), true);

    thread1.join();
    EXPECT_EQ(failedObjCDescription, false);
    [object release];
}

} // namespace TestWebKitAPI

