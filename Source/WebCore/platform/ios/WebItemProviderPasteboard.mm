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

#include "config.h"
#import "WebItemProviderPasteboard.h"

#if ENABLE(DATA_INTERACTION)

#import "SoftLinking.h"
#import "UIKitSPI.h"
#import <Foundation/NSProgress.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIColor.h>
#import <UIKit/UIImage.h>
#import <UIKit/UIItemProviderWriting.h>
#import <WebCore/FileSystemIOS.h>
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIColor)
SOFT_LINK_CLASS(UIKit, UIImage)
SOFT_LINK_CLASS(UIKit, UIItemProvider)

#define MATCHES_UTI_TYPE(type, suffix) [type isEqualToString:(__bridge NSString *)kUTType ## suffix]
#define MATCHES_UIKIT_TYPE(type, suffix) [type isEqualToString:@"com.apple.uikit. ## suffix ##"]

static BOOL isRichTextType(NSString *type)
{
    return MATCHES_UTI_TYPE(type, RTF) || MATCHES_UTI_TYPE(type, RTFD) || MATCHES_UTI_TYPE(type, HTML);
}

static BOOL isStringType(NSString *type)
{
    return MATCHES_UTI_TYPE(type, Text) || MATCHES_UTI_TYPE(type, UTF8PlainText) || MATCHES_UTI_TYPE(type, UTF16PlainText);
}

static BOOL isURLType(NSString *type)
{
    return MATCHES_UTI_TYPE(type, URL);
}

static BOOL isColorType(NSString *type)
{
    return MATCHES_UIKIT_TYPE(type, color);
}

static BOOL isImageType(NSString *type)
{
    return MATCHES_UTI_TYPE(type, PNG) || MATCHES_UTI_TYPE(type, JPEG) || MATCHES_UTI_TYPE(type, GIF) || MATCHES_UIKIT_TYPE(type, image);
}

@interface WebItemProviderPasteboard ()

@property (nonatomic) NSInteger numberOfItems;
@property (nonatomic) NSInteger changeCount;
@property (nonatomic) NSInteger pendingOperationCount;

@end

@implementation WebItemProviderPasteboard {
    RetainPtr<NSArray> _itemProviders;
    RetainPtr<NSArray> _cachedTypeIdentifiers;
}

+ (instancetype)sharedInstance
{
    static WebItemProviderPasteboard *sharedPasteboard = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^() {
        sharedPasteboard = [[WebItemProviderPasteboard alloc] init];
    });
    return sharedPasteboard;
}

- (instancetype)init
{
    if (self = [super init]) {
        _itemProviders = adoptNS([[NSArray alloc] init]);
        _changeCount = 0;
        _pendingOperationCount = 0;
    }
    return self;
}

- (NSArray<NSString *> *)pasteboardTypes
{
    if (_cachedTypeIdentifiers)
        return _cachedTypeIdentifiers.get();

    NSMutableSet<NSString *> *allTypes = [NSMutableSet set];
    NSMutableArray<NSString *> *allTypesInOrder = [NSMutableArray array];
    for (UIItemProvider *provider in _itemProviders.get()) {
        for (NSString *typeIdentifier in provider.registeredTypeIdentifiers) {
            if ([allTypes containsObject:typeIdentifier])
                continue;

            [allTypes addObject:typeIdentifier];
            [allTypesInOrder addObject:typeIdentifier];
        }
    }
    _cachedTypeIdentifiers = allTypesInOrder;
    return _cachedTypeIdentifiers.get();
}

- (NSArray<UIItemProvider *> *)itemProviders
{
    return _itemProviders.get();
}

- (void)setItemProviders:(NSArray<UIItemProvider *> *)itemProviders
{
    itemProviders = itemProviders ?: [NSArray array];
    if (_itemProviders == itemProviders || [_itemProviders isEqualToArray:itemProviders])
        return;

    _itemProviders = itemProviders;
    _changeCount++;
    _cachedTypeIdentifiers = nil;
}

- (NSInteger)numberOfItems
{
    return [_itemProviders count];
}

