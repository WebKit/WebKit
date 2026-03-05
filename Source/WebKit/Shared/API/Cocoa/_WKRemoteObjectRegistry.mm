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
#import "WKRemoteObject.h"
#import "WKRemoteObjectCoder.h"
#import "WKSharedAPICast.h"
#import "WebPage.h"
#import "WebRemoteObjectRegistry.h"
#import "_WKRemoteObjectInterface.h"
#import <objc/runtime.h>
#import <wtf/MainThread.h>
#import <wtf/ObjCRuntimeExtras.h>

extern "C" const char *_protocol_getMethodTypeEncoding(Protocol *p, SEL sel, BOOL isRequiredMethod, BOOL isInstanceMethod);
extern "C" id __NSMakeSpecialForwardingCaptureBlock(const char *signature, void (^handler)(NSInvocation *inv)) NS_RETURNS_RETAINED; // FIXME(rdar://162217343): Despite the name, __NSMakeSpecialForwardingCaptureBlock returns +1

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
    RefPtr<WebKit::RemoteObjectRegistry> _remoteObjectRegistry;

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

    if (RetainPtr<id> remoteObjectProxy = [_remoteObjectProxies objectForKey:retainPtr(interface.identifier).get()])
        return remoteObjectProxy.autorelease();

    RetainPtr<NSString> identifier = adoptNS([interface.identifier copy]);
    auto remoteObject = adoptNS([[WKRemoteObject alloc] _initWithObjectRegistry:self interface:interface]);
    [_remoteObjectProxies setObject:remoteObject.get() forKey:identifier.get()];

    return remoteObject.autorelease();
}

- (id)_initWithWebPage:(std::reference_wrapper<WebKit::WebPage>)page
{
    if (!(self = [super init]))
        return nil;

    _remoteObjectRegistry = WebKit::WebRemoteObjectRegistry::create(self, Ref { page.get() });

    return self;
}

- (id)_initWithWebPageProxy:(std::reference_wrapper<WebKit::WebPageProxy>)page
{
    if (!(self = [super init]))
        return nil;

    _remoteObjectRegistry = WebKit::UIRemoteObjectRegistry::create(self, Ref { page.get() });

    return self;
}

- (void)_invalidate
{
    _remoteObjectRegistry = nullptr;
}

static uint64_t NODELETE generateReplyIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

- (void)_sendInvocation:(NSInvocation *)invocation interface:(_WKRemoteObjectInterface *)interface
{
    RELEASE_ASSERT(isMainRunLoop());

    std::unique_ptr<WebKit::RemoteObjectInvocation::ReplyInfo> replyInfo;

    RetainPtr<NSMethodSignature> methodSignature = invocation.methodSignature;
    for (NSUInteger i = 0, count = methodSignature.get().numberOfArguments; i < count; ++i) {
        auto type = unsafeSpan([methodSignature getArgumentTypeAtIndex:i]);

        if (!equalSpans(type, "@?"_span))
            continue;

        if (replyInfo)
            [NSException raise:NSInvalidArgumentException format:@"Only one reply block is allowed per message send. (%s)", sel_getName(invocation.selector)];

        id replyBlock = nullptr;
        [invocation getArgument:&replyBlock atIndex:i];
        if (!replyBlock)
            [NSException raise:NSInvalidArgumentException format:@"A NULL reply block was passed into a message. (%s)", sel_getName(invocation.selector)];

        const char* replyBlockSignature = _Block_signature((__bridge void*)replyBlock);

        if (!methodHasReturnType<void>([NSMethodSignature signatureWithObjCTypes:replyBlockSignature]))
            [NSException raise:NSInvalidArgumentException format:@"Return value of block argument must be 'void'. (%s)", sel_getName(invocation.selector)];

        replyInfo = makeUnique<WebKit::RemoteObjectInvocation::ReplyInfo>(generateReplyIdentifier(), String::fromLatin1(replyBlockSignature));

        // Replace the block object so we won't try to encode it.
        id null = nullptr;
        [invocation setArgument:&null atIndex:i];

        ASSERT(!_pendingReplies.contains(replyInfo->replyID));
        _pendingReplies.add(replyInfo->replyID, PendingReply(interface, invocation.selector, replyBlock));
    }

    RetainPtr<WKRemoteObjectEncoder> encoder = adoptNS([[WKRemoteObjectEncoder alloc] init]);

    [encoder encodeObject:invocation forKey:invocationKey];

    RefPtr remoteObjectRegistry = _remoteObjectRegistry;
    if (!remoteObjectRegistry)
        return;

    remoteObjectRegistry->sendInvocation(WebKit::RemoteObjectInvocation(interface.identifier, [encoder rootObjectDictionary], WTF::move(replyInfo)));
}

