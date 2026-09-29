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

#import "ResourceRequest.h"
#import <pal/spi/cocoa/NSFileManagerSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>

#import <pal/ios/QuickLookSoftLink.h>

namespace WebCore {

NSSet *QLPreviewGetSupportedMIMETypesSetSingleton()
{
    static NeverDestroyed<RetainPtr<NSSet>> set = PAL::softLink_QuickLook_QLPreviewGetSupportedMIMETypes();
    return set.get().get();
}

static Lock qlPreviewConverterDictionaryLock;

static NSMutableDictionary *QLPreviewConverterDictionarySingleton() WTF_REQUIRES_LOCK(qlPreviewConverterDictionaryLock)
{
    static NeverDestroyed<RetainPtr<NSMutableDictionary>> dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    return dictionary.get();
}

static NSMutableDictionary *QLContentDictionarySingleton()
{
    static NeverDestroyed<RetainPtr<NSMutableDictionary>> contentDictionary = adoptNS([[NSMutableDictionary alloc] init]);
    return contentDictionary.get();
}

void removeQLPreviewConverterForURL(NSURL *url)
{
    Locker locker { qlPreviewConverterDictionaryLock };
    [QLPreviewConverterDictionarySingleton() removeObjectForKey:url];
    [QLContentDictionarySingleton() removeObjectForKey:url];
}

static void addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    Locker locker { qlPreviewConverterDictionaryLock };
    [QLPreviewConverterDictionarySingleton() setObject:converter forKey:url];
    [QLContentDictionarySingleton() setObject:(fileName ? fileName : @"") forKey:url];
}

RetainPtr<NSURLRequest> registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(PAL::softLink_QuickLook_QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([QLPreviewGetSupportedMIMETypesSetSingleton() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(PAL::softLink_QuickLook_QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        auto converter = adoptNS([PAL::allocQLPreviewConverterInstance() initWithData:data name:nil uti:uti.get() options:nil]);
        ResourceRequest previewRequest = [converter previewRequest];

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL(previewRequest.url().createNSURL().get(), converter.get(), nil);

        return previewRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    }

    return nil;
}

bool isQuickLookPreviewURL(const URL& url)
{
    return url.protocolIs(QLPreviewProtocol);
}

static NSDictionary *temporaryFileAttributesSingleton()
{
    static NeverDestroyed<RetainPtr<NSDictionary>> attributes = @{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)],
    };
    return attributes.get().get();
}

static NSDictionary *temporaryDirectoryAttributesSingleton()
{
    static NeverDestroyed<RetainPtr<NSDictionary>> attributes = @{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)],
        NSFileProtectionKey : NSFileProtectionCompleteUnlessOpen,
    };
    return attributes.get().get();
}

NSString *createTemporaryFileForQuickLook(NSString *fileName)
{
    RetainPtr downloadDirectory = FileSystem::createTemporaryDirectory(@"QuickLookContent");
    if (!downloadDirectory)
        return nil;

    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSError *error;
    if (![fileManager setAttributes:temporaryDirectoryAttributesSingleton() ofItemAtPath:downloadDirectory error:&error]) {
        LOG_ERROR("Failed to set attribute NSFileProtectionCompleteUnlessOpen on directory %@ with error: %@.", downloadDirectory.get(), error.localizedDescription);
        return nil;
    }

    NSString *contentPath = [downloadDirectory stringByAppendingPathComponent:fileName.lastPathComponent];
    if (![fileManager _web_createFileAtPath:contentPath contents:nil attributes:temporaryFileAttributesSingleton()]) {
        LOG_ERROR("Failed to create QuickLook temporary file at path %@.", contentPath);
        return nil;
    }

    return contentPath;
}

} // namespace WebCore

#endif // USE(QUICK_LOOK)
