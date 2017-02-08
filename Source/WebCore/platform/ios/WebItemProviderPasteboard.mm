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
    NSMutableSet<NSString *> *allTypes = [NSMutableSet set];
    for (UIItemProvider *provider in _itemProviders.get())
        [allTypes addObjectsFromArray:provider.registeredTypeIdentifiers];
    return allTypes.allObjects;
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
}

- (NSInteger)numberOfItems
{
    return [_itemProviders count];
}

- (void)setItems:(NSArray *)items
{
    NSMutableArray *providers = [NSMutableArray array];
    for (NSDictionary *item in items) {
        if (!item.count)
            continue;
        RetainPtr<UIItemProvider> itemProvider = adoptNS([[getUIItemProviderClass() alloc] init]);
        RetainPtr<NSMutableDictionary> itemRepresentationsCopy = adoptNS([item mutableCopy]);
        // First, let the platform write all the default object types it can recognize, such as NSString and NSURL.
        for (NSString *typeIdentifier in [itemRepresentationsCopy allKeys]) {
            id representingObject = [itemRepresentationsCopy objectForKey:typeIdentifier];
            if (![representingObject conformsToProtocol:@protocol(UIItemProviderWriting)])
                continue;

            id <UIItemProviderWriting> objectToWrite = (id <UIItemProviderWriting>)representingObject;
            if (![objectToWrite.writableTypeIdentifiersForItemProvider containsObject:typeIdentifier])
                continue;

            [itemRepresentationsCopy removeObjectForKey:typeIdentifier];
            [objectToWrite registerLoadHandlersToItemProvider:itemProvider.get()];
        }

        // Secondly, WebKit uses some custom type representations and/or type identifiers, so we need to write these as well.
        for (NSString *typeIdentifier in itemRepresentationsCopy.get()) {
            [itemProvider registerDataRepresentationForTypeIdentifier:typeIdentifier options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
            {
                completionBlock([itemRepresentationsCopy objectForKey:typeIdentifier], nil);
                return [NSProgress discreteProgressWithTotalUnitCount:100];
            }];
        }
        [providers addObject:itemProvider.get()];
    }
    _changeCount++;
    _itemProviders = providers;
}

- (NSArray *)dataForPasteboardType:(NSString *)pasteboardType inItemSet:(NSIndexSet *)itemSet
{
    __block NSMutableArray *values = [NSMutableArray array];
    [itemSet enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *) {
        UIItemProvider *provider = [self itemProviderAtIndex:index];
        if (!provider)
            return;

        NSData *data = [provider copyDataRepresentationForTypeIdentifier:pasteboardType error:nil];
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
    if (![provider canCreateObjectOfClass:objectClass])
        return NO;

    id object = [provider createObjectOfClass:objectClass error:nil];
    if (object)
        [array addObject:object];

    return !!object;
}

- (NSInteger)changeCount
{
    return _changeCount;
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

@end

#endif // ENABLE(DATA_INTERACTION)
