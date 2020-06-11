/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#import "WKOpenPanelParametersInternal.h"
#import <pal/spi/cocoa/CoreServicesSPI.h>

#if PLATFORM(MAC)

#import "WKNSArray.h"

static NSDictionary<NSString *, NSSet<NSString *> *> *extensionsForMIMETypeMap()
{
    static auto extensionsForMIMETypeMap = makeNeverDestroyed([] {
        auto extensionsForMIMETypeMap = adoptNS([[NSMutableDictionary alloc] init]);
        auto allUTIs = adoptCF(_UTCopyDeclaredTypeIdentifiers());

        auto addExtensionForMIMEType = ^(NSString *mimeType, NSString *extension) {
            if (!extensionsForMIMETypeMap.get()[mimeType])
                extensionsForMIMETypeMap.get()[mimeType] = [NSMutableSet set];
            [extensionsForMIMETypeMap.get()[mimeType] addObject:extension];
        };

        auto addExtensionsForMIMEType = ^(NSString *mimeType, NSArray<NSString *> *extensions) {
            auto wildcardMIMEType = [[mimeType componentsSeparatedByString:@"/"][0] stringByAppendingString:@"/*"];

            for (NSString *extension in extensions) {
                if (!extension)
                    continue;
                // Add extension to wildcardMIMEType, for example add "png" to "image/*"
                addExtensionForMIMEType(wildcardMIMEType, extension);
                // Add extension to itsmimeType, for example add "png" to "image/png"
                addExtensionForMIMEType(mimeType, extension);
            }
        };

        for (CFIndex i = 0, count = CFArrayGetCount(allUTIs.get()); i < count; ++i) {
            auto uti = static_cast<CFStringRef>(CFArrayGetValueAtIndex(allUTIs.get(), i));
            auto mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
            if (!mimeType)
                continue;
            auto extensions = adoptCF(UTTypeCopyAllTagsWithClass(uti, kUTTagClassFilenameExtension));
            addExtensionsForMIMEType((__bridge NSString *)mimeType.get(), (__bridge NSArray<NSString *> *)extensions.get());
        }

        // Add additional mime types which _UTCopyDeclaredTypeIdentifiers() may not return.
        addExtensionForMIMEType(@"image/webp", @"webp");

        return extensionsForMIMETypeMap;
    }());

    return extensionsForMIMETypeMap.get().get();
}

static NSSet<NSString *> *extensionsForMIMEType(NSString *mimetype)
{
    return [extensionsForMIMETypeMap() objectForKey:mimetype];
}

@implementation WKOpenPanelParameters

- (BOOL)allowsMultipleSelection
{
    return _openPanelParameters->allowMultipleFiles();
}

- (BOOL)allowsDirectories
{
    return _openPanelParameters->allowDirectories();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_openPanelParameters;
}

@end

@implementation WKOpenPanelParameters (WKPrivate)

- (NSArray<NSString *> *)_acceptedMIMETypes
{
    return wrapper(_openPanelParameters->acceptMIMETypes());
}

- (NSArray<NSString *> *)_acceptedFileExtensions
{
    return wrapper(_openPanelParameters->acceptFileExtensions());
}

- (NSArray<NSString *> *)_allowedFileExtensions
{
    // Aggregate extensions based on specified MIME types.
    auto acceptedMIMETypes = [self _acceptedMIMETypes];
    auto acceptedFileExtensions = [self _acceptedFileExtensions];

    auto allowedFileExtensions = adoptNS([[NSMutableSet alloc] init]);

    [acceptedMIMETypes enumerateObjectsUsingBlock:^(NSString *mimeType, NSUInteger index, BOOL* stop) {
        ASSERT([mimeType containsString:@"/"]);
        [allowedFileExtensions unionSet:extensionsForMIMEType(mimeType)];
    }];

    auto additionalAllowedFileExtensions = adoptNS([[NSMutableArray alloc] init]);

    [acceptedFileExtensions enumerateObjectsUsingBlock:^(NSString *extension, NSUInteger index, BOOL *stop) {
        ASSERT([extension characterAtIndex:0] == '.');
        [additionalAllowedFileExtensions addObject:[extension substringFromIndex:1]];
    }];
    
    [allowedFileExtensions addObjectsFromArray:additionalAllowedFileExtensions.get()];
    return [allowedFileExtensions allObjects];
}

- (NSArray<NSString *> *)_allowedFileExtensionsTitles
{
    auto acceptedMIMETypes = [self _acceptedMIMETypes];
    auto acceptedFileExtensions = [self _acceptedFileExtensions];

    constexpr auto AllFilesTitle = @"All Files";
    constexpr auto CustomFilesTitle = @"Custom Files";
    
    if (![acceptedMIMETypes count] && ![acceptedFileExtensions count])
        return @[AllFilesTitle];

    if (!([acceptedMIMETypes count] == 1 && ![acceptedFileExtensions count]))
        return @[CustomFilesTitle, AllFilesTitle];

    auto mimeType = [acceptedMIMETypes firstObject];
    auto range = [mimeType rangeOfString:@"/"];
    
    if (!range.length)
        return @[CustomFilesTitle, AllFilesTitle];
    
    auto mimeTypePrefix = [mimeType substringToIndex:range.location];
    auto mimeTypeSuffix = [mimeType substringFromIndex:range.location + 1];
    
    if ([mimeTypeSuffix isEqualToString:@"*"])
        return @[[NSString stringWithFormat:@"%@%@ Files", [[mimeTypePrefix substringToIndex:1] uppercaseString], [mimeTypePrefix substringFromIndex:1]], AllFilesTitle];

    if ([mimeTypeSuffix length] <= 4)
        return @[[NSString stringWithFormat:@"%@ %@", [mimeTypeSuffix uppercaseString], mimeTypePrefix], AllFilesTitle];

    return @[[NSString stringWithFormat:@"%@ %@", mimeTypeSuffix, mimeTypePrefix], AllFilesTitle];
}

@end

#endif
