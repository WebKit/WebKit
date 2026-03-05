/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#import "_WKResourceLoadInfo.h"

#import "APIFrameHandle.h"
#import "APIResourceLoadInfo.h"
#import "ResourceLoadInfo.h"
#import "_WKFrameHandleInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

static _WKResourceLoadInfoResourceType NODELETE toWKResourceLoadInfoResourceType(WebKit::ResourceLoadInfo::Type type)
{
    using namespace WebKit;
    switch (type) {
    case ResourceLoadInfo::Type::ApplicationManifest:
        return _WKResourceLoadInfoResourceTypeApplicationManifest;
    case ResourceLoadInfo::Type::Beacon:
        return _WKResourceLoadInfoResourceTypeBeacon;
    case ResourceLoadInfo::Type::CSPReport:
        return _WKResourceLoadInfoResourceTypeCSPReport;
    case ResourceLoadInfo::Type::Document:
        return _WKResourceLoadInfoResourceTypeDocument;
    case ResourceLoadInfo::Type::Fetch:
        return _WKResourceLoadInfoResourceTypeFetch;
    case ResourceLoadInfo::Type::Font:
        return _WKResourceLoadInfoResourceTypeFont;
    case ResourceLoadInfo::Type::Image:
        return _WKResourceLoadInfoResourceTypeImage;
    case ResourceLoadInfo::Type::Media:
        return _WKResourceLoadInfoResourceTypeMedia;
    case ResourceLoadInfo::Type::Object:
        return _WKResourceLoadInfoResourceTypeObject;
    case ResourceLoadInfo::Type::Other:
        return _WKResourceLoadInfoResourceTypeOther;
    case ResourceLoadInfo::Type::Ping:
        return _WKResourceLoadInfoResourceTypePing;
    case ResourceLoadInfo::Type::Script:
        return _WKResourceLoadInfoResourceTypeScript;
    case ResourceLoadInfo::Type::Stylesheet:
        return _WKResourceLoadInfoResourceTypeStylesheet;
    case ResourceLoadInfo::Type::XMLHTTPRequest:
        return _WKResourceLoadInfoResourceTypeXMLHTTPRequest;
    case ResourceLoadInfo::Type::XSLT:
        return _WKResourceLoadInfoResourceTypeXSLT;
    }
    
    ASSERT_NOT_REACHED();
    return _WKResourceLoadInfoResourceTypeOther;
}


@implementation _WKResourceLoadInfo

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKResourceLoadInfo.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _info->API::ResourceLoadInfo::~ResourceLoadInfo();
    [super dealloc];
}

- (uint64_t)resourceLoadID
{
    return _info->resourceLoadID().toUInt64();
}

- (_WKFrameHandle *)frame
{
    if (auto frameID = _info->frameID())
        return wrapper(API::FrameHandle::create(*frameID)).autorelease();
    return nil;
}

- (_WKFrameHandle *)parentFrame
{
    if (auto parentFrameID = _info->parentFrameID())
        return wrapper(API::FrameHandle::create(*parentFrameID)).autorelease();
    return nil;
}

- (NSUUID *)documentID
{
    if (auto documentID = _info->documentID())
        return documentID.value().createNSUUID().autorelease();
    return nil;
}

- (NSURL *)originalURL
{
    return _info->originalURL().createNSURL().autorelease();
}

- (NSString *)originalHTTPMethod
{
    return _info->originalHTTPMethod().createNSString().autorelease();
}

- (NSDate *)eventTimestamp
{
    return [NSDate dateWithTimeIntervalSince1970:_info->eventTimestamp().secondsSinceEpoch().seconds()];
}

- (BOOL)loadedFromCache
{
    return _info->loadedFromCache();
}

- (_WKResourceLoadInfoResourceType)resourceType
{
    return toWKResourceLoadInfoResourceType(_info->resourceLoadType());
}

- (API::Object&)_apiObject
{
    return *_info;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [super init]))
        return nil;

    RetainPtr<NSNumber> resourceLoadID = [coder decodeObjectOfClass:[NSNumber class] forKey:@"resourceLoadID"];
    if (!resourceLoadID) {
        [self release];
        return nil;
    }

    RetainPtr<_WKFrameHandle> frame = [coder decodeObjectOfClass:[_WKFrameHandle class] forKey:@"frame"];
    if (!frame) {
        [self release];
        return nil;
    }

    RetainPtr<_WKFrameHandle> parentFrame = [coder decodeObjectOfClass:[_WKFrameHandle class] forKey:@"parentFrame"];
    // parentFrame is nullable, so decoding null is ok.

    RetainPtr<NSUUID> documentID = [coder decodeObjectOfClass:NSUUID.class forKey:@"documentID"];
    // documentID is nullable, so decoding null is ok.

    RetainPtr<NSURL> originalURL = [coder decodeObjectOfClass:[NSURL class] forKey:@"originalURL"];
    if (!originalURL) {
        [self release];
        return nil;
    }

    RetainPtr<NSString> originalHTTPMethod = [coder decodeObjectOfClass:[NSString class] forKey:@"originalHTTPMethod"];
    if (!originalHTTPMethod) {
        [self release];
        return nil;
    }

    RetainPtr<NSDate> eventTimestamp = [coder decodeObjectOfClass:[NSDate class] forKey:@"eventTimestamp"];
    if (!eventTimestamp) {
        [self release];
        return nil;
    }

    RetainPtr<NSNumber> loadedFromCache = [coder decodeObjectOfClass:[NSNumber class] forKey:@"loadedFromCache"];
    if (!loadedFromCache) {
        [self release];
        return nil;
    }

    RetainPtr<NSNumber> type = [coder decodeObjectOfClass:[NSNumber class] forKey:@"type"];
    if (!type) {
        [self release];
        return nil;
    }

    WebKit::ResourceLoadInfo info {
        ObjectIdentifier<WebKit::NetworkResourceLoadIdentifierType>(resourceLoadID.get().unsignedLongLongValue),
        frame->_frameHandle->frameID(),
        parentFrame ? parentFrame->_frameHandle->frameID() : std::nullopt,
        documentID ? WTF::UUID::fromNSUUID(documentID.get()) : std::nullopt,
        originalURL.get(),
        originalHTTPMethod.get(),
        WallTime::fromRawSeconds(eventTimestamp.get().timeIntervalSince1970),
        static_cast<bool>(loadedFromCache.get().boolValue),
        static_cast<WebKit::ResourceLoadInfo::Type>(type.get().unsignedCharValue),
    };

    API::Object::constructInWrapper<API::ResourceLoadInfo>(self, WTF::move(info));

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:@(self.resourceLoadID) forKey:@"resourceLoadID"];
    [coder encodeObject:retainPtr(self.frame).get() forKey:@"frame"];
    [coder encodeObject:retainPtr(self.parentFrame).get() forKey:@"parentFrame"];
    [coder encodeObject:retainPtr(self.documentID).get() forKey:@"documentID"];
    [coder encodeObject:retainPtr(self.originalURL).get() forKey:@"originalURL"];
    [coder encodeObject:retainPtr(self.originalHTTPMethod).get() forKey:@"originalHTTPMethod"];
    [coder encodeObject:retainPtr(self.eventTimestamp).get() forKey:@"eventTimestamp"];
    [coder encodeObject:@(self.loadedFromCache) forKey:@"loadedFromCache"];
    [coder encodeObject:@(static_cast<unsigned char>(_info->resourceLoadType())) forKey:@"type"];
}

@end

