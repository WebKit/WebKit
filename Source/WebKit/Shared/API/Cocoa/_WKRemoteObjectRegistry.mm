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
#import "_WKRemoteObjectRegistryInternal.h"

#import "APIDictionary.h"
#import "BlockSPI.h"
#import "Connection.h"
#import "RemoteObjectInvocation.h"
#import "UIRemoteObjectRegistry.h"
#import "UserData.h"
#import "WKConnectionRef.h"
#import "WKRemoteObject.h"
#import "WKRemoteObjectCoder.h"
#import "WKSharedAPICast.h"
#import "WebPage.h"
#import "WebRemoteObjectRegistry.h"
#import "_WKRemoteObjectInterface.h"
#import <objc/runtime.h>

extern "C" id __NSMakeSpecialForwardingCaptureBlock(const char *signature, void (^handler)(NSInvocation *inv));

static const void* replyBlockKey = &replyBlockKey;

@interface NSMethodSignature ()
- (NSString *)_typeString;
- (NSMethodSignature *)_signatureForBlockAtArgumentIndex:(NSInteger)idx;
@end

NSString * const invocationKey = @"invocation";

struct PendingReply {
    PendingReply() = default;

    PendingReply(_WKRemoteObjectInterface* interface, SEL selector, id block)
        : interface(interface)
        , selector(selector)
        , block(adoptNS([block copy]))
    {
    }

    RetainPtr<_WKRemoteObjectInterface> interface;
    SEL selector { nullptr };
    RetainPtr<id> block;
};

@implementation _WKRemoteObjectRegistry {
    std::unique_ptr<WebKit::RemoteObjectRegistry> _remoteObjectRegistry;

    RetainPtr<NSMapTable> _remoteObjectProxies;
    HashMap<String, std::pair<RetainPtr<id>, RetainPtr<_WKRemoteObjectInterface>>> _exportedObjects;

    HashMap<uint64_t, PendingReply> _pendingReplies;
}

- (void)registerExportedObject:(id)object interface:(_WKRemoteObjectInterface *)interface
{
    ASSERT(!_exportedObjects.contains(interface.identifier));
    _exportedObjects.add(interface.identifier, std::make_pair<RetainPtr<id>, RetainPtr<_WKRemoteObjectInterface>>(object, interface));
}

- (void)unregisterExportedObject:(id)object interface:(_WKRemoteObjectInterface *)interface
{
    ASSERT(_exportedObjects.get(interface.identifier).first == object);
    ASSERT(_exportedObjects.get(interface.identifier).second == interface);

    _exportedObjects.remove(interface.identifier);
}

- (id)remoteObjectProxyWithInterface:(_WKRemoteObjectInterface *)interface
{
    if (!_remoteObjectProxies)
        _remoteObjectProxies = [NSMapTable strongToWeakObjectsMapTable];

    if (id remoteObjectProxy = [_remoteObjectProxies objectForKey:interface.identifier])
        return remoteObjectProxy;

    RetainPtr<NSString> identifier = adoptNS([interface.identifier copy]);
    auto remoteObject = adoptNS([[WKRemoteObject alloc] _initWithObjectRegistry:self interface:interface]);
    [_remoteObjectProxies setObject:remoteObject.get() forKey:identifier.get()];

    return remoteObject.autorelease();
}

- (id)_initWithWebPage:(WebKit::WebPage&)page
{
    if (!(self = [super init]))
        return nil;

    _remoteObjectRegistry = makeUnique<WebKit::WebRemoteObjectRegistry>(self, page);

    return self;
}

- (id)_initWithWebPageProxy:(WebKit::WebPageProxy&)page
{
    if (!(self = [super init]))
        return nil;

    _remoteObjectRegistry = makeUnique<WebKit::UIRemoteObjectRegistry>(self, page);

    return self;
}

- (void)_invalidate
{
    _remoteObjectRegistry = nullptr;
}

static uint64_t generateReplyIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

