/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "Test.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKTextPreview.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

#if USE(UICONTEXTMENU) || PLATFORM(MAC)

TEST(ElementTextPreview, PreviewForElement)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<p id='element'>Hello world!</p>"];

    __block bool done = false;

#if USE(UICONTEXTMENU)
    [webView _targetedPreviewForElementWithID:@"element" completionHandler:^(UITargetedPreview *preview) {
        EXPECT_NOT_NULL(preview);
        EXPECT_TRUE([[preview view] isKindOfClass:[UIImageView class]]);
        EXPECT_NOT_NULL([preview target]);
        EXPECT_EQ([[preview target] container], static_cast<id>(webView.get()));

        done = true;
    }];
#else
    [webView _textPreviewsForElementWithID:@"element" completionHandler:^(NSArray<_WKTextPreview *> *previews) {
        EXPECT_NOT_NULL(previews);
        EXPECT_EQ([previews count], 1U);
        EXPECT_NOT_NULL([[previews firstObject] previewImage]);

        done = true;
    }];
#endif

    TestWebKitAPI::Util::run(&done);
}

#endif // USE(UICONTEXTMENU) || PLATFORM(MAC)

} // namespace TestWebKitAPI
