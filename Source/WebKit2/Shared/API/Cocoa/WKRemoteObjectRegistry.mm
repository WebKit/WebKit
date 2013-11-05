/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKRemoteObjectRegistryInternal.h"

#import "Connection.h"
#import "ImmutableDictionary.h"
#import "WKConnectionRef.h"
#import "WKRemoteObject.h"
#import "WKRemoteObjectCoder.h"
#import "WKRemoteObjectInterface.h"
#import "WKSharedAPICast.h"
#import "WebConnection.h"

#if WK_API_ENABLED

const char* const messageName = "WKRemoteObjectRegistryMessage";

NSString * const interfaceIdentifierKey = @"interfaceIdentifier";

using namespace WebKit;

@implementation WKRemoteObjectRegistry {
    RefPtr<WebConnection> _connection;
    RetainPtr<NSMapTable> _remoteObjectProxies;
}

- (void)registerExportedObject:(id)object interface:(WKRemoteObjectInterface *)interface
{
    // FIXME: Implement.
}

- (void)unregisterExportedObject:(id)object interface:(WKRemoteObjectInterface *)interface
{
    // FIXME: Implement.
}

- (id)remoteObjectProxyWithInterface:(WKRemoteObjectInterface *)interface
{
    if (!_remoteObjectProxies)
        _remoteObjectProxies = [NSMapTable strongToWeakObjectsMapTable];

    if (id remoteObjectProxy = [_remoteObjectProxies objectForKey:interface.identifier])
        return remoteObjectProxy;

    RetainPtr<NSString> identifier = adoptNS([interface.identifier copy]);
    RetainPtr<WKRemoteObject> remoteObject = adoptNS([[WKRemoteObject alloc] _initWithObjectRegistry:self interface:interface]);
    [_remoteObjectProxies setObject:remoteObject.get() forKey:identifier.get()];

    return [remoteObject.leakRef() autorelease];
}

- (void)_sendInvocation:(NSInvocation *)invocation interface:(WKRemoteObjectInterface *)interface
{
    RetainPtr<WKRemoteObjectEncoder> encoder = adoptNS([[WKRemoteObjectEncoder alloc] init]);

    [encoder encodeObject:interface.identifier forKey:interfaceIdentifierKey];
    [encoder encodeObject:invocation forKey:@"invocation"];

    [self _sendMessageWithBody:[encoder rootObjectDictionary]];
}

- (void)_sendMessageWithBody:(PassRefPtr<ImmutableDictionary>)body
{
    _connection->postMessage(messageName, body.get());
}

@end

@implementation WKRemoteObjectRegistry (WKPrivate)

- (id)_initWithConnectionRef:(WKConnectionRef)connectionRef
{
    if (!(self = [super init]))
        return nil;

    _connection = toImpl(connectionRef);

    return self;
}

- (BOOL)_handleMessageWithName:(WKStringRef)name body:(WKTypeRef)body
{
    if (toImpl(name)->string() != messageName)
        return NO;

    if (!toImpl(body) || toImpl(body)->type() != APIObject::TypeDictionary)
        return NO;

    [self _invokeMessageWithBody:toImpl((WKDictionaryRef)body)];
    return YES;
}

- (void)_invokeMessageWithBody:(ImmutableDictionary*)body
{
    RetainPtr<WKRemoteObjectDecoder> decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithRootObjectDictionary:body]);

    NSString *interfaceIdentifier = nil;

    // FIXME: Decode the invocation.
    @try {
        interfaceIdentifier = [decoder decodeObjectOfClass:[NSString class] forKey:interfaceIdentifierKey];
    } @catch (NSException *exception) {
        NSLog(@"Exception caught during decoding of message: %@", exception);
    }

    // FIXME: Invoke the invocation.
}

@end

#endif // WK_API_ENABLED