- (WebKit::RemoteObjectRegistry&)remoteObjectRegistry
{
    return *_remoteObjectRegistry;
}

static NSString *replyBlockSignature(Protocol *protocol, SEL selector, NSUInteger blockIndex)
{
    // Required, non-inherited method:
    const char* methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, true, true);
    // @optional, non-inherited method:
    if (!methodTypeEncoding)
        methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, false, true);

    ASSERT(methodTypeEncoding);
    if (!methodTypeEncoding)
        return nil;

    RetainPtr<NSMethodSignature> targetMethodSignature = [NSMethodSignature signatureWithObjCTypes:methodTypeEncoding];
    ASSERT(targetMethodSignature);
    if (!targetMethodSignature)
        return nil;

    return retainPtr([targetMethodSignature _signatureForBlockAtArgumentIndex:blockIndex]).get()._typeString;
}

- (void)_invokeMethod:(const WebKit::RemoteObjectInvocation&)remoteObjectInvocation
{
    auto& interfaceIdentifier = remoteObjectInvocation.interfaceIdentifier();
    RefPtr encodedInvocation = remoteObjectInvocation.encodedInvocation();
    if (!encodedInvocation)
        return;

    auto interfaceAndObject = _exportedObjects.get(interfaceIdentifier);
    if (!interfaceAndObject.second) {
        NSLog(@"Did not find a registered object for the interface \"%@\"", interfaceIdentifier.createNSString().get());
        return;
    }

    RetainPtr<_WKRemoteObjectInterface> interface = interfaceAndObject.second;

    RetainPtr decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithInterface:interface.get() rootObjectDictionary:encodedInvocation.releaseNonNull() replyToSelector:nullptr]);

    RetainPtr<NSInvocation> invocation = [decoder decodeObjectOfClass:[NSInvocation class] forKey:invocationKey];

    RetainPtr<NSMethodSignature> methodSignature = invocation.get().methodSignature;
    auto& replyInfo = remoteObjectInvocation.replyInfo();

    // Look for the block argument (if any).
    for (NSUInteger i = 0, count = methodSignature.get().numberOfArguments; i < count; ++i) {
        auto type = unsafeSpan([methodSignature getArgumentTypeAtIndex:i]);

        if (!equalSpans(type, "@?"_span))
            continue;

        // We found the block.
        // If the wire had no block signature but we expect one, we drop the message.
        if (!replyInfo) {
            NSLog(@"_invokeMethod: Expected reply block, but none provided");
            return;
        }

        RetainPtr<NSString> wireBlockSignature = [NSMethodSignature signatureWithObjCTypes:replyInfo->blockSignature.utf8().data()]._typeString;
        RetainPtr<NSString> expectedBlockSignature = replyBlockSignature(retainPtr([interface protocol]).get(), invocation.get().selector, i);

        if (!expectedBlockSignature) {
            NSLog(@"_invokeMethod: Failed to validate reply block signature: could not find local signature");
            ASSERT_NOT_REACHED();
            return;
        }

        if (!WebKit::methodSignaturesAreCompatible(wireBlockSignature.get(), expectedBlockSignature.get())) {
            NSLog(@"_invokeMethod: Failed to validate reply block signature: %@ != %@", wireBlockSignature.get(), expectedBlockSignature.get());
            return;
        }

        RetainPtr<_WKRemoteObjectRegistry> remoteObjectRegistry = self;
        uint64_t replyID = replyInfo->replyID;

        class ReplyBlockCallChecker : public WTF::ThreadSafeRefCounted<ReplyBlockCallChecker, WTF::DestructionThread::MainRunLoop> {
        public:
            static Ref<ReplyBlockCallChecker> create(_WKRemoteObjectRegistry *registry, uint64_t replyID) { return adoptRef(*new ReplyBlockCallChecker(registry, replyID)); }

            ~ReplyBlockCallChecker()
            {
                if (m_didCallReplyBlock)
                    return;

                // FIXME: Instead of not sending anything when the remote object registry is null, we should
                // keep track of all reply block checkers and invalidate them (sending the unused reply message) in
                // -[_WKRemoteObjectRegistry _invalidate].
                RefPtr remoteObjectRegistry = m_remoteObjectRegistry->_remoteObjectRegistry;
                if (!remoteObjectRegistry)
                    return;

                remoteObjectRegistry->sendUnusedReply(m_replyID);
            }

            void NODELETE didCallReplyBlock() { m_didCallReplyBlock = true; }

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
        auto replyBlock = adoptNS(__NSMakeSpecialForwardingCaptureBlock([wireBlockSignature UTF8String], [interface, remoteObjectRegistry, replyID, checker](NSInvocation *invocation) {
            auto encoder = adoptNS([[WKRemoteObjectEncoder alloc] init]);
            [encoder encodeObject:invocation forKey:invocationKey];

            if (RefPtr protectedRemoteObjectRegistry = remoteObjectRegistry->_remoteObjectRegistry)
                protectedRemoteObjectRegistry->sendReplyBlock(replyID, WebKit::UserData([encoder rootObjectDictionary]));
            checker->didCallReplyBlock();
        }));

        SUPPRESS_UNRETAINED_LOCAL id replyBlockId = replyBlock.get();
        [invocation setArgument:&replyBlockId atIndex:i];

        // Make sure that the block won't be destroyed before the invocation.
        objc_setAssociatedObject(invocation.get(), replyBlockKey, replyBlock.get(), OBJC_ASSOCIATION_RETAIN);

        break;
    }

    invocation.get().target = interfaceAndObject.first.get();

    @try {
        [invocation invoke];
    } @catch (NSException *exception) {
        NSLog(@"%@: Warning: Exception caught during invocation of received message, dropping incoming message .\nException: %@", self, exception);
    }
}

- (void)_callReplyWithID:(uint64_t)replyID blockInvocation:(const WebKit::UserData&)blockInvocation
{
    RefPtr encodedInvocation = blockInvocation.object();
    if (!encodedInvocation || encodedInvocation->type() != API::Object::Type::Dictionary)
        return;

    auto it = _pendingReplies.find(replyID);
    if (it == _pendingReplies.end())
        return;

    auto pendingReply = it->value;
    _pendingReplies.remove(it);

    auto decoder = adoptNS([[WKRemoteObjectDecoder alloc] initWithInterface:pendingReply.interface.get() rootObjectDictionary:Ref { *downcast<API::Dictionary>(encodedInvocation.get()) } replyToSelector:pendingReply.selector]);

    RetainPtr replyInvocation = [decoder decodeObjectOfClass:[NSInvocation class] forKey:invocationKey];

    [replyInvocation setTarget:pendingReply.block.get()];
    [replyInvocation invoke];
}

- (void)_releaseReplyWithID:(uint64_t)replyID
{
    _pendingReplies.remove(replyID);
}

@end