- (void)setItemsFromObjectRepresentations:(NSArray<WebPasteboardItemData *> *)itemData
{
    NSMutableArray *providers = [NSMutableArray array];
    for (WebPasteboardItemData *data in itemData) {
        if (!data.representingObjects.count && !data.additionalData.count)
            continue;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        RetainPtr<UIItemProvider> itemProvider = adoptNS([[getUIItemProviderClass() alloc] init]);
        // First, register all platform objects, prioritizing objects at the beginning of the array.
        for (id representingObject in data.representingObjects) {
            if (![representingObject conformsToProtocol:@protocol(UIItemProviderWriting)])
                continue;

            [itemProvider registerObject:(id <UIItemProviderWriting>)representingObject options:nil];
        }

        // Next, register other custom data representations for type identifiers.
        NSDictionary <NSString *, NSData *> *additionalData = data.additionalData;
        for (NSString *typeIdentifier in additionalData) {
            if (![additionalData[typeIdentifier] isKindOfClass:[NSData class]])
                continue;

            [itemProvider registerDataRepresentationForTypeIdentifier:typeIdentifier options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
            {
                completionBlock(additionalData[typeIdentifier], nil);
                return [NSProgress discreteProgressWithTotalUnitCount:100];
            }];
        }
        [providers addObject:itemProvider.get()];
#pragma clang diagnostic pop
    }

    self.itemProviders = providers;
}

- (NSArray *)dataForPasteboardType:(NSString *)pasteboardType inItemSet:(NSIndexSet *)itemSet
{
    __block NSMutableArray *values = [NSMutableArray array];
    [itemSet enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *) {
        UIItemProvider *provider = [self itemProviderAtIndex:index];
        if (!provider)
            return;

        // FIXME: <rdar://problem/30451096> Adopt asynchronous UIItemProvider methods when retrieving data.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSData *data = [provider copyDataRepresentationForTypeIdentifier:pasteboardType error:nil];
#pragma clang diagnostic pop
        if (data)
            [values addObject:data];
    }];

    return values;
}

- (NSArray *)valuesForPasteboardType:(NSString *)pasteboardType inItemSet:(NSIndexSet *)itemSet
{
    __block NSMutableArray *values = [NSMutableArray array];
    [itemSet enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *) {
        UIItemProvider *provider = [self itemProviderAtIndex:index];
        if (!provider)
            return;

        // FIXME: These should be refactored to use asynchronous calls.
        if (isColorType(pasteboardType) && [self _tryToCreateAndAppendObjectOfClass:[getUIColorClass() class] toArray:values usingProvider:provider])
            return;

        if (isImageType(pasteboardType) && [self _tryToCreateAndAppendObjectOfClass:[getUIImageClass() class] toArray:values usingProvider:provider])
            return;

        if (isURLType(pasteboardType) && [self _tryToCreateAndAppendObjectOfClass:[NSURL class] toArray:values usingProvider:provider])
            return;

        if (isRichTextType(pasteboardType) && [self _tryToCreateAndAppendObjectOfClass:[NSAttributedString class] toArray:values usingProvider:provider])
            return;

        if (isStringType(pasteboardType) && [self _tryToCreateAndAppendObjectOfClass:[NSString class] toArray:values usingProvider:provider])
            return;

        WTFLogAlways("Failed to instantiate object for type: '%s' at index: %tu", pasteboardType.UTF8String, index);
    }];
    return values;
}

- (BOOL)_tryToCreateAndAppendObjectOfClass:(Class)objectClass toArray:(NSMutableArray *)array usingProvider:(UIItemProvider *)provider
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (![provider canCreateObjectOfClass:objectClass])
        return NO;

    // FIXME: <rdar://problem/30451096> Adopt asynchronous UIItemProvider methods when retrieving data.
    id object = [provider createObjectOfClass:objectClass error:nil];
#pragma clang diagnostic pop
    if (object)
        [array addObject:object];

    return !!object;
}

- (NSInteger)changeCount
{
    return _changeCount;
}

- (NSInteger)numberOfFiles
{
    NSInteger numberOfFiles = 0;
    for (UIItemProvider *itemProvider in _itemProviders.get()) {
        for (NSString *identifier in itemProvider.registeredTypeIdentifiers) {
            if (!UTTypeConformsTo((__bridge CFStringRef)identifier, kUTTypeContent))
                continue;
            ++numberOfFiles;
            break;
        }
    }
    return numberOfFiles;
}

