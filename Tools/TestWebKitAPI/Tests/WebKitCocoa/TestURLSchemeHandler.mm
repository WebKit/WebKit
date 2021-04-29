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
#import "TestURLSchemeHandler.h"

#import <wtf/BlockPtr.h>

@implementation TestURLSchemeHandler {
    BlockPtr<void(WKWebView *, id <WKURLSchemeTask>)> _startURLSchemeTaskHandler;
    BlockPtr<void(WKWebView *, id <WKURLSchemeTask>)> _stopURLSchemeTaskHandler;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    if (_startURLSchemeTaskHandler)
        _startURLSchemeTaskHandler(webView, urlSchemeTask);
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    if (_stopURLSchemeTaskHandler)
        _stopURLSchemeTaskHandler(webView, urlSchemeTask);
}

- (void)setStartURLSchemeTaskHandler:(void (^)(WKWebView *, id <WKURLSchemeTask>))block
{
    _startURLSchemeTaskHandler = makeBlockPtr(block);
}

- (void (^)(WKWebView *, id <WKURLSchemeTask>))startURLSchemeTaskHandler
{
    return _startURLSchemeTaskHandler.get();
}

- (void)setStopURLSchemeTaskHandler:(void (^)(WKWebView *, id <WKURLSchemeTask>))block
{
    _startURLSchemeTaskHandler = makeBlockPtr(block);
}

- (void (^)(WKWebView *, id <WKURLSchemeTask>))stopURLSchemeTaskHandler
{
    return _stopURLSchemeTaskHandler.get();
}

@end

void respond(id<WKURLSchemeTask> task, const char* html)
{
    NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(html) textEncodingName:nil] autorelease];
    [task didReceiveResponse:response];
    [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
    [task didFinish];
}
