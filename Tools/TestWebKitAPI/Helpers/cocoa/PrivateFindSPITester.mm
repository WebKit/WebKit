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
#import "Helpers/cocoa/PrivateFindSPITester.h"

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFindDelegate.h>

@interface PrivateFindSPITesterDelegate : NSObject <_WKFindDelegate>
@property (nonatomic) bool done;
@property (nonatomic) bool found;
@property (nonatomic) NSUInteger matchCount;
@property (nonatomic) NSInteger matchIndex;
@end

@implementation PrivateFindSPITesterDelegate

- (void)_webView:(WKWebView *)webView didCountMatches:(NSUInteger)matches forString:(NSString *)string
{
    _matchCount = matches;
    _done = true;
}

- (void)_webView:(WKWebView *)webView didFindMatches:(NSUInteger)matches forString:(NSString *)string withMatchIndex:(NSInteger)matchIndex
{
    _found = true;
    _matchCount = matches;
    _matchIndex = matchIndex;
    _done = true;
}

- (void)_webView:(WKWebView *)webView didFailToFindString:(NSString *)string
{
    _found = false;
    _matchCount = 0;
    _matchIndex = 0;
    _done = true;
}

@end

namespace TestWebKitAPI {

PrivateFindSPITester::PrivateFindSPITester(TestWKWebView *webView)
    : m_webView(webView)
    , m_delegate(adoptNS([PrivateFindSPITesterDelegate new]))
{
    [m_webView _setFindDelegate:m_delegate.get()];
}

PrivateFindSPITester::~PrivateFindSPITester() = default;

PrivateFindSPITester::Result PrivateFindSPITester::findString(NSString *string, _WKFindOptions options, NSUInteger maxCount)
{
    [m_delegate setDone:false];
    [m_webView _findString:string options:options maxCount:maxCount];
    while (![m_delegate done])
        Util::spinRunLoop();
    return { [m_delegate found], static_cast<unsigned>([m_delegate matchCount]), static_cast<int>([m_delegate matchIndex]) };
}

unsigned PrivateFindSPITester::countStringMatches(NSString *string, _WKFindOptions options, NSUInteger maxCount)
{
    [m_delegate setDone:false];
    [m_webView _countStringMatches:string options:options maxCount:maxCount];
    while (![m_delegate done])
        Util::spinRunLoop();
    return [m_delegate matchCount];
}

void PrivateFindSPITester::hideFindUI()
{
    [m_webView _hideFindUI];
    [m_webView waitForNextPresentationUpdate];
}

} // namespace TestWebKitAPI
