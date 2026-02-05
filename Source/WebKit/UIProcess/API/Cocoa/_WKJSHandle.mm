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
#import "_WKJSHandleInternal.h"

#import "FrameInfoData.h"
#import "WKContentWorldInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKRemoteObjectCoder.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebKitJSHandle.h>
#import <wtf/BlockPtr.h>

@implementation _WKJSHandle

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKJSHandle.class, self))
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

- (void)windowFrameInfo:(void (^)(WKFrameInfo *))completionHandler
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

    return _ref->info() == ((_WKJSHandle *)object)->_ref->info();
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

// NSSecureCoding implementation.
// This is for injected bundle transition support only.
// Do not include this in a public API version of WKJSHandle.
+ (BOOL)supportsSecureCoding
{
#if PLATFORM(MAC)
    RELEASE_ASSERT(WTF::MacApplication::isSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#else
    RELEASE_ASSERT(WTF::IOSApplication::isMobileSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#endif
    return YES;
}

static NSString* const identifierObjectKey = @"a";
static NSString* const identifierProcessIdentifierKey = @"b";
static NSString* const worldIdentifierObjectKey = @"c";
static NSString* const worldIdentifierProcessIdentifierKey = @"d";
static NSString* const frameIdentifierKey = @"e";
static NSString* const documentIDProcessIdentifierKey = @"f";
static NSString* const worldIdentifierHighBitsKey = @"g";
static NSString* const worldIdentifierLowBitsKey = @"h";

- (id)initWithCoder:(NSCoder *)decoder
{
    if (!(self = [super init]))
        return nil;

#if PLATFORM(MAC)
    RELEASE_ASSERT(WTF::MacApplication::isSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#else
    RELEASE_ASSERT(WTF::IOSApplication::isMobileSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#endif
    RELEASE_ASSERT(!isInAuxiliaryProcess());

    uint64_t rawIdentifierObject = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:identifierObjectKey]) unsignedLongLongValue];
    if (!WebCore::WebProcessJSHandleIdentifier::isValidIdentifier(rawIdentifierObject)) {
        [self release];
        return nil;
    }

    uint64_t rawIdentifierProcessIdentifier = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:identifierProcessIdentifierKey]) unsignedLongLongValue];
    if (!WebCore::ProcessIdentifier::isValidIdentifier(rawIdentifierProcessIdentifier)) {
        [self release];
        return nil;
    }

    uint64_t rawWorldIdentifierObject = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:worldIdentifierObjectKey]) unsignedLongLongValue];
    if (!WebKit::NonProcessQualifiedContentWorldIdentifier::isValidIdentifier(rawWorldIdentifierObject)) {
        [self release];
        return nil;
    }

    uint64_t rawWorldIdentifierProcessIdentifier = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:worldIdentifierProcessIdentifierKey]) unsignedLongLongValue];
    if (!WebCore::ProcessIdentifier::isValidIdentifier(rawWorldIdentifierProcessIdentifier)) {
        [self release];
        return nil;
    }

    uint64_t frameIdentifier = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:frameIdentifierKey]) unsignedLongLongValue];
    if (!WebCore::FrameIdentifier::isValidIdentifier(frameIdentifier)) {
        [self release];
        return nil;
    }

    uint64_t documentIDProcessIdentifier = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:documentIDProcessIdentifierKey]) unsignedLongLongValue];
    uint64_t worldIdentifierHighBits = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:worldIdentifierHighBitsKey]) unsignedLongLongValue];
    uint64_t worldIdentifierLowBits = [dynamic_objc_cast<NSNumber>([decoder decodeObjectOfClass:NSNumber.class forKey:worldIdentifierLowBitsKey]) unsignedLongLongValue];

    Markable<WebCore::ScriptExecutionContextIdentifier> documentID;
    if (WebCore::ProcessIdentifier::isValidIdentifier(documentIDProcessIdentifier) && WTF::UUID::isValid(worldIdentifierHighBits, worldIdentifierLowBits))
        documentID = WebCore::ScriptExecutionContextIdentifier { WTF::UUID(worldIdentifierHighBits, worldIdentifierLowBits), WebCore::ProcessIdentifier(documentIDProcessIdentifier) };

    WebCore::JSHandleIdentifier identifier { WebCore::WebProcessJSHandleIdentifier(rawIdentifierObject), WebCore::ProcessIdentifier(rawIdentifierProcessIdentifier) };
    WebKit::ContentWorldIdentifier worldIdentifier { WebKit::NonProcessQualifiedContentWorldIdentifier(rawWorldIdentifierObject), WebCore::ProcessIdentifier(rawWorldIdentifierProcessIdentifier) };

    auto frameInfo = WebKit::legacyEmptyFrameInfo({ });
    frameInfo.documentID = documentID;
    frameInfo.frameID = WebCore::FrameIdentifier(frameIdentifier);

    WebKit::JSHandleInfo info {
        identifier,
        worldIdentifier,
        WTF::move(frameInfo),
        std::nullopt
    };

    API::Object::constructInWrapper<API::JSHandle>(self, WTF::move(info));

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
#if PLATFORM(MAC)
    RELEASE_ASSERT(WTF::MacApplication::isSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#else
    RELEASE_ASSERT(WTF::IOSApplication::isMobileSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#endif
    RELEASE_ASSERT(isInWebProcess());

    const auto& info = _ref->info();
    WebCore::WebKitJSHandle::jsHandleSentToAnotherProcess(info.identifier);

    [coder encodeObject:@(info.identifier.object().toUInt64()) forKey:identifierObjectKey];
    [coder encodeObject:@(info.identifier.processIdentifier().toUInt64()) forKey:identifierProcessIdentifierKey];

    [coder encodeObject:@(info.worldIdentifier.object().toUInt64()) forKey:worldIdentifierObjectKey];
    [coder encodeObject:@(info.worldIdentifier.processIdentifier().toUInt64()) forKey:worldIdentifierProcessIdentifierKey];

    [coder encodeObject:@(info.frameInfo.frameID.toUInt64()) forKey:frameIdentifierKey];

    if (info.frameInfo.documentID) {
        [coder encodeObject:@(info.frameInfo.documentID->processIdentifier().toUInt64()) forKey:documentIDProcessIdentifierKey];
        [coder encodeObject:@(info.frameInfo.documentID->object().high()) forKey:worldIdentifierHighBitsKey];
        [coder encodeObject:@(info.frameInfo.documentID->object().low()) forKey:worldIdentifierLowBitsKey];
    }

    // Remaining information is currently not needed, and this is on its way to being removed so let's not add more here.
    // This also avoids encoding any complex types.
}

@end
