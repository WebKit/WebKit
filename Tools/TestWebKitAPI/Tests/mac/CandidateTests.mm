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

#import "config.h"

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import <WebKit/WebViewPrivate.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <wtf/RetainPtr.h>

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
    auto webView = adoptNS([[DoNotLeakWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    auto delegate = adoptNS([[DoNotLeakFrameLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"autofocused-text-input" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    TestWebKitAPI::Util::run(&didCallShowCandidates);

    webView = nil;
    EXPECT_TRUE(webViewWasDeallocated);
}

TEST(CandidateTests, DISABLED_RequestCandidatesForTextInput)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView forceRequestCandidatesForTesting];
    auto delegate = adoptNS([[CandidateRequestFrameLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"focus-inputs" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    
    [webView forceRequestCandidatesForTesting];

    if ([webView shouldRequestCandidates])
        candidatesWereRequested = true;

    webView = nil;
    EXPECT_TRUE(candidatesWereRequested);
}

TEST(CandidateTests, DoNotRequestCandidatesForPasswordInput)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView forceRequestCandidatesForTesting];
    auto delegate = adoptNS([[CandidateRequestFrameLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    
    NSURL *contentURL = [[NSBundle mainBundle] URLForResource:@"focus-inputs" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];
    
    TestWebKitAPI::Util::run(&didFinishLoad);
    
    [webView forceRequestCandidatesForTesting];
    [webView stringByEvaluatingJavaScriptFromString:@"focusPasswordField()"];

    if ([webView shouldRequestCandidates])
        candidatesWereRequested = true;

    EXPECT_FALSE(candidatesWereRequested);
}

static BOOL candidateListWasHidden = NO;
static BOOL candidateListWasShown = NO;
static void updateCandidateListWithVisibility(id, SEL, BOOL visible)
{
    if (visible)
        candidateListWasShown = YES;
    else
        candidateListWasHidden = YES;
}

TEST(CandidateTests, DoNotHideCandidatesDuringTextReplacement)
{
    InstanceMethodSwizzler visibilityUpdateSwizzler { NSCandidateListTouchBarItem.class, @selector(updateWithInsertionPointVisibility:), reinterpret_cast<IMP>(updateCandidateListWithVisibility) };

    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView forceRequestCandidatesForTesting];

    auto delegate = adoptNS([[CandidateRequestFrameLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    [[webView mainFrame] loadHTMLString:@"<body contenteditable>Hello world<script>document.body.focus()</script>" baseURL:nil];
    TestWebKitAPI::Util::run(&didFinishLoad);

    auto textToInsert = adoptNS([[NSMutableAttributedString alloc] initWithString:@"Goodbye"]);
    [textToInsert addAttribute:NSTextInputReplacementRangeAttributeName value:NSStringFromRange(NSMakeRange(0, 5)) range:NSMakeRange(0, 7)];
    [webView insertText:textToInsert.get()];

    EXPECT_WK_STREQ("Goodbye world", [webView stringByEvaluatingJavaScriptFromString:@"document.body.innerText"]);
    EXPECT_FALSE(candidateListWasHidden);
    EXPECT_TRUE(candidateListWasShown);
}

}

#endif // PLATFORM(MAC)
