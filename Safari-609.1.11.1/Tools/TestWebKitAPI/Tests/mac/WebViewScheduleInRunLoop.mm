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
#import "PlatformUtilities.h"
#import <CoreFoundation/CFRunLoop.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

@interface ScheduleInRunLoopDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

static size_t loadsFinished;

@implementation ScheduleInRunLoopDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    loadsFinished++;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, ScheduleInRunLoop)
{
    const size_t webViewCount = 50;
    Vector<RetainPtr<WebView>> webViews;
    auto delegate = adoptNS([[ScheduleInRunLoopDelegate alloc] init]);
    for (size_t i = 0; i < webViewCount; ++i) {
        auto webView = adoptNS([[WebView alloc] init]);
        [webView setFrameLoadDelegate:delegate.get()];
        [webView unscheduleFromRunLoop:[NSRunLoop currentRunLoop] forMode:(NSString *)kCFRunLoopCommonModes];
        [webView scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:@"TestRunLoopMode"];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
        webViews.append(WTFMove(webView));
    }

    while (loadsFinished < webViewCount)
        CFRunLoopRunInMode(CFSTR("TestRunLoopMode"), .001, true);
}

} // namespace TestWebKitAPI
