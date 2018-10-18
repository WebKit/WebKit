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

#if PLATFORM(IOS_FAMILY) && WK_API_ENABLED

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

@implementation WKWebView (WKAccessibilityTesting)
- (NSArray<NSValue *> *)rectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text
{
    __block RetainPtr<NSArray> selectionRects;
    __block bool done = false;
    [self _accessibilityRetrieveRectsAtSelectionOffset:offset withText:text completionHandler:^(NSArray<NSValue *> *rects) {
        selectionRects = rects;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return selectionRects.autorelease();
}
@end

static void checkCGRectValueAtIndex(NSArray<NSValue *> *rectValues, CGRect expectedRect, NSUInteger index)
{
    EXPECT_LT(index, rectValues.count);
    auto observedRect = [rectValues[index] CGRectValue];
    EXPECT_EQ(expectedRect.origin.x, observedRect.origin.x);
    EXPECT_EQ(expectedRect.origin.y, observedRect.origin.y);
    EXPECT_EQ(expectedRect.size.width, observedRect.size.width);
    EXPECT_EQ(expectedRect.size.height, observedRect.size.height);
}

namespace TestWebKitAPI {

TEST(AccessibilityTests, RectsForSpeakingSelectionBasic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><span id='first'>first</span><span id='second'> second</span><br><span id='third'> third</span>"];
    [webView stringByEvaluatingJavaScript:@"document.execCommand('SelectAll')"];

    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:0 withText:@"first"], CGRectMake(8, 8, 26, 19), 0);
    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:6 withText:@"second"], CGRectMake(37, 8, 46, 19), 0);
    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:13 withText:@"third"], CGRectMake(8, 27, 31, 20), 0);
}

TEST(AccessibilityTests, RectsForSpeakingSelectionWithLineWrapping)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><body style='font-size: 100px; word-wrap: break-word'><span id='text'>abcdefghijklmnopqrstuvwxyz</span></body>"];
    [webView stringByEvaluatingJavaScript:@"document.execCommand('SelectAll')"];

    NSArray<NSValue *> *rects = [webView rectsAtSelectionOffset:0 withText:@"abcdefghijklmnopqrstuvwxyz"];
    checkCGRectValueAtIndex(rects, CGRectMake(8, 8, 304, 114), 0);
    checkCGRectValueAtIndex(rects, CGRectMake(8, 122, 304, 117), 1);
    checkCGRectValueAtIndex(rects, CGRectMake(8, 239, 304, 117), 2);
    checkCGRectValueAtIndex(rects, CGRectMake(8, 356, 304, 117), 3);
    checkCGRectValueAtIndex(rects, CGRectMake(8, 473, 145, 117), 4);
}

TEST(AccessibilityTests, RectsForSpeakingSelectionDoNotCrashWhenChangingSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><span id='first'>first</span><span id='second'> second</span><br><span id='third'> third</span>"];

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(third, 0, third, 1)"];
    EXPECT_EQ(0UL, [webView rectsAtSelectionOffset:13 withText:@"third"].count);
    EXPECT_WK_STREQ("third", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];
    EXPECT_EQ(0UL, [webView rectsAtSelectionOffset:13 withText:@"third"].count);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(AccessibilityTests, StoreSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><span id='first'>first</span><br><span id='second'>first</span>"];
    
    // Select first node and store the selection
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(first, 0, first, 1)"];
    [webView _accessibilityStoreSelection];
    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:0 withText:@"first"], CGRectMake(8, 8, 26, 19), 0);
    // Now select the second node, we should use the stored selection to retrieve rects
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(second, 0, second, 1)"];
    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:0 withText:@"first"], CGRectMake(8, 8, 26, 19), 0);
    
    // Clear the stored selection, we should use the current selection to retrieve rects
    [webView _accessibilityClearSelection];
    checkCGRectValueAtIndex([webView rectsAtSelectionOffset:0 withText:@"first"], CGRectMake(8, 27, 26, 20), 0);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && WK_API_ENABLED

