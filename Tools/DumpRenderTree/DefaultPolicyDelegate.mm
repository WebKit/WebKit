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
#import "DefaultPolicyDelegate.h"

#import "DumpRenderTree.h"
#import "TestRunner.h"
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/EnumTraits.h>

@implementation DefaultPolicyDelegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id <WebPolicyDecisionListener>)listener
{
    if ([WebView _canHandleRequest:request]) {
        if (![frame frameElement])
            gTestRunner->willNavigate();
        [listener use];
        return;
    }

    WebNavigationType navType = (WebNavigationType)[[actionInformation objectForKey:WebActionNavigationTypeKey] intValue];
    if (std::to_underlying(navType) == std::to_underlying(WebNavigationTypePlugInRequest)) {
        if (![frame frameElement])
            gTestRunner->willNavigate();
        [listener use];
        return;
    }

    // The default WebKit policy delegate passes the URL along to -[NSWorkspace openURL:] here,
    // but we don't want to do that so we just ignore the navigation completely.
    [listener ignore];
}

@end
