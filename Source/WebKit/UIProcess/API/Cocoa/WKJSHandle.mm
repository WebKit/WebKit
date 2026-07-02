/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKJSHandleInternal.h"

#import "APIJSHandle.h"
#import "FrameInfoData.h"
#import "WKContentWorldInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKRemoteObjectCoder.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebKitJSHandle.h>
#import <wtf/BlockPtr.h>

@implementation WKJSHandle

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKJSHandle.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _ref->API::JSHandle::~JSHandle();
    [super dealloc];
}

- (WKFrameInfo *)frame
{
    return wrapper(API::FrameInfo::create(WebKit::FrameInfoData { _ref->info().frameInfo })).autorelease();
}

- (WKContentWorld *)world
{
    return wrapper(API::ContentWorld::worldForIdentifier(_ref->info().worldIdentifier));
}

- (void)windowProxyFrameInfo:(void (^)(WKFrameInfo *))completionHandler
{
    RefPtr webFrame = WebKit::WebFrameProxy::webFrame(_ref->info().windowProxyFrameIdentifier);
    if (!webFrame)
        return completionHandler(nil);

    webFrame->getFrameInfo([completionHandler = makeBlockPtr(completionHandler)] (auto&& data) mutable {
        if (!data)
            return completionHandler(nil);
        completionHandler(wrapper(API::FrameInfo::create(WTF::move(*data))).get());
    });
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    if (![object isKindOfClass:self.class])
        return NO;

    return _ref->info() == ((WKJSHandle *)object)->_ref->info();
}

- (NSUInteger)hash
{
    return _ref->info().identifier.object().toUInt64();
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (API::Object&)_apiObject
{
    return *_ref;
}

@end

@implementation _WKJSHandle

- (void)windowFrameInfo:(void (^)(WKFrameInfo * _Nullable))completionHandler
{
    [self windowProxyFrameInfo:completionHandler];
}

@end