static NSURL *temporaryFileURLForDataInteractionContent(NSString *fileExtension, NSString *suggestedName)
{
    static NSString *defaultDataInteractionFileName = @"file";
    static NSString *dataInteractionDirectoryPrefix = @"data-interaction";
    if (!fileExtension.length)
        return nil;

    NSString *temporaryDataInteractionDirectory = WebCore::createTemporaryDirectory(dataInteractionDirectoryPrefix);
    if (!temporaryDataInteractionDirectory)
        return nil;

    NSString *filenameWithExtension = [suggestedName ?: defaultDataInteractionFileName stringByAppendingPathExtension:fileExtension];
    return [NSURL fileURLWithPath:[temporaryDataInteractionDirectory stringByAppendingPathComponent:filenameWithExtension]];
}

- (void)doAfterLoadingProvidedContentIntoFileURLs:(WebItemProviderFileLoadBlock)action
{
    auto itemProvidersWithFiles = adoptNS([[NSMutableArray alloc] init]);
    auto contentTypeIdentifiersToLoad = adoptNS([[NSMutableArray alloc] init]);
    auto loadedFileURLs = adoptNS([[NSMutableArray alloc] init]);
    for (UIItemProvider *itemProvider in _itemProviders.get()) {
        NSString *typeIdentifierOfContentToSave = nil;
        for (NSString *identifier in itemProvider.registeredTypeIdentifiers) {
            if (!UTTypeConformsTo((CFStringRef)identifier, kUTTypeContent))
                continue;

            typeIdentifierOfContentToSave = identifier;
            break;
        }

        if (typeIdentifierOfContentToSave) {
            [itemProvidersWithFiles addObject:itemProvider];
            [contentTypeIdentifiersToLoad addObject:typeIdentifierOfContentToSave];
        }
    }

    if (![itemProvidersWithFiles count]) {
        action(@[ ]);
        return;
    }

    auto fileLoadingGroup = adoptOSObject(dispatch_group_create());
    for (NSUInteger index = 0; index < [itemProvidersWithFiles count]; ++index) {
        RetainPtr<UIItemProvider> itemProvider = [itemProvidersWithFiles objectAtIndex:index];
        RetainPtr<NSString> typeIdentifier = [contentTypeIdentifiersToLoad objectAtIndex:index];
        RetainPtr<NSString> suggestedName = [itemProvider suggestedName];
        dispatch_group_enter(fileLoadingGroup.get());
        dispatch_group_async(fileLoadingGroup.get(), dispatch_get_main_queue(), [itemProvider, typeIdentifier, suggestedName, loadedFileURLs, fileLoadingGroup] {
            [itemProvider loadFileRepresentationForTypeIdentifier:typeIdentifier.get() completionHandler:[suggestedName, loadedFileURLs, fileLoadingGroup] (NSURL *url, NSError *error) {
                // After executing this completion block, UIKit removes the file at the given URL. However, we need this data to persist longer for the web content process.
                // To address this, we hard link the given URL to a new temporary file in the temporary directory. This follows the same flow as regular file upload, in
                // WKFileUploadPanel.mm. The temporary files are cleaned up by the system at a later time.
                RetainPtr<NSURL> destinationURL = temporaryFileURLForDataInteractionContent(url.pathExtension, suggestedName.get());
                if (destinationURL && !error && [[NSFileManager defaultManager] linkItemAtURL:url toURL:destinationURL.get() error:nil])
                    [loadedFileURLs addObject:destinationURL.get()];
                dispatch_group_leave(fileLoadingGroup.get());
            }];
        });
    }

    dispatch_group_notify(fileLoadingGroup.get(), dispatch_get_main_queue(), [fileLoadingGroup, loadedFileURLs, completionBlock = makeBlockPtr(action)] {
        completionBlock(loadedFileURLs.get());
    });
}

- (UIItemProvider *)itemProviderAtIndex:(NSInteger)index
{
    return 0 <= index && index < (NSInteger)[_itemProviders count] ? [_itemProviders objectAtIndex:index] : nil;
}

- (BOOL)hasPendingOperation
{
    return _pendingOperationCount;
}

- (void)incrementPendingOperationCount
{
    _pendingOperationCount++;
}

- (void)decrementPendingOperationCount
{
    _pendingOperationCount--;
}

- (void)enumerateItemProvidersWithBlock:(void (^)(UIItemProvider *itemProvider, NSUInteger index, BOOL *stop))block
{
    [_itemProviders enumerateObjectsUsingBlock:block];
}

@end

#endif // ENABLE(DATA_INTERACTION)
