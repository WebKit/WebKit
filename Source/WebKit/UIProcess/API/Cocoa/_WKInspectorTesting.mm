/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "_WKInspectorInternal.h"
#import "_WKInspectorPrivateForTesting.h"

// This file exists to centralize all fragile code that is used by _WKInspector API tests. The tests
// trigger WebInspectorUI behavior by evaluating JavaScript or by calling internal methods.

static NSString *JavaScriptSnippetToOpenURLExternally(NSURL *url)
{
    return [NSString stringWithFormat:@"InspectorFrontendHost.openURLExternally(\"%@\")", url.absoluteString];
}

static NSString *JavaScriptSnippetToFetchURL(NSURL *url)
{
    return [NSString stringWithFormat:@"fetch(\"%@\")", url.absoluteString];
}

@implementation _WKInspector (WKTesting)

- (WKWebView *)inspectorWebView
{
    auto page = _inspector->inspectorPage();
    return page ? page->cocoaView().autorelease() : nil;
}

- (void)_fetchURLForTesting:(NSURL *)url
{
    _inspector->evaluateInFrontendForTesting(JavaScriptSnippetToFetchURL(url));
}

- (void)_openURLExternallyForTesting:(NSURL *)url useFrontendAPI:(BOOL)useFrontendAPI
{
    if (useFrontendAPI)
        _inspector->evaluateInFrontendForTesting(JavaScriptSnippetToOpenURLExternally(url));
    else {
        // Force the navigation request to be handled naturally through the
        // internal NavigationDelegate of WKInspectorViewController.
        [self.inspectorWebView loadRequest:[NSURLRequest requestWithURL:url]];
    }
}

@end
