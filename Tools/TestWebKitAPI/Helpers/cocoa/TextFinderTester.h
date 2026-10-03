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

#if PLATFORM(MAC) && defined(__cplusplus)

#import <WebKit/WKWebView.h>
#import <pal/spi/mac/NSTextFinderSPI.h>
#import <wtf/RetainPtr.h>

@class TestWKWebView;

@interface WKWebView (NSTextFinderSupport)
- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(id<NSTextFinderAsynchronousDocumentFindMatch>)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector;
- (void)replaceMatches:(NSArray<id<NSTextFinderAsynchronousDocumentFindMatch>> *)matches withString:(NSString *)replacementString inSelectionOnly:(BOOL)selectionOnly resultCollector:(void (^)(NSUInteger replacementCount))resultCollector;
- (void)selectFindMatch:(id<NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(void))completionHandler;
- (void)scrollFindMatchToVisible:(id<NSTextFinderAsynchronousDocumentFindMatch>)findMatch;
@end

namespace TestWebKitAPI {

class TextFinderTester {
public:
    explicit TextFinderTester(TestWKWebView *);

    struct Result {
        RetainPtr<NSArray<id<NSTextFinderAsynchronousDocumentFindMatch>>> matches;
        bool didWrap { false };
    };
    Result findMatches(NSString *, NSTextFinderAsynchronousDocumentFindOptions = static_cast<NSTextFinderAsynchronousDocumentFindOptions>(0), NSUInteger maxResults = 1000);
    NSUInteger replaceMatches(NSArray<id<NSTextFinderAsynchronousDocumentFindMatch>> *, NSString *replacement);
    void selectMatch(id<NSTextFinderAsynchronousDocumentFindMatch>);
    void scrollMatchToVisible(id<NSTextFinderAsynchronousDocumentFindMatch>);
    RetainPtr<NSImage> imageForMatch(id<NSTextFinderAsynchronousDocumentFindMatch>);

private:
    RetainPtr<TestWKWebView> m_webView;
};

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && defined(__cplusplus)
