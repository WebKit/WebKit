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

#if USE(WEB_THREAD)

#import "Helpers/PlatformUtilities.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKitLegacy, NestedRunLoopUnderRunLoopObserverDoubleUnlock)
{
    WebThreadEnable();

    WebThreadLock();
    
    __block BOOL spunInnerRunLoop = NO;
    RetainPtr observer = adoptCF(CFRunLoopObserverCreateWithHandler(NULL, kCFRunLoopBeforeWaiting, NO, 2, ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
        Util::spinRunLoop(1);
        spunInnerRunLoop = YES;
    }));

    CFRunLoopAddObserver(CFRunLoopGetMain(), observer.get(), kCFRunLoopCommonModes);

    // We must use a time-based instead of count-or-condition-based spin,
    // so that we hit the above "before waiting" observer.
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    
    EXPECT_TRUE(spunInnerRunLoop);
    
    // Spinning the runloop should have resulted in dropping the lock,
    // and *not underflowing* the lock count (which is also reported as "locked").
    EXPECT_FALSE(WebThreadIsLocked());
}

TEST(WebKitLegacy, WebThreadRunWithoutWebThread)
{
    ASSERT_FALSE(WebThreadIsEnabled());

    __block bool ranSynchronously = false;
    WebThreadRun(^{
        ranSynchronously = true;
    });
    EXPECT_TRUE(ranSynchronously);

    __block bool done = false;
    __block bool ranOnMainThread = false;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        WebThreadRun(^{
            ranOnMainThread = [NSThread isMainThread];
            done = true;
        });
    });
    Util::run(&done);
    EXPECT_TRUE(ranOnMainThread);
}

TEST(WebKitLegacy, DedicatedWorkerWithoutWebThread)
{
    ASSERT_FALSE(WebThreadIsEnabled());

    RetainPtr window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr webView = adoptNS([[UIWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [window addSubview:webView.get()];
    [webView loadHTMLString:@"<script>"
        "const worker = new Worker(URL.createObjectURL(new Blob(['postMessage(1.5 * 2)'])));"
        "worker.onmessage = (event) => { document.title = 'result:' + event.data; };"
        "</script>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScriptFromString:@"document.title"] isEqualToString:@"result:3"];
    }));
}

}

#endif