- (void)_sendInvocation:(NSInvocation *)invocation interface:(_WKRemoteObjectInterface *)interface
{
    std::unique_ptr<WebKit::RemoteObjectInvocation::ReplyInfo> replyInfo;

    NSMethodSignature *methodSignature = invocation.methodSignature;
    for (NSUInteger i = 0, count = methodSignature.numberOfArguments; i < count; ++i) {
        const char *type = [methodSignature getArgumentTypeAtIndex:i];

        if (strcmp(type, "@?"))
            continue;

        if (replyInfo)
            [NSException raise:NSInvalidArgumentException format:@"Only one reply block is allowed per message send. (%s)", sel_getName(invocation.selector)];

        id replyBlock = nullptr;
        [invocation getArgument:&replyBlock atIndex:i];
        if (!replyBlock)
            [NSException raise:NSInvalidArgumentException format:@"A NULL reply block was passed into a message. (%s)", sel_getName(invocation.selector)];

        const char* replyBlockSignature = _Block_signature((__bridge void*)replyBlock);

        if (strcmp([NSMethodSignature signatureWithObjCTypes:replyBlockSignature].methodReturnType, "v"))
            [NSException raise:NSInvalidArgumentException format:@"Return value of block argument must be 'void'. (%s)", sel_getName(invocation.selector)];

        replyInfo = makeUnique<WebKit::RemoteObjectInvocation::ReplyInfo>(generateReplyIdentifier(), replyBlockSignature);

        // Replace the block object so we won't try to encode it.
        id null = nullptr;
        [invocation setArgument:&null atIndex:i];

        ASSERT(!_pendingReplies.contains(replyInfo->replyID));
        _pendingReplies.add(replyInfo->replyID, PendingReply(interface, invocation.selector, replyBlock));
    }

    RetainPtr<WKRemoteObjectEncoder> encoder = adoptNS([[WKRemoteObjectEncoder alloc] init]);

    [encoder encodeObject:invocation forKey:invocationKey];

    if (!_remoteObjectRegistry)
        return;

    _remoteObjectRegistry->sendInvocation(WebKit::RemoteObjectInvocation(interface.identifier, [encoder rootObjectDictionary], WTFMove(replyInfo)));
}

- (WebKit::RemoteObjectRegistry&)remoteObjectRegistry
{
    return *_remoteObjectRegistry;
}

static bool validateReplyBlockSignature(NSMethodSignature *wireBlockSignature, Protocol *protocol, SEL selector, NSUInteger blockIndex)
{
    // Required, non-inherited method:
    const char* methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, true, true);
    // @optional, non-inherited method:
    if (!methodTypeEncoding)
        methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, false, true);

    ASSERT(methodTypeEncoding);
    if (!methodTypeEncoding)
        return false;

    NSMethodSignature *targetMethodSignature = [NSMethodSignature signatureWithObjCTypes:methodTypeEncoding];
    ASSERT(targetMethodSignature);
    if (!targetMethodSignature)
        return false;
    NSMethodSignature *expectedBlockSignature = [targetMethodSignature _signatureForBlockAtArgumentIndex:blockIndex];
    ASSERT(expectedBlockSignature);

    return [wireBlockSignature isEqual:expectedBlockSignature];
}

