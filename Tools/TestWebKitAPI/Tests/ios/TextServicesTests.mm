/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <wtf/RetainPtr.h>

@interface WKWebView (TextServices)
- (void)_lookup:(id)sender;
@end

namespace TestWebKitAPI {

static bool doneWaitingForLookup = false;
static NSRange lastLookupRange = NSMakeRange(0, 0);
static CGRect lastLookupPresentationRect = CGRectNull;

static void handleLookup(id, SEL, NSString *, NSRange range, CGRect presentationRect)
{
    lastLookupRange = range;
    lastLookupPresentationRect = presentationRect;
    doneWaitingForLookup = true;
}

TEST(TextServicesTests, LookUpPresentationRect)
{
    InstanceMethodSwizzler swizzler { UIWKTextInteractionAssistant.class, @selector(lookup:withRange:fromRect:), reinterpret_cast<IMP>(handleLookup) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><style>p { font-size: 20px; }</style><p>Foo</p><p>Hello World</p>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.querySelector('p'))"];
    [webView waitForNextPresentationUpdate];

    doneWaitingForLookup = false;
    [webView _lookup:nil];
    TestWebKitAPI::Util::run(&doneWaitingForLookup);

    EXPECT_EQ(lastLookupRange.location, 0U);
    EXPECT_EQ(lastLookupRange.length, 3U);
    auto rectWhenLookingUpFirstLine = lastLookupPresentationRect;

    [[webView textInputContentView] selectAll:nil];
    [webView waitForNextPresentationUpdate];

    doneWaitingForLookup = false;
    [webView _lookup:nil];
    TestWebKitAPI::Util::run(&doneWaitingForLookup);

    EXPECT_EQ(lastLookupRange.location, 0U);
    EXPECT_EQ(lastLookupRange.length, 16U);
    auto rectWhenLookingUpBothLines = lastLookupPresentationRect;

    EXPECT_GT(rectWhenLookingUpBothLines.size.width, rectWhenLookingUpFirstLine.size.width);
    EXPECT_GT(rectWhenLookingUpBothLines.size.height, rectWhenLookingUpFirstLine.size.height);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
