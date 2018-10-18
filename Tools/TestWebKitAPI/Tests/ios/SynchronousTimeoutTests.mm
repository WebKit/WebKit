/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

using namespace TestWebKitAPI;

@interface NSObject ()

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point;

@end

namespace TestWebKitAPI {
    
static UIGestureRecognizer *recursiveFindHighlightLongPressRecognizer(UIView *view)
{
    for (UIGestureRecognizer *recognizer in view.gestureRecognizers) {
        if ([recognizer isKindOfClass:NSClassFromString(@"_UIWebHighlightLongPressGestureRecognizer")])
            return recognizer;
    }
    
    for (UIView *subview in view.subviews) {
        UIGestureRecognizer *recognizer = recursiveFindHighlightLongPressRecognizer(subview);
        if (recognizer)
            return recognizer;
    }
    
    return nil;
}
    
TEST(SynchronousTimeoutTests, UnresponsivePageDoesNotCausePositionInformationToHangUI)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    
    UIGestureRecognizer *highlightLongPressRecognizer = recursiveFindHighlightLongPressRecognizer(webView.get());
    UIView *interactionView = highlightLongPressRecognizer.view;
    EXPECT_NOT_NULL(highlightLongPressRecognizer);
    
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];
    
    // The test passes if we can long press and still finish the test.
    [interactionView hasSelectablePositionAtPoint:CGPointMake(100, 100)];
}

    
} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)

