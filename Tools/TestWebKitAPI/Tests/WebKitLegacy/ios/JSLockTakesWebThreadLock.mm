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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import <JavaScriptCore/JSVirtualMachine.h>
#import <JavaScriptCore/JSVirtualMachineInternal.h>
#import <UIKit/UIKit.h>
#import <WebCore/WebCoreThread.h>
#import <stdlib.h>
#import <wtf/DataLog.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKitLegacy, JSLockTakesWebThreadLock)
{
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];
    [uiWebView loadHTMLString:@"<p>WebThreadLock and JSLock</p>" baseURL:nil];
    RetainPtr<JSContext> jsContext = [uiWebView valueForKeyPath:@"documentView.webView.mainFrame.javaScriptContext"];

    EXPECT_TRUE(!!jsContext.get());

    RetainPtr<JSVirtualMachine> jsVM = [jsContext virtualMachine];
    EXPECT_TRUE([jsVM isWebThreadAware]);

    // Release WebThread lock.
    Util::spinRunLoop(1);

    EXPECT_FALSE(WebThreadIsLocked());
    // XMLHttpRequest has Timer, which has thread afinity.
    [jsContext evaluateScript:@"for (var i = 0; i < 1e3; ++i) { var request = new XMLHttpRequest; request.property = new Array(10000); }"];
}

}

#endif
