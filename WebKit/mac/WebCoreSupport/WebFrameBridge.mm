/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebFrameBridge.h"

#import "WebFrameInternal.h"
#import "WebFrameLoaderClient.h"
#import "WebKitStatisticsPrivate.h"
#import "WebView.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLFrameOwnerElement.h>

using namespace WebCore;

@implementation WebFrameBridge

- (void)finishInitializingWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView ownerElement:(HTMLFrameOwnerElement*)ownerElement
{
    ++WebBridgeCount;

    WebView *webView = kit(page);

    _frame = [[WebFrame alloc] _initWithWebFrameView:frameView webView:webView bridge:self];

    m_frame = new Frame(page, ownerElement, new WebFrameLoaderClient(_frame));
    m_frame->setBridge(self);
    m_frame->tree()->setName(name);
    m_frame->init();

    [self setTextSizeMultiplier:[webView textSizeMultiplier]];
}

- (id)initMainFrameWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    self = [super init];
    [self finishInitializingWithPage:page frameName:name frameView:frameView ownerElement:0];
    return self;
}

- (id)initSubframeWithOwnerElement:(HTMLFrameOwnerElement*)ownerElement frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    self = [super init];
    [self finishInitializingWithPage:ownerElement->document()->frame()->page() frameName:name frameView:frameView ownerElement:ownerElement];
    return self;
}

- (WebFrame *)webFrame
{
    return _frame;
}

- (void)close
{
    [super close];
    if (!_frame)
        return;
    --WebBridgeCount;
    [_frame release];
    _frame = nil;
}

@end
