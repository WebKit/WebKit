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
#import <WebKit/_WKAttachment.h>

#import "APIAttachment.h"
#import "WKErrorPrivate.h"
#import "_WKAttachmentInternal.h"
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

static const NSInteger UnspecifiedAttachmentErrorCode = 1;
static const NSInteger InvalidAttachmentErrorCode = 2;

@implementation _WKAttachmentDisplayOptions : NSObject
@end

@implementation _WKAttachmentInfo {
    RefPtr<const API::Attachment> _attachment;
    RetainPtr<NSString> _mimeType;
    RetainPtr<NSString> _utiType;
    RetainPtr<NSString> _filePath;
}

- (instancetype)initWithAttachment:(const API::Attachment&)attachment
{
    if (!(self = [super init]))
        return nil;

    _attachment = attachment;
    _filePath = attachment.filePath().createNSString();
    _mimeType = attachment.mimeType().createNSString();
    _utiType = attachment.utiType().createNSString();
    return self;
}

- (NSData *)data
{
    NSData *result = nil;
    _attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
        // FIXME: Handle attachments backed by NSFileWrappers that represent directories.
        result = fileWrapper.isRegularFile ? fileWrapper.regularFileContents : nil;
    });
    return result;
}

- (NSString *)name
{
    NSString *result = nil;
    _attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
        result = fileWrapper.filename.length ? fileWrapper.filename : fileWrapper.preferredFilename;
    });
    return result;
}

- (NSString *)filePath
{
    return _filePath.get();
}

- (NSFileWrapper *)fileWrapper
{
    // FIXME: This API is potentially unsafe for WebKit clients, since the file wrapper that's
    // returned could be simultaneously accessed from a background thread, due to QuickLook
    // thumbnailing. This should be replaced with a method that instead takes a callback, and
    // invokes with callback with a file wrapper in a way that guarantees thread safety.
    NSFileWrapper *result = nil;
    _attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
        result = fileWrapper;
    });
    return result;
}

- (NSString *)contentType
{
    if ([_mimeType length])
        return _mimeType.get();

    return _utiType.get();
}

- (BOOL)shouldPreserveFidelity
{
    return _attachment->associatedElementType() == WebCore::AttachmentAssociatedElementType::Source;
}

@end

@implementation _WKAttachment

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKAttachment.class, self))
        return;

    _attachment->~Attachment();

    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_attachment;
}

- (_WKAttachmentInfo *)info
{
    if (!_attachment->isValid())
        return nil;

    return adoptNS([[_WKAttachmentInfo alloc] initWithAttachment:*_attachment]).autorelease();
}

- (void)requestInfo:(void(^)(_WKAttachmentInfo *, NSError *))completionHandler
{
    completionHandler(self.info, nil);
}

- (void)setFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType completion:(void (^)(NSError *))completionHandler
{
    if (!_attachment->isValid()) {
        if (completionHandler)
            completionHandler([NSError errorWithDomain:WKErrorDomain code:InvalidAttachmentErrorCode userInfo:nil]);
        return;
    }

    // This file path member is only populated when the attachment is generated upon dropping files. When data is specified via NSFileWrapper
    // from the SPI client, the corresponding file path of the data is unknown, if it even exists at all.
    _attachment->setFilePath({ });
    _attachment->setFileWrapperAndUpdateContentType(fileWrapper, contentType);
    _attachment->updateAttributes([capturedBlock = makeBlockPtr(completionHandler)] {
        if (capturedBlock)
            capturedBlock(nil);
    });
}

- (void)setData:(NSData *)data newContentType:(NSString *)newContentType
{
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    [self setFileWrapper:fileWrapper.get() contentType:newContentType completion:nil];
}

- (void)setData:(NSData *)data newContentType:(NSString *)newContentType newFilename:(NSString *)newFilename completion:(void(^)(NSError *))completionHandler
{
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (newFilename)
        [fileWrapper setPreferredFilename:newFilename];
    [self setFileWrapper:fileWrapper.get() contentType:newContentType completion:completionHandler];
}

- (NSString *)uniqueIdentifier
{
    return _attachment->identifier().createNSString().autorelease();
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p id='%@'>", [self class], self, self.uniqueIdentifier];
}

- (BOOL)isConnected
{
    return _attachment->insertionState() == API::Attachment::InsertionState::Inserted;
}

@end