- (void)_invokeMethod:(const WebKit::RemoteObjectInvocation&)remoteObjectInvocation
{
    auto& interfaceIdentifier = remoteObjectInvocation.interfaceIdentifier();
    auto* encodedInvocation = remoteObjectInvocation.encodedInvocation();

    auto interfaceAndObject = _exportedObjects.get(interfaceIdentifier);
    if (!interfaceAndObject.second) {
        NSLog(@"Did not find a registered object for the interface \"%@\"", (NSString *)interfaceIdentifier);
        return;
    }

    RetainPtr<_WKRemoteObjectInterface> interface = interfaceAndObject.second;

    auto decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithInterface:interface.get() rootObjectDictionary:encodedInvocation replyToSelector:nullptr]);

    NSInvocation *invocation = nil;

    @try {
        invocation = [decoder decodeObjectOfClass:[NSInvocation class] forKey:invocationKey];
    } @catch (NSException *exception) {
        NSLog(@"Exception caught during decoding of message: %@", exception);
    }

    if (auto* replyInfo = remoteObjectInvocation.replyInfo()) {
        NSMethodSignature *methodSignature = invocation.methodSignature;

        // Look for the block argument.
        for (NSUInteger i = 0, count = methodSignature.numberOfArguments; i < count; ++i) {
            const char *type = [methodSignature getArgumentTypeAtIndex:i];

            if (strcmp(type, "@?"))
                continue;

            // We found the block.
            NSMethodSignature *wireBlockSignature = [NSMethodSignature signatureWithObjCTypes:replyInfo->blockSignature.utf8().data()];

            if (!validateReplyBlockSignature(wireBlockSignature, [interface protocol], invocation.selector, i)) {
                NSLog(@"_invokeMethod: Failed to validate reply block signature: %@", wireBlockSignature._typeString);
                ASSERT_NOT_REACHED();
                continue;
            }

            RetainPtr<_WKRemoteObjectRegistry> remoteObjectRegistry = self;
            uint64_t replyID = replyInfo->replyID;

            class ReplyBlockCallChecker : public WTF::ThreadSafeRefCounted<ReplyBlockCallChecker> {
            public:
                static Ref<ReplyBlockCallChecker> create(_WKRemoteObjectRegistry *registry, uint64_t replyID) { return adoptRef(*new ReplyBlockCallChecker(registry, replyID)); }

                ~ReplyBlockCallChecker()
                {
                    if (m_didCallReplyBlock)
                        return;

                    // FIXME: Instead of not sending anything when the remote object registry is null, we should
                    // keep track of all reply block checkers and invalidate them (sending the unused reply message) in
                    // -[_WKRemoteObjectRegistry _invalidate].
                    if (!m_remoteObjectRegistry->_remoteObjectRegistry)
                        return;

                    m_remoteObjectRegistry->_remoteObjectRegistry->sendUnusedReply(m_replyID);
                }

                void didCallReplyBlock() { m_didCallReplyBlock = true; }

            private:
                ReplyBlockCallChecker(_WKRemoteObjectRegistry *registry, uint64_t replyID)
                    : m_remoteObjectRegistry(registry)
                    , m_replyID(replyID)
                {
                }

                RetainPtr<_WKRemoteObjectRegistry> m_remoteObjectRegistry;
                uint64_t m_replyID = 0;
                bool m_didCallReplyBlock = false;
            };

            RefPtr<ReplyBlockCallChecker> checker = ReplyBlockCallChecker::create(self, replyID);
            id replyBlock = __NSMakeSpecialForwardingCaptureBlock(wireBlockSignature._typeString.UTF8String, [interface, remoteObjectRegistry, replyID, checker](NSInvocation *invocation) {
                auto encoder = adoptNS([[WKRemoteObjectEncoder alloc] init]);
                [encoder encodeObject:invocation forKey:invocationKey];

                remoteObjectRegistry->_remoteObjectRegistry->sendReplyBlock(replyID, WebKit::UserData([encoder rootObjectDictionary]));
                checker->didCallReplyBlock();
            });

            [invocation setArgument:&replyBlock atIndex:i];

            // Make sure that the block won't be destroyed before the invocation.
            objc_setAssociatedObject(invocation, replyBlockKey, replyBlock, OBJC_ASSOCIATION_RETAIN);
            [replyBlock release];

            break;
        }
    }

    invocation.target = interfaceAndObject.first.get();

    @try {
        [invocation invoke];
    } @catch (NSException *exception) {
        NSLog(@"%@: Warning: Exception caught during invocation of received message, dropping incoming message .\nException: %@", self, exception);
    }
}

- (void)_callReplyWithID:(uint64_t)replyID blockInvocation:(const WebKit::UserData&)blockInvocation
{
    auto encodedInvocation = blockInvocation.object();
    if (!encodedInvocation || encodedInvocation->type() != API::Object::Type::Dictionary)
        return;

    auto it = _pendingReplies.find(replyID);
    if (it == _pendingReplies.end())
        return;

    auto pendingReply = it->value;
    _pendingReplies.remove(it);

    auto decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithInterface:pendingReply.interface.get() rootObjectDictionary:static_cast<API::Dictionary*>(encodedInvocation) replyToSelector:pendingReply.selector]);

    NSInvocation *replyInvocation = nil;

    @try {
        replyInvocation = [decoder decodeObjectOfClass:[NSInvocation class] forKey:invocationKey];
    } @catch (NSException *exception) {
        NSLog(@"Exception caught during decoding of reply: %@", exception);
        return;
    }

    [replyInvocation setTarget:pendingReply.block.get()];
    [replyInvocation invoke];
}

- (void)_releaseReplyWithID:(uint64_t)replyID
{
    _pendingReplies.remove(replyID);
}

@end
