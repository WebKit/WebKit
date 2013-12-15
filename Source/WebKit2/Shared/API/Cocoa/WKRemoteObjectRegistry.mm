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

#if WK_API_ENABLED

#import "Connection.h"
#import "ImmutableDictionary.h"
#import "MutableDictionary.h"
#import "WKConnectionRef.h"
#import "WKRemoteObject.h"
#import "WKRemoteObjectCoder.h"
#import "WKRemoteObjectInterface.h"
#import "WKSharedAPICast.h"
#import "WebConnection.h"

const char* const messageName = "WKRemoteObjectRegistryMessage";

const char* const encodedInvocationKey = "encodedInvocation";
const char* const interfaceIdentifierKey = "interfaceIdentifier";

NSString * const invocationKey = @"invocation";

using namespace WebKit;

@implementation WKRemoteObjectRegistry {
    RefPtr<WebConnection> _connection;
    RetainPtr<NSMapTable> _remoteObjectProxies;
    HashMap<String, std::pair<RetainPtr<id>, RetainPtr<WKRemoteObjectInterface>>> _exportedObjects;
}

- (void)registerExportedObject:(id)object interface:(WKRemoteObjectInterface *)interface
{
    ASSERT(!_exportedObjects.contains(interface.identifier));
    _exportedObjects.add(interface.identifier, std::make_pair<RetainPtr<id>, RetainPtr<WKRemoteObjectInterface>>(object, interface));
}

- (void)unregisterExportedObject:(id)object interface:(WKRemoteObjectInterface *)interface
{
    ASSERT(_exportedObjects.get(interface.identifier).first == object);
    ASSERT(_exportedObjects.get(interface.identifier).second == interface);

    _exportedObjects.remove(interface.identifier);
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
    [encoder encodeObject:invocation forKey:invocationKey];

    RefPtr<MutableDictionary> body = MutableDictionary::create();
    body->set(interfaceIdentifierKey, API::String::create(interface.identifier));
    body->set(encodedInvocationKey, [encoder rootObjectDictionary]);

    [self _sendMessageWithBody:body.release()];
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

    if (!toImpl(body) || toImpl(body)->type() != API::Object::Type::Dictionary)
        return NO;

    const ImmutableDictionary* dictionary = toImpl(static_cast<WKDictionaryRef>(body));

    API::String* interfaceIdentifier = dictionary->get<API::String>(interfaceIdentifierKey);
    if (!interfaceIdentifier)
        return NO;

    const ImmutableDictionary* encodedInvocation = dictionary->get<ImmutableDictionary>(encodedInvocationKey);
    if (!encodedInvocationKey)
        return NO;

    [self _invokeMessageWithInterfaceIdentifier:interfaceIdentifier->string() encodedInvocation:encodedInvocation];
    return YES;
}

- (void)_invokeMessageWithInterfaceIdentifier:(const String&)interfaceIdentifier encodedInvocation:(const ImmutableDictionary*)encodedInvocation
{
    auto interfaceAndObject = _exportedObjects.get(interfaceIdentifier);
    if (!interfaceAndObject.second) {
        NSLog(@"Did not find a registered object for the interface \"%@\"", (NSString *)interfaceIdentifier);
        return;
    }

    RetainPtr<WKRemoteObjectDecoder> decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithInterface:interfaceAndObject.second.get() rootObjectDictionary:encodedInvocation]);

    NSInvocation *invocation = nil;

    @try {
        invocation = [decoder decodeObjectOfClass:[NSInvocation class] forKey:invocationKey];
    } @catch (NSException *exception) {
        NSLog(@"Exception caught during decoding of message: %@", exception);
    }

    // FIXME: Invoke the invocation.
}

@end

#endif // WK_API_ENABLED
