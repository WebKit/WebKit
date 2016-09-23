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
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201

static bool webViewWasDeallocated = false;
static bool didFinishLoad = false;
static bool didCallShowCandidates = false;
static bool candidatesWereRequested = false;

@interface DoNotLeakWebView : WebView
@end

@implementation DoNotLeakWebView

- (void)dealloc
{
    webViewWasDeallocated = true;
    [super dealloc];
}

- (void)showCandidates:(NSArray *)candidates forString:(NSString *)string inRect:(NSRect)rectOfTypedString forSelectedRange:(NSRange)range view:(NSView *)view completionHandler:(void (^)(NSTextCheckingResult *acceptedCandidate))completionBlock
{
    [super showCandidates:candidates forString:string inRect:rectOfTypedString forSelectedRange:range view:view completionHandler:completionBlock];
    didCallShowCandidates = true;
}

@end

@interface DoNotLeakFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

@implementation DoNotLeakFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    [sender forceRequestCandidatesForTesting];
    didFinishLoad = true;
}

@end

@interface CandidateRequestFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

@implementation CandidateRequestFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(CandidateTests, DISABLED_DoNotLeakViewThatLoadsEditableArea)
{
    DoNotLeakWebView *webView = [[DoNotLeakWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    DoNotLeakFrameLoadDelegate *delegate = [[DoNotLeakFrameLoadDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"autofocused-text-input" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    TestWebKitAPI::Util::run(&didCallShowCandidates);

    [webView release];
    EXPECT_TRUE(webViewWasDeallocated);
}

TEST(CandidateTests, DISABLED_RequestCandidatesForTextInput)
{
    WebView *webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [webView forceRequestCandidatesForTesting];
    CandidateRequestFrameLoadDelegate *delegate = [[CandidateRequestFrameLoadDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"focus-inputs" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    
    [webView forceRequestCandidatesForTesting];

    if ([webView shouldRequestCandidates])
        candidatesWereRequested = true;

    [webView release];
    EXPECT_TRUE(candidatesWereRequested);
}

TEST(CandidateTests, DoNotRequestCandidatesForPasswordInput)
{
    WebView *webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [webView forceRequestCandidatesForTesting];
    CandidateRequestFrameLoadDelegate *delegate = [[CandidateRequestFrameLoadDelegate alloc] init];
    [webView setFrameLoadDelegate:delegate];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"focus-inputs" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    
    [webView forceRequestCandidatesForTesting];
    [webView stringByEvaluatingJavaScriptFromString:@"focusPasswordField()"];

    if ([webView shouldRequestCandidates])
        candidatesWereRequested = true;

    [webView release];
    EXPECT_FALSE(candidatesWereRequested);
}

}

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101201
