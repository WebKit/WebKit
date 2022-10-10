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

#import "WKSecurityOriginInternal.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "_WKFrameHandleInternal.h"
#import "_WKFrameTreeNodeInternal.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKFrameTreeNode

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKFrameTreeNode.class, self))
        return;
    _node->API::FrameTreeNode::~FrameTreeNode();
    [super dealloc];
}

- (BOOL)isMainFrame
{
    return _node->isMainFrame();
}

- (NSURLRequest *)request
{
    return _node->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (WKSecurityOrigin *)securityOrigin
{
    auto& data = _node->securityOrigin();
    auto apiOrigin = API::SecurityOrigin::create(data.protocol, data.host, data.port);
    return retainPtr(wrapper(apiOrigin.get())).autorelease();
}

- (WKWebView *)webView
{
    return _node->page().cocoaView().autorelease();
}

- (NSArray<_WKFrameTreeNode *> *)childFrames
{
    return createNSArray(_node->childFrames(), [&] (auto& child) {
        return wrapper(API::FrameTreeNode::create(WebKit::FrameTreeNodeData(child), _node->page()));
    }).autorelease();
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (_WKFrameHandle *)_handle
{
    return retainPtr(wrapper(_node->handle())).autorelease();
}

- (_WKFrameHandle *)_parentFrameHandle
{
    return retainPtr(wrapper(_node->parentFrameHandle())).autorelease();
}

- (pid_t)_processIdentifier
{
    auto* frame = WebKit::WebFrameProxy::webFrame(_node->handle()->frameID());
    return frame ? frame->processIdentifier() : 0;
}

- (API::Object&)_apiObject
{
    return *_node;
}

@end
