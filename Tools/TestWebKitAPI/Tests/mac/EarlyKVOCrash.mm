/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#import <WebKit/WebView.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/RetainPtr.h>

@interface EarlyKVOCrashResponder : NSResponder {
    RetainPtr<WebView> _webView;
}

- (instancetype)initWithWebView:(WebView *)webView;

@end

@implementation EarlyKVOCrashResponder

- (instancetype)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"hidden" options:0 context:nullptr];

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [super initWithCoder:coder]))
        return nil;

    _webView = [coder decodeObjectForKey:@"webView"];
    [_webView addObserver:self forKeyPath:@"hidden" options:0 context:nullptr];

    return self;
}

- (void)dealloc
{
    [_webView removeObserver:self forKeyPath:@"hidden" context:nullptr];

    [super dealloc];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [super encodeWithCoder:coder];

    [coder encodeObject:_webView.get() forKey:@"webView"];
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, EarlyKVOCrash)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:nil groupName:nil]);
    auto earlyKVOCrashResponder = adoptNS([[EarlyKVOCrashResponder alloc] initWithWebView:webView.get()]);

    [webView setNextResponder:earlyKVOCrashResponder.get()];

    auto data = securelyArchivedDataWithRootObject(@[webView.get(), earlyKVOCrashResponder.get()]);

    if ([webView conformsToProtocol:@protocol(NSSecureCoding)])
        unarchivedObjectOfClassesFromData([NSSet setWithObjects:[NSArray class], [WebView class], [EarlyKVOCrashResponder class], nil], data);
    else
        insecurelyUnarchiveObjectFromData(data);
}

} // namespace TestWebKitAPI
