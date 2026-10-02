/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(UIFINDINTERACTION) && defined(__cplusplus)

#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>

@class TestScrollViewDelegate;
@class TestWKWebView;

namespace TestWebKitAPI {

class FindInteractionTester {
public:
    explicit FindInteractionTester(TestWKWebView *);
    ~FindInteractionTester();

    void didBeginTextSearchOperation();
    void didEndTextSearchOperation();
    RetainPtr<NSOrderedSet<UITextRange *>> performTextSearch(NSString *, UITextSearchOptions * = nil);
    void decorateFoundTextRange(UITextRange *, UITextSearchFoundTextStyle);
    void clearAllDecoratedFoundText();
    void scrollRangeToVisible(UITextRange *);
    void replaceFoundTextInRange(UITextRange *, NSString *);
    CGRect rectForFoundTextRange(UITextRange *);

private:
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<TestScrollViewDelegate> m_scrollViewDelegate;
};

} // namespace TestWebKitAPI

#endif // HAVE(UIFINDINTERACTION) && defined(__cplusplus)
