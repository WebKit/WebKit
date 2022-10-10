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
#import "_WKResourceLoadInfo.h"

#import "APIFrameHandle.h"
#import "APIResourceLoadInfo.h"
#import "ResourceLoadInfo.h"
#import "_WKFrameHandleInternal.h"
#import "_WKResourceLoadInfoInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

static _WKResourceLoadInfoResourceType toWKResourceLoadInfoResourceType(WebKit::ResourceLoadInfo::Type type)
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
    _info->API::ResourceLoadInfo::~ResourceLoadInfo();
    [super dealloc];
}

- (uint64_t)resourceLoadID
{
    return _info->resourceLoadID().toUInt64();
}

- (_WKFrameHandle *)frame
{
    if (auto frameID = _info->frameID())
        return wrapper(API::FrameHandle::create(*frameID));
    return nil;
}

- (_WKFrameHandle *)parentFrame
{
    if (auto parentFrameID = _info->parentFrameID())
        return wrapper(API::FrameHandle::create(*parentFrameID));
    return nil;
}

- (NSURL *)originalURL
{
    return _info->originalURL();
}

- (NSString *)originalHTTPMethod
{
    return _info->originalHTTPMethod();
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

    NSNumber *resourceLoadID = [coder decodeObjectOfClass:[NSNumber class] forKey:@"resourceLoadID"];
    if (!resourceLoadID) {
        [self release];
        return nil;
    }

    _WKFrameHandle *frame = [coder decodeObjectOfClass:[_WKFrameHandle class] forKey:@"frame"];
    if (!frame) {
        [self release];
        return nil;
    }

    _WKFrameHandle *parentFrame = [coder decodeObjectOfClass:[_WKFrameHandle class] forKey:@"parentFrame"];
    // parentFrame is nullable, so decoding null is ok.

    NSURL *originalURL = [coder decodeObjectOfClass:[NSURL class] forKey:@"originalURL"];
    if (!originalURL) {
        [self release];
        return nil;
    }

    NSString *originalHTTPMethod = [coder decodeObjectOfClass:[NSString class] forKey:@"originalHTTPMethod"];
    if (!originalHTTPMethod) {
        [self release];
        return nil;
    }

    NSDate *eventTimestamp = [coder decodeObjectOfClass:[NSDate class] forKey:@"eventTimestamp"];
    if (!eventTimestamp) {
        [self release];
        return nil;
    }

    NSNumber *loadedFromCache = [coder decodeObjectOfClass:[NSNumber class] forKey:@"loadedFromCache"];
    if (!loadedFromCache) {
        [self release];
        return nil;
    }

    NSNumber *type = [coder decodeObjectOfClass:[NSNumber class] forKey:@"type"];
    if (!type) {
        [self release];
        return nil;
    }

    WebKit::ResourceLoadInfo info {
        makeObjectIdentifier<WebKit::NetworkResourceLoadIdentifierType>(resourceLoadID.unsignedLongLongValue),
        frame->_frameHandle->frameID(),
        parentFrame ? std::optional<WebCore::FrameIdentifier>(parentFrame->_frameHandle->frameID()) : std::nullopt,
        originalURL,
        originalHTTPMethod,
        WallTime::fromRawSeconds(eventTimestamp.timeIntervalSince1970),
        static_cast<bool>(loadedFromCache.boolValue),
        static_cast<WebKit::ResourceLoadInfo::Type>(type.unsignedCharValue),
    };

    API::Object::constructInWrapper<API::ResourceLoadInfo>(self, WTFMove(info));

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:@(self.resourceLoadID) forKey:@"resourceLoadID"];
    [coder encodeObject:self.frame forKey:@"frame"];
    [coder encodeObject:self.parentFrame forKey:@"parentFrame"];
    [coder encodeObject:self.originalURL forKey:@"originalURL"];
    [coder encodeObject:self.originalHTTPMethod forKey:@"originalHTTPMethod"];
    [coder encodeObject:self.eventTimestamp forKey:@"eventTimestamp"];
    [coder encodeObject:@(self.loadedFromCache) forKey:@"loadedFromCache"];
    [coder encodeObject:@(static_cast<unsigned char>(_info->resourceLoadType())) forKey:@"type"];
}

@end

