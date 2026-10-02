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

#import "config.h"
#import "Helpers/cocoa/TextFinderTester.h"

#if PLATFORM(MAC)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"

namespace TestWebKitAPI {

TextFinderTester::TextFinderTester(TestWKWebView *webView)
    : m_webView(webView)
{
}

TextFinderTester::Result TextFinderTester::findMatches(NSString *string, NSTextFinderAsynchronousDocumentFindOptions options, NSUInteger maxResults)
{
    __block Result result;
    __block bool done = false;
    [m_webView findMatchesForString:string relativeToMatch:nil findOptions:options maxResults:maxResults resultCollector:^(NSArray *matches, BOOL didWrap) {
        result = { matches, !!didWrap };
        done = true;
    }];
    Util::run(&done);
    return result;
}

NSUInteger TextFinderTester::replaceMatches(NSArray<id<NSTextFinderAsynchronousDocumentFindMatch>> *matches, NSString *replacement)
{
    __block NSUInteger result = 0;
    __block bool done = false;
    [m_webView replaceMatches:matches withString:replacement inSelectionOnly:NO resultCollector:^(NSUInteger replacementCount) {
        result = replacementCount;
        done = true;
    }];
    Util::run(&done);
    return result;
}

void TextFinderTester::selectMatch(id<NSTextFinderAsynchronousDocumentFindMatch> match)
{
    [m_webView selectFindMatch:match completionHandler:nil];
    [m_webView waitForNextPresentationUpdate];
}

void TextFinderTester::scrollMatchToVisible(id<NSTextFinderAsynchronousDocumentFindMatch> match)
{
    [m_webView scrollFindMatchToVisible:match];
    [m_webView waitForNextPresentationUpdate];
}

RetainPtr<NSImage> TextFinderTester::imageForMatch(id<NSTextFinderAsynchronousDocumentFindMatch> match)
{
    __block RetainPtr<NSImage> result;
    __block bool done = false;
    [match generateTextImage:^(NSImage *image) {
        result = image;
        done = true;
    }];
    Util::runFor(&done, 5_s);
    return result;
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
