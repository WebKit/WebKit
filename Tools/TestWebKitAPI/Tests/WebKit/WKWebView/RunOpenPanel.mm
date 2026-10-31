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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKOpenPanelParametersPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static bool fileSelected;
static NSString * const expectedFileName = @"这是中文";

@interface RunOpenPanelUIDelegate : NSObject <WKUIDelegate>
@end

@implementation RunOpenPanelUIDelegate

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *))completionHandler
{
    EXPECT_FALSE(parameters.allowsMultipleSelection);
    EXPECT_FALSE(parameters.allowsDirectories);
    EXPECT_EQ(0ull, parameters._acceptedMIMETypes.count);
    EXPECT_EQ(0ull, parameters._acceptedFileExtensions.count);
    [[NSFileManager defaultManager] createFileAtPath:[NSTemporaryDirectory() stringByAppendingPathComponent:expectedFileName] contents:nil attributes:nil];
    completionHandler(@[ [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:expectedFileName]] ]);
    fileSelected = true;
    [[NSFileManager defaultManager] removeItemAtPath:[NSTemporaryDirectory() stringByAppendingPathComponent:expectedFileName] error:nil];
}

@end

@interface FileInputTypeCancelEventUIDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation FileInputTypeCancelEventUIDelegate

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> * URLs))completionHandler
{
    completionHandler(nil);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, RunOpenPanelNonLatin1)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr uiDelegate = adoptNS([[RunOpenPanelUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<!DOCTYPE html><input style='width: 100vw; height: 100vh;' id='file' type='file'>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    [webView clickOnElementID:@"file"];
    Util::run(&fileSelected);
    Util::runFor(50_ms);
    
    __block bool testFinished = false;
    [webView evaluateJavaScript:@"document.getElementsByTagName('input')[0].files[0].name" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(expectedFileName, result);
        testFinished = true;
    }];
    Util::run(&testFinished);
}

TEST(WebKit, FileInputTypeCancelEvent)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr uiDelegate = adoptNS([[FileInputTypeCancelEventUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    NSString *markup = @""
        "<script>"
        "    function loaded() {"
        "        setTimeout(() => {"
        "            const $file = document.getElementById('file');"
        "            $file.addEventListener('cancel', () => {"
        "                webkit.messageHandlers.testHandler.postMessage('cancel');"
        "            }, false);"
        "        }, 0);"
        "    }"
        "</script>"
        "<body style='width: 100vw; height: 100vh;' onload='loaded()'>"
        "   <input style='width: 100vw; height: 100vh;' type='file' id='file' name='file'>"
        "</body>";

    [webView loadHTMLString:markup baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    [webView performAfterReceivingMessage:@"cancel" action:^{
        done = true;
    }];

    [webView clickOnElementID:@"file"];

    Util::run(&done);
}

TEST(WebKit, FileInputOpenPanelAfterMessageChannelHop)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr uiDelegate = adoptNS([[FileInputTypeCancelEventUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    NSString *markup = @""
        "<body style='margin: 0;'>"
        "<button id='button' style='width: 100vw; height: 100vh;'>Choose</button>"
        "<script>"
        "    const channel = new MessageChannel();"
        "    channel.port1.onmessage = () => {"
        "        const input = document.createElement('input');"
        "        input.type = 'file';"
        "        input.addEventListener('cancel', () => webkit.messageHandlers.testHandler.postMessage('cancel'));"
        "        document.body.appendChild(input);"
        "        input.click();"
        "        setTimeout(() => webkit.messageHandlers.testHandler.postMessage('timeout'), 1000);"
        "    };"
        "    document.getElementById('button').addEventListener('click', () => channel.port2.postMessage(0));"
        "</script>"
        "</body>";

    [webView loadHTMLString:markup baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    __block bool openPanelRan = false;
    [webView performAfterReceivingMessage:@"cancel" action:^{
        openPanelRan = true;
        done = true;
    }];
    [webView performAfterReceivingMessage:@"timeout" action:^{
        done = true;
    }];

    [webView sendClickAtPoint:NSMakePoint(50, 50)];

    Util::run(&done);
    EXPECT_TRUE(openPanelRan);
}

TEST(WebKit, FileInputOpenPanelAfterMessageChannelHopAndAnimationFrame)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr uiDelegate = adoptNS([[FileInputTypeCancelEventUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    NSString *markup = @""
        "<body style='margin: 0;'>"
        "<button id='button' style='width: 100vw; height: 100vh;'>Choose</button>"
        "<button id='hidden' style='display: none;'></button>"
        "<script>"
        "    const channel = new MessageChannel();"
        "    document.getElementById('hidden').addEventListener('click', () => {"
        "        const input = document.createElement('input');"
        "        input.type = 'file';"
        "        input.addEventListener('cancel', () => webkit.messageHandlers.testHandler.postMessage('cancel'));"
        "        document.body.appendChild(input);"
        "        input.click();"
        "        setTimeout(() => webkit.messageHandlers.testHandler.postMessage('timeout'), 1000);"
        "    });"
        "    channel.port1.onmessage = () => requestAnimationFrame(() => document.getElementById('hidden').click());"
        "    document.getElementById('button').addEventListener('click', () => channel.port2.postMessage(0));"
        "</script>"
        "</body>";

    [webView loadHTMLString:markup baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    __block bool openPanelRan = false;
    [webView performAfterReceivingMessage:@"cancel" action:^{
        openPanelRan = true;
        done = true;
    }];
    [webView performAfterReceivingMessage:@"timeout" action:^{
        done = true;
    }];

    [webView sendClickAtPoint:NSMakePoint(50, 50)];

    Util::run(&done);
    EXPECT_TRUE(openPanelRan);
}

TEST(WebKit, FileInputOpenPanelFromRunningMessageLoop)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr uiDelegate = adoptNS([[FileInputTypeCancelEventUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    // A scheduler-style loop that is already running when the click arrives, so the click's work is
    // picked up by a message that was posted before the user gesture existed.
    NSString *markup = @""
        "<body style='margin: 0;'>"
        "<button id='button' style='width: 100vw; height: 100vh;'>Choose</button>"
        "<script>"
        "    const channel = new MessageChannel();"
        "    const queue = [];"
        "    let stopped = false;"
        "    channel.port1.onmessage = () => {"
        "        for (const task of queue.splice(0))"
        "            task();"
        "        if (!stopped)"
        "            channel.port2.postMessage(0);"
        "    };"
        "    channel.port2.postMessage(0);"
        "    document.getElementById('button').addEventListener('click', () => {"
        "        queue.push(() => {"
        "            const input = document.createElement('input');"
        "            input.type = 'file';"
        "            input.addEventListener('cancel', () => webkit.messageHandlers.testHandler.postMessage('cancel'));"
        "            document.body.appendChild(input);"
        "            input.click();"
        "            setTimeout(() => { stopped = true; webkit.messageHandlers.testHandler.postMessage('timeout'); }, 1000);"
        "        });"
        "    });"
        "</script>"
        "</body>";

    [webView loadHTMLString:markup baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    __block bool openPanelRan = false;
    [webView performAfterReceivingMessage:@"cancel" action:^{
        openPanelRan = true;
        done = true;
    }];
    [webView performAfterReceivingMessage:@"timeout" action:^{
        done = true;
    }];

    [webView sendClickAtPoint:NSMakePoint(50, 50)];

    Util::run(&done);
    EXPECT_TRUE(openPanelRan);
}

} // namespace TestWebKitAPI
