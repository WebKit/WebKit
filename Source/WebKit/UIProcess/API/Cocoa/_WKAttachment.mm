/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "_WKAttachment.h"

#if WK_API_ENABLED

#import "APIAttachment.h"
#import "WKErrorPrivate.h"
#import "_WKAttachmentInternal.h"
#import <WebCore/AttachmentTypes.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/BlockPtr.h>

using namespace WebKit;

@implementation _WKAttachmentDisplayOptions : NSObject

- (instancetype)init
{
    if (self = [super init])
        _mode = _WKAttachmentDisplayModeAuto;

    return self;
}

- (WebCore::AttachmentDisplayOptions)coreDisplayOptions
{
    WebCore::AttachmentDisplayMode mode;
    switch (self.mode) {
    case _WKAttachmentDisplayModeAuto:
        mode = WebCore::AttachmentDisplayMode::Auto;
        break;
    case _WKAttachmentDisplayModeAsIcon:
        mode = WebCore::AttachmentDisplayMode::AsIcon;
        break;
    case _WKAttachmentDisplayModeInPlace:
        mode = WebCore::AttachmentDisplayMode::InPlace;
        break;
    default:
        ASSERT_NOT_REACHED();
        mode = WebCore::AttachmentDisplayMode::Auto;
    }
    return { mode };
}

@end

@implementation _WKAttachment

- (API::Object&)_apiObject
{
    return *_attachment;
}

- (BOOL)isEqual:(id)object
{
    return [object isKindOfClass:[_WKAttachment class]] && [self.uniqueIdentifier isEqual:[(_WKAttachment *)object uniqueIdentifier]];
}

- (void)requestData:(void(^)(NSData *, NSError *))completionHandler
{
    _attachment->requestData([ capturedBlock = makeBlockPtr(completionHandler) ] (RefPtr<WebCore::SharedBuffer> buffer, CallbackBase::Error error) {
        if (!capturedBlock)
            return;

        if (buffer && error == CallbackBase::Error::None)
            capturedBlock(buffer->createNSData().autorelease(), nil);
        else
            capturedBlock(nil, [NSError errorWithDomain:WKErrorDomain code:1 userInfo:nil]);
    });
}

- (void)setDisplayOptions:(_WKAttachmentDisplayOptions *)options completion:(void(^)(NSError *))completionHandler
{
    auto coreOptions = options ? options.coreDisplayOptions : WebCore::AttachmentDisplayOptions { };
    _attachment->setDisplayOptions(coreOptions, [capturedBlock = makeBlockPtr(completionHandler)] (CallbackBase::Error error) {
        if (!capturedBlock)
            return;

        if (error == CallbackBase::Error::None)
            capturedBlock(nil);
        else
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:1 userInfo:nil]);
    });
}

- (void)setData:(NSData *)data newContentType:(NSString *)newContentType newFilename:(NSString *)newFilename completion:(void(^)(NSError *))completionHandler
{
    auto buffer = WebCore::SharedBuffer::create(data);
    _attachment->setDataAndContentType(buffer.get(), newContentType, newFilename, [capturedBlock = makeBlockPtr(completionHandler), capturedBuffer = buffer.copyRef()] (CallbackBase::Error error) {
        if (!capturedBlock)
            return;

        if (error == CallbackBase::Error::None)
            capturedBlock(nil);
        else
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:1 userInfo:nil]);
    });
}

- (NSString *)uniqueIdentifier
{
    return _attachment->identifier();
}

- (NSUInteger)hash
{
    return [self.uniqueIdentifier hash];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ id='%@'>", [self class], self.uniqueIdentifier];
}

@end

#endif // WK_API_ENABLED
