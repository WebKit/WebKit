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
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

using namespace WebKit;

static const NSInteger UnspecifiedAttachmentErrorCode = 1;
static const NSInteger InvalidAttachmentErrorCode = 2;

@implementation _WKAttachmentDisplayOptions : NSObject

- (WebCore::AttachmentDisplayOptions)coreDisplayOptions
{
    return { };
}

@end

@implementation _WKAttachmentInfo {
    RetainPtr<NSFileWrapper> _fileWrapper;
    RetainPtr<NSString> _contentType;
    RetainPtr<NSString> _filePath;
}

- (instancetype)initWithFileWrapper:(NSFileWrapper *)fileWrapper filePath:(NSString *)filePath contentType:(NSString *)contentType
{
    if (!(self = [super init]))
        return nil;

    _fileWrapper = fileWrapper;
    _filePath = filePath;
    _contentType = contentType;
    return self;
}

- (NSData *)data
{
    if (![_fileWrapper isRegularFile]) {
        // FIXME: Handle attachments backed by NSFileWrappers that represent directories.
        return nil;
    }

    return [_fileWrapper regularFileContents];
}

- (NSString *)name
{
    if ([_fileWrapper filename].length)
        return [_fileWrapper filename];

    return [_fileWrapper preferredFilename];
}

- (NSString *)filePath
{
    return _filePath.get();
}

static BOOL isDeclaredOrDynamicTypeIdentifier(NSString *type)
{
    return UTTypeIsDeclared((CFStringRef)type) || UTTypeIsDynamic((CFStringRef)type);
}

- (NSString *)_typeIdentifierFromPathExtension
{
    if (NSString *extension = self.name.pathExtension)
        return WebCore::MIMETypeRegistry::getMIMETypeForExtension(extension);

    return nil;
}

- (NSString *)contentType
{
    // A "content type" can refer to either a UTI or a MIME type. We prefer MIME type here to preserve existing behavior.
    return self.mimeType ?: self.utiType;
}

- (NSString *)mimeType
{
    NSString *contentType = [_contentType length] ? _contentType.get() : [self _typeIdentifierFromPathExtension];
    if (!isDeclaredOrDynamicTypeIdentifier(contentType))
        return contentType;

    auto mimeType = adoptCF(UTTypeCopyPreferredTagWithClass((CFStringRef)contentType, kUTTagClassMIMEType));
    return (NSString *)mimeType.autorelease();
}

- (NSString *)utiType
{
    NSString *contentType = [_contentType length] ? _contentType.get() : [self _typeIdentifierFromPathExtension];
    if (isDeclaredOrDynamicTypeIdentifier(contentType))
        return contentType;

    auto utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)contentType, nil));
    return (NSString *)utiType.autorelease();
}

- (NSFileWrapper *)fileWrapper
{
    return _fileWrapper.get();
}

@end

@implementation _WKAttachment

- (API::Object&)_apiObject
{
    return *_attachment;
}

- (_WKAttachmentInfo *)info
{
    if (!_attachment->isValid())
        return nil;

    return [[[_WKAttachmentInfo alloc] initWithFileWrapper:_attachment->fileWrapper() filePath:_attachment->filePath() contentType:_attachment->contentType()] autorelease];
}

- (void)requestInfo:(void(^)(_WKAttachmentInfo *, NSError *))completionHandler
{
    completionHandler(self.info, nil);
}

- (void)setDisplayOptions:(_WKAttachmentDisplayOptions *)options completion:(void(^)(NSError *))completionHandler
{
    if (!_attachment->isValid()) {
        completionHandler([NSError errorWithDomain:WKErrorDomain code:InvalidAttachmentErrorCode userInfo:nil]);
        return;
    }

    auto coreOptions = options ? options.coreDisplayOptions : WebCore::AttachmentDisplayOptions { };
    _attachment->setDisplayOptions(coreOptions, [capturedBlock = makeBlockPtr(completionHandler)] (CallbackBase::Error error) {
        if (!capturedBlock)
            return;

        if (error == CallbackBase::Error::None)
            capturedBlock(nil);
        else
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:UnspecifiedAttachmentErrorCode userInfo:nil]);
    });
}

- (void)setFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType completion:(void (^)(NSError *))completionHandler
{
    if (!_attachment->isValid()) {
        completionHandler([NSError errorWithDomain:WKErrorDomain code:InvalidAttachmentErrorCode userInfo:nil]);
        return;
    }

    auto fileSize = [fileWrapper.fileAttributes[NSFileSize] unsignedLongLongValue];
    if (!fileSize && fileWrapper.regularFile)
        fileSize = fileWrapper.regularFileContents.length;

    if (!contentType.length) {
        if (NSString *pathExtension = (fileWrapper.filename.length ? fileWrapper.filename : fileWrapper.preferredFilename).pathExtension)
            contentType = WebCore::MIMETypeRegistry::getMIMETypeForExtension(pathExtension);
    }

    _attachment->setFileWrapper(fileWrapper);
    _attachment->updateAttributes(fileSize, contentType, fileWrapper.preferredFilename, [capturedBlock = makeBlockPtr(completionHandler)] (auto error) {
        if (!capturedBlock)
            return;

        if (error == CallbackBase::Error::None)
            capturedBlock(nil);
        else
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:UnspecifiedAttachmentErrorCode userInfo:nil]);
    });
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
    return _attachment->identifier();
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

#endif // WK_API_ENABLED
