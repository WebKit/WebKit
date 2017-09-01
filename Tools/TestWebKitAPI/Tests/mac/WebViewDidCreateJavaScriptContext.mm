/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import <JavaScriptCore/JSExport.h>
#import <JavaScriptCore/JSContext.h>
#import <WebKit/WebFrameLoadDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if JSC_OBJC_API_ENABLED

@class MyConsole;

static bool didFinishLoad = false;
static bool didCompleteTestSuccessfully = false;
static bool didCallWindowCallback = false;
static bool didFindMyCustomProperty = false;
static bool didInsertMyCustomProperty = true;
static bool didReportException = false;

@protocol MyConsole<JSExport>
- (void)log:(NSString *)s;
- (void)printHelloWorld;
- (int)add:(int)a to:(int)b;
@end

@interface MyConsole : NSObject<MyConsole>
@end

@implementation MyConsole
- (void)log:(NSString *)s
{
    NSLog(@"%@", s);
}

- (void)printHelloWorld
{
    NSLog(@"Hello, World!");
}

- (int)add:(int)a to:(int)b
{
    return a + b;
}
@end

@interface DidCreateJavaScriptContextFrameLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation DidCreateJavaScriptContextFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

- (void)webView:(WebView *)webView didCreateJavaScriptContext:(JSContext *)context forFrame:(WebFrame *)frame
{
    MyConsole *myConsole = [[MyConsole alloc] init];
    context[@"myConsole"] = myConsole;
    context.exceptionHandler = nil;
    [myConsole release];

    context[@"windowCallback"] = ^(JSValue *thisObject){
        didCallWindowCallback = true;
    };

    context[@"didCompleteTestSuccessfully"] = ^{
        didCompleteTestSuccessfully = true;
    };

    context[@"callMeBack"] = ^(JSValue *functionValue) {
        [functionValue callWithArguments:[NSArray array]];
    };

    context[@"checkForMyCustomProperty"] = ^(JSValue *element) {
        if ([element hasProperty:@"myCustomProperty"] && [[element valueForProperty:@"myCustomProperty"] toInt32] == 42)
            didFindMyCustomProperty = true;
        else
            NSLog(@"ERROR: Did not find myCustomProperty.");
    };

    context[@"insertMyCustomProperty"] = ^(JSValue *element) {
        JSValue *fortyTwo = [JSValue valueWithInt32:42 inContext:[JSContext currentContext]];
        [element setValue:fortyTwo forProperty:@"myCustomProperty"];
        didInsertMyCustomProperty = true;
    };

    context[@"testReportException"] = ^{
        JSContext* context = [JSContext currentContext];
        [context evaluateScript:@"function doThrow() { throw 'TestError'; }"];

        JSGlobalContextRef globalContext = [context JSGlobalContextRef];
        JSObjectRef globalObject = JSContextGetGlobalObject(globalContext);

        JSStringRef jsString = JSStringCreateWithUTF8CString("doThrow");
        JSValueRef function = JSObjectGetProperty(globalContext, globalObject, jsString, nullptr);
        JSStringRelease(jsString);

        const JSValueRef arguments[] = { };

        JSValueRef exception = 0;
        JSObjectCallAsFunction(globalContext, (JSObjectRef)function, globalObject, 0, arguments, &exception);
        if (exception) {
            [WebView _reportException:exception inContext:globalContext];
            didReportException = true;
        }
    };
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, DidCreateJavaScriptContextSanity1)
{
    didFinishLoad = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSString *bodyString = 
            @"<body> \
                <script> \
                    myConsole.printHelloWorld(); \
                    myConsole.log(\"Loaded custom stuff.\"); \
                    myConsole.log(myConsole.addTo(40, 2)); \
                    didCompleteTestSuccessfully(); \
                </script> \
            </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];

        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didCompleteTestSuccessfully);
    }
}

