/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WKConnectionInternal.h"

#import "ObjCObjectGraph.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKConnection {
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
    WeakObjCPtr<id <WKConnectionDelegate>> _delegate;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKConnection.class, self))
        return;

    self._connection.~WebConnection();

    [super dealloc];
}

static void didReceiveMessage(WKConnectionRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto connection = (__bridge WKConnection *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto delegate = connection->_delegate.get();

    if ([delegate respondsToSelector:@selector(connection:didReceiveMessageWithName:body:)]) {
        auto name = bridge_cast(adoptCF(WKStringCopyCFString(kCFAllocatorDefault, messageName)));
        id body = static_cast<WebKit::ObjCObjectGraph*>(WebKit::toImpl(messageBody))->rootObject();
        [delegate connection:connection didReceiveMessageWithName:name.get() body:body];
    }
}

static void didClose(WKConnectionRef, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto connection = (__bridge WKConnection *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto delegate = connection->_delegate.get();

    if ([delegate respondsToSelector:@selector(connectionDidClose:)])
        [delegate connectionDidClose:connection];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpClient(WKConnection *wrapper, WebKit::WebConnection& connection)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKConnectionClientV0 client;
    memset(&client, 0, sizeof(client));

    client.base.version = 0;
    client.base.clientInfo = (__bridge void*)wrapper;
    client.didReceiveMessage = didReceiveMessage;
    client.didClose = didClose;

    connection.initializeConnectionClient(&client.base);
}

- (id <WKConnectionDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id <WKConnectionDelegate>)delegate
{
    _delegate = delegate;
    if (delegate)
        setUpClient(self, self._connection);
    else
        self._connection.initializeConnectionClient(nullptr);
}

- (void)sendMessageWithName:(NSString *)messageName body:(id)messageBody
{
    auto wkMessageBody = WebKit::ObjCObjectGraph::create(messageBody);
    self._connection.postMessage(messageName, wkMessageBody.ptr());
}

- (WebKit::WebConnection&)_connection
{
    return static_cast<WebKit::WebConnection&>(API::Object::fromWKObjectExtraSpace(self));
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return API::Object::fromWKObjectExtraSpace(self);
}

@end
