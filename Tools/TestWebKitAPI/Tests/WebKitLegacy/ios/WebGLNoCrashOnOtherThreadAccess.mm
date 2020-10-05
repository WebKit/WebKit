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

#import "PlatformUtilities.h"
#import <JavaScriptCore/JSVirtualMachine.h>
#import <JavaScriptCore/JSVirtualMachineInternal.h>
#import <UIKit/UIKit.h>
#import <WebCore/WebCoreThread.h>
#import <stdlib.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import <OpenGL/OpenGL.h>
#elif PLATFORM(IOS)
#import <OpenGLES/EAGL.h>
#endif

namespace {
bool didFinishLoad;
bool didFinishTest;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
class SetContextCGL {
    SetContextCGL()
    {
        // TODO: implement properly. Skipped due to complicated instantiation.
        // instead, just clobber the current context with nullptr.
        CGLSetCurrentContext(nullptr);
    }
    ~SetContextCGL()
    {
        CGLSetCurrentContext(nullptr);
    }
};
#else
class SetContextCGL {

};
#endif

#if PLATFORM(IOS)
class SetContextEAGL {
public:
    SetContextEAGL()
    {
        m_eaglContext = adoptNS([[EAGLContext alloc] initWithAPI: kEAGLRenderingAPIOpenGLES3]);
        auto r = [EAGLContext setCurrentContext: m_eaglContext.get()];
        ASSERT(r);
        UNUSED_VARIABLE(r);
    }
    ~SetContextEAGL()
    {
        [EAGLContext setCurrentContext: nil];
    }

private:
    RetainPtr<EAGLContext> m_eaglContext;
};
#else
class SetContextEAGL {

};
#endif

}

@interface WebGLNoCrashOnOtherThreadAccessWebViewDelegate : NSObject <UIWebViewDelegate>
@end

@implementation WebGLNoCrashOnOtherThreadAccessWebViewDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    didFinishLoad = true;
}

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if ([request.URL.scheme isEqualToString:@"callback"]) {
        if ([request.URL.resourceSpecifier isEqualToString:@"didFinishTest"])
            didFinishTest = true;
        else
            EXPECT_TRUE(false) << [request.URL.resourceSpecifier UTF8String];
        return NO;
    }

    return YES;
}
IGNORE_WARNINGS_END

@end

namespace TestWebKitAPI {

// Test that tests behavior of UIWebView API with regards to WebGL:
// 1) WebGL can be run on web thread.
// 2) WebGL can be run on client main thread.
// 3) WebGL run on client thread is not affected by client changing
//    the EAGL/CGL context.
TEST(WebKitLegacy, WebGLNoCrashOnOtherThreadAccess)
{
    const unsigned testIterations = 10;

    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];

    ASSERT_TRUE(WebThreadIsEnabled());
    SetContextCGL clientContextCGL1;
    SetContextEAGL clientContextEAGL1;
    UNUSED_VARIABLE(clientContextCGL1);
    UNUSED_VARIABLE(clientContextEAGL1);

    RetainPtr<WebGLNoCrashOnOtherThreadAccessWebViewDelegate> uiDelegate =
        adoptNS([[WebGLNoCrashOnOtherThreadAccessWebViewDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();
    NSString* testHtml = @R"HTML(<body>NOLINT(readability/multiline_string)
<script>
function runTest()
{
    let w = 100;
    let h = 100;
    let canvas = document.createElement("canvas");
    canvas.width = w;
    canvas.height = h;
    let gl = canvas.getContext("webgl");
    gl.clearColor(0, 0, 1, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    let expected = 255 << 24 | 255 << 16;
    let ps = new Uint8Array(w * h * 4);
    gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, ps);
    for (let i = 0; i < ps.length; i += 4) {
        let got = ps[i] | ps[i+1] << 8 | ps[i+2] << 16 | ps[i+3] << 24;
        if (got != expected)
            return `${got} != ${expected}`; // NOLINT
    }
    return "didFinishTest";
}
function runTestAsync()
{
    setTimeout(() => { window.location = 'callback:' + runTest(); }, 10); // NOLINT
}
</script>)HTML"; // NOLINT(readability/multiline_string)
    [uiWebView loadHTMLString:testHtml baseURL:nil];

    Util::run(&didFinishLoad);

    RetainPtr<JSContext> jsContext = [uiWebView valueForKeyPath:@"documentView.webView.mainFrame.javaScriptContext"];
    RetainPtr<JSVirtualMachine> jsVM = [jsContext virtualMachine];
    EXPECT_TRUE([jsVM isWebThreadAware]);

    // Start to run WebGL on web thread.
    [uiWebView stringByEvaluatingJavaScriptFromString:@"runTestAsync();"];
    EXPECT_FALSE(didFinishTest); // The pattern runs the test on web thread.

    for (unsigned i = 0; i < testIterations; i++) {
        // Run WebGL on client main thread.
        NSString* result = [[jsContext evaluateScript: @"runTest();"] toString];
        EXPECT_TRUE([@"didFinishTest" isEqualToString: result]);

        // Client clobbers EAGLContext between calls that run WebGL on main thread.
        SetContextCGL clientContextCGL2;
        SetContextEAGL clientContextEAGL2;
        UNUSED_VARIABLE(clientContextCGL2);
        UNUSED_VARIABLE(clientContextEAGL2);

        result = [[jsContext evaluateScript: @"runTest();"] toString];
        EXPECT_TRUE([@"didFinishTest" isEqualToString: result]);

        // Wait until the previous WebGL test run has finished.
        Util::run(&didFinishTest);
        didFinishTest = false;
        // Start to run WebGL on web thread.
        [uiWebView stringByEvaluatingJavaScriptFromString:@"runTestAsync();"];
        EXPECT_FALSE(didFinishTest);
    }

    Util::run(&didFinishTest);
}

}

#endif
