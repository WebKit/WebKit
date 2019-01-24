/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
#import "QuickLook.h"

#if USE(QUICK_LOOK)

#import "PreviewConverter.h"
#import "ResourceRequest.h"
#import "SchemeRegistry.h"
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/cocoa/NSFileManagerSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>

namespace WebCore {

const char* QLPreviewProtocol = "x-apple-ql-id";

NSSet *QLPreviewGetSupportedMIMETypesSet()
{
    static NSSet *set = [PAL::softLink_QuickLook_QLPreviewGetSupportedMIMETypes() retain];
    return set;
}

static Lock qlPreviewConverterDictionaryLock;

static NSMutableDictionary *QLPreviewConverterDictionary()
{
    static NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] init];
    return dictionary;
}

static NSMutableDictionary *QLContentDictionary()
{
    static NSMutableDictionary *contentDictionary = [[NSMutableDictionary alloc] init];
    return contentDictionary;
}

void removeQLPreviewConverterForURL(NSURL *url)
{
    auto locker = holdLock(qlPreviewConverterDictionaryLock);
    [QLPreviewConverterDictionary() removeObjectForKey:url];
    [QLContentDictionary() removeObjectForKey:url];
}

static void addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    auto locker = holdLock(qlPreviewConverterDictionaryLock);
    [QLPreviewConverterDictionary() setObject:converter forKey:url];
    [QLContentDictionary() setObject:(fileName ? fileName : @"") forKey:url];
}

RetainPtr<NSURLRequest> registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(PAL::softLink_QuickLook_QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([QLPreviewGetSupportedMIMETypesSet() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(PAL::softLink_QuickLook_QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        auto converter = std::make_unique<PreviewConverter>(data, uti.get());
        ResourceRequest previewRequest = converter->previewRequest();

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL(previewRequest.url(), converter->platformConverter(), nil);

        return previewRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    }

    return nil;
}

bool isQuickLookPreviewURL(const URL& url)
{
    return url.protocolIs(QLPreviewProtocol);
}

static NSDictionary *temporaryFileAttributes()
{
    static NSDictionary *attributes = [@{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)],
        } retain];
    return attributes;
}

static NSDictionary *temporaryDirectoryAttributes()
{
    static NSDictionary *attributes = [@{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)],
        NSFileProtectionKey : NSFileProtectionCompleteUnlessOpen,
        } retain];
    return attributes;
}

NSString *createTemporaryFileForQuickLook(NSString *fileName)
{
    NSString *downloadDirectory = FileSystem::createTemporaryDirectory(@"QuickLookContent");
    if (!downloadDirectory)
        return nil;

    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSError *error;
    if (![fileManager setAttributes:temporaryDirectoryAttributes() ofItemAtPath:downloadDirectory error:&error]) {
        LOG_ERROR("Failed to set attribute NSFileProtectionCompleteUnlessOpen on directory %@ with error: %@.", downloadDirectory, error.localizedDescription);
        return nil;
    }

    NSString *contentPath = [downloadDirectory stringByAppendingPathComponent:fileName.lastPathComponent];
    if (![fileManager _web_createFileAtPath:contentPath contents:nil attributes:temporaryFileAttributes()]) {
        LOG_ERROR("Failed to create QuickLook temporary file at path %@.", contentPath);
        return nil;
    }

    return contentPath;
}

} // namespace WebCore

#endif // USE(QUICK_LOOK)