TEST(WebKitLegacy, DidCreateJavaScriptContextSanity2)
{
    didCallWindowCallback = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSString *bodyString = 
            @"<body> \
                <script> \
                    setTimeout(windowCallback, 100); \
                </script> \
            </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];

        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didCallWindowCallback);
    }
}

TEST(WebKitLegacy, DidCreateJavaScriptContextCallJSFunctionFromObjCCallbackTest)
{
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSString *bodyString = 
            @"<body> \
                <script> \
                    callMeBack(function() { \
                        didCompleteTestSuccessfully(); \
                    }); \
                </script> \
            </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];

        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didCompleteTestSuccessfully);
    }
}

TEST(WebKitLegacy, DidCreateJavaScriptContextAddCustomPropertiesFromJSTest)
{
    didFindMyCustomProperty = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSString *bodyString = 
        @"<body> \
            <div id=\"test-div\"></div> \
            <script> \
                var testDiv = document.getElementById(\"test-div\"); \
                testDiv.myCustomProperty = 42; \
                checkForMyCustomProperty(testDiv); \
            </script> \
        </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];

        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didFindMyCustomProperty);
    }
}

TEST(WebKitLegacy, DidCreateJavaScriptContextAddCustomPropertiesFromObjCTest)
{
    didFindMyCustomProperty = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSString *bodyString = 
            @"<body> \
                <div id=\"test-div\"></div> \
                <script> \
                    var testDiv = document.getElementById(\"test-div\"); \
                    insertMyCustomProperty(testDiv); \
                    if (testDiv.myCustomProperty === 42) { \
                        checkForMyCustomProperty(testDiv); \
                    } \
                </script> \
            </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];

        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didFindMyCustomProperty);
    }
}

TEST(WebKitLegacy, DidCreateJavaScriptContextBackForwardCacheTest)
{
    didInsertMyCustomProperty = false;
    didFindMyCustomProperty = false;
    didCompleteTestSuccessfully = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);
        [[webView.get() preferences] setCacheModel:WebCacheModelDocumentBrowser];

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"JSContextBackForwardCache1" 
                                              withExtension:@"html" 
                                               subdirectory:@"TestWebKitAPI.resources"];
        [mainFrame loadRequest:[NSURLRequest requestWithURL:url1]];
        Util::run(&didInsertMyCustomProperty);

        NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"JSContextBackForwardCache2" 
                                              withExtension:@"html" 
                                               subdirectory:@"TestWebKitAPI.resources"];
        [mainFrame loadRequest:[NSURLRequest requestWithURL:url2]];
        Util::run(&didCompleteTestSuccessfully);

        didCompleteTestSuccessfully = false;
        [[mainFrame javaScriptContext] evaluateScript:
            @"var testDiv = document.getElementById(\"test-div\"); \
            if (!testDiv.myCustomProperty) { \
                didCompleteTestSuccessfully(); \
            }"];
        EXPECT_TRUE(didCompleteTestSuccessfully);

        if ([webView.get() goBack]) {
            [[mainFrame javaScriptContext] evaluateScript:
                @"var testDiv = document.getElementById(\"test-div\"); \
                checkForMyCustomProperty(testDiv);"];
            EXPECT_TRUE(didFindMyCustomProperty);
        } else
            EXPECT_TRUE(false);
    }
}

TEST(WebKitLegacy, ReportExceptionTest)
{
    didReportException = false;
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        RetainPtr<DidCreateJavaScriptContextFrameLoadDelegate> frameLoadDelegate = adoptNS([[DidCreateJavaScriptContextFrameLoadDelegate alloc] init]);
        
        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;
        
        NSString *bodyString =
            @"<body> \
                <script> \
                    testReportException(); \
                </script> \
            </body>";
        NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];
        
        [mainFrame loadHTMLString:bodyString baseURL:aboutBlankURL];
        Util::run(&didReportException);
    }
}

} // namespace TestWebKitAPI

#endif // ENABLE(JSC_OBJC_API)
