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

#import "UIKitSPI.h"
#import <Foundation/NSProgress.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIColor.h>
#import <UIKit/UIImage.h>
#import <WebCore/FileSystemIOS.h>
#import <WebCore/Pasteboard.h>
#import <wtf/BlockPtr.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIColor)
SOFT_LINK_CLASS(UIKit, UIImage)
SOFT_LINK_CLASS(UIKit, UIItemProvider)

// FIXME: Remove once +objectWithItemProviderData:typeIdentifier:error: is available in the public SDK.
@interface NSObject (Foundation_NSItemProvider_Staging)
+ (id <NSItemProviderReading>)objectWithItemProviderData:(NSData *)data typeIdentifier:(NSString *)typeIdentifier error:(NSError **)outError;
- (id <NSItemProviderReading>)initWithItemProviderData:(NSData *)data typeIdentifier:(NSString *)typeIdentifier error:(NSError **)outError;
@end

using namespace WebCore;

typedef void(^ItemProviderDataLoadCompletionHandler)(NSData *, NSError *);
typedef NSDictionary<NSString *, NSURL *> TypeToFileURLMap;

@interface WebItemProviderRegistrationInfo ()
{
    RetainPtr<id <UIItemProviderWriting>> _representingObject;
    RetainPtr<NSString> _typeIdentifier;
    RetainPtr<NSData> _data;
}
@end

@implementation WebItemProviderRegistrationInfo

- (instancetype)initWithRepresentingObject:(id <UIItemProviderWriting>)representingObject typeIdentifier:(NSString *)typeIdentifier data:(NSData *)data
{
    if (representingObject)
        ASSERT(!typeIdentifier && !data);
    else
        ASSERT(typeIdentifier && data);

    if (self = [super init]) {
        _representingObject = representingObject;
        _typeIdentifier = typeIdentifier;
        _data = data;
    }
    return self;
}

- (id <UIItemProviderWriting>)representingObject
{
    return _representingObject.get();
}

- (NSString *)typeIdentifier
{
    return _typeIdentifier.get();
}

- (NSData *)data
{
    return _data.get();
}

@end

@interface WebItemProviderRegistrationInfoList ()
{
    RetainPtr<NSMutableArray> _items;
}
@end

@implementation WebItemProviderRegistrationInfoList

- (instancetype)init
{
    if (self = [super init]) {
        _items = adoptNS([[NSMutableArray alloc] init]);
        _estimatedDisplayedSize = CGSizeZero;
    }

    return self;
}

- (void)addData:(NSData *)data forType:(NSString *)typeIdentifier
{
    [_items addObject:[[[WebItemProviderRegistrationInfo alloc] initWithRepresentingObject:nil typeIdentifier:typeIdentifier data:data] autorelease]];
}

- (void)addRepresentingObject:(id <UIItemProviderWriting>)object
{
    ASSERT([object conformsToProtocol:@protocol(UIItemProviderWriting)]);
    [_items addObject:[[[WebItemProviderRegistrationInfo alloc] initWithRepresentingObject:object typeIdentifier:nil data:nil] autorelease]];
}

- (NSUInteger)numberOfItems
{
    return [_items count];
}

- (WebItemProviderRegistrationInfo *)itemAtIndex:(NSUInteger)index
{
    if (index >= self.numberOfItems)
        return nil;

    return [_items objectAtIndex:index];
}

- (void)enumerateItems:(void (^)(WebItemProviderRegistrationInfo *, NSUInteger))block
{
    for (NSUInteger index = 0; index < self.numberOfItems; ++index)
        block([self itemAtIndex:index], index);
}

@end

@interface WebItemProviderPasteboard ()

@property (nonatomic) NSInteger numberOfItems;
@property (nonatomic) NSInteger changeCount;
@property (nonatomic) NSInteger pendingOperationCount;

@end

@implementation WebItemProviderPasteboard {
    // FIXME: These ivars should be refactored to be Vector<RetainPtr<Type>> instead of generic NSArrays.
    RetainPtr<NSArray> _itemProviders;
    RetainPtr<NSArray> _cachedTypeIdentifiers;
    RetainPtr<NSArray> _typeToFileURLMaps;
    RetainPtr<NSArray> _supportedTypeIdentifiers;
    RetainPtr<NSArray> _registrationInfoLists;
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
        _typeToFileURLMaps = adoptNS([[NSArray alloc] init]);
        _supportedTypeIdentifiers = nil;
        _registrationInfoLists = nil;
    }
    return self;
}

- (void)updateSupportedTypeIdentifiers:(NSArray<NSString *> *)types
{
    _supportedTypeIdentifiers = types;
}

- (NSArray<NSString *> *)pasteboardTypesByFidelityForItemAtIndex:(NSUInteger)index
{
    return [self itemProviderAtIndex:index].registeredTypeIdentifiers ?: @[ ];
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

- (NSArray<__kindof NSItemProvider *> *)itemProviders
{
    return _itemProviders.get();
}

- (void)setItemProviders:(NSArray<__kindof NSItemProvider *> *)itemProviders
{
    itemProviders = itemProviders ?: [NSArray array];
    if (_itemProviders == itemProviders || [_itemProviders isEqualToArray:itemProviders])
        return;

    _itemProviders = itemProviders;
    _changeCount++;
    _cachedTypeIdentifiers = nil;
    _registrationInfoLists = nil;

    NSMutableArray *typeToFileURLMaps = [NSMutableArray arrayWithCapacity:itemProviders.count];
    [itemProviders enumerateObjectsUsingBlock:[typeToFileURLMaps] (UIItemProvider *, NSUInteger, BOOL *) {
        [typeToFileURLMaps addObject:@{ }];
    }];
}

- (NSInteger)numberOfItems
{
    return [_itemProviders count];
}

- (void)setItemsUsingRegistrationInfoLists:(NSArray<WebItemProviderRegistrationInfoList *> *)itemLists
{
    NSMutableArray *providers = [NSMutableArray array];
    for (WebItemProviderRegistrationInfoList *itemList in itemLists) {
        if (!itemList.numberOfItems)
            continue;

        auto itemProvider = adoptNS([allocUIItemProviderInstance() init]);
        [itemList enumerateItems:[itemProvider] (WebItemProviderRegistrationInfo *item, NSUInteger) {
            if (item.representingObject) {
                [itemProvider registerObject:item.representingObject visibility:UIItemProviderRepresentationOptionsVisibilityAll];
                return;
            }

            if (!item.typeIdentifier.length || !item.data.length)
                return;

            RetainPtr<NSData> itemData = item.data;
            [itemProvider registerDataRepresentationForTypeIdentifier:item.typeIdentifier visibility:UIItemProviderRepresentationOptionsVisibilityAll loadHandler:[itemData] (ItemProviderDataLoadCompletionHandler completionHandler) -> NSProgress * {
                completionHandler(itemData.get(), nil);
                return nil;
            }];
        }];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [itemProvider setEstimatedDisplayedSize:itemList.estimatedDisplayedSize];
#pragma clang diagnostic pop
        [itemProvider setSuggestedName:itemList.suggestedName];
        [providers addObject:itemProvider.get()];
    }

    self.itemProviders = providers;
    _registrationInfoLists = itemLists;
}

- (NSData *)_preLoadedDataConformingToType:(NSString *)typeIdentifier forItemProviderAtIndex:(NSUInteger)index
{
    if ([_typeToFileURLMaps count] != [_itemProviders count]) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    TypeToFileURLMap *typeToFileURLMap = [_typeToFileURLMaps objectAtIndex:index];
    for (NSString *loadedType in typeToFileURLMap) {
        if (!UTTypeConformsTo((CFStringRef)loadedType, (CFStringRef)typeIdentifier))
            continue;

        // We've already loaded data relevant for this UTI type onto disk, so there's no need to ask the UIItemProvider for the same data again.
        if (NSData *result = [NSData dataWithContentsOfURL:typeToFileURLMap[loadedType] options:NSDataReadingMappedIfSafe error:nil])
            return result;
    }
    return nil;
}

- (NSArray *)dataForPasteboardType:(NSString *)pasteboardType inItemSet:(NSIndexSet *)itemSet
{
    auto values = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<WebItemProviderPasteboard> retainedSelf = self;
    [itemSet enumerateIndexesUsingBlock:[retainedSelf, pasteboardType, values] (NSUInteger index, BOOL *) {
        UIItemProvider *provider = [retainedSelf itemProviderAtIndex:index];
        if (!provider)
            return;

        if (NSData *loadedData = [retainedSelf _preLoadedDataConformingToType:pasteboardType forItemProviderAtIndex:index])
            [values addObject:loadedData];
    }];
    return values.autorelease();
}

static NSArray<Class<UIItemProviderReading>> *allLoadableClasses()
{
    return @[ [getUIColorClass() class], [getUIImageClass() class], [NSURL class], [NSString class], [NSAttributedString class] ];
}

static Class classForTypeIdentifier(NSString *typeIdentifier, NSString *&outTypeIdentifierToLoad)
{
    outTypeIdentifierToLoad = typeIdentifier;

    // First, try to load a platform UIItemProviderReading-conformant object as-is.
    for (Class<UIItemProviderReading> loadableClass in allLoadableClasses()) {
        if ([[loadableClass readableTypeIdentifiersForItemProvider] containsObject:(NSString *)typeIdentifier])
            return loadableClass;
    }

    // If we were unable to load any object, check if the given type identifier is still something
    // WebKit knows how to handle.
    if ([typeIdentifier isEqualToString:(NSString *)kUTTypeHTML]) {
        // Load kUTTypeHTML as a plain text HTML string.
        outTypeIdentifierToLoad = (NSString *)kUTTypePlainText;
        return [NSString class];
    }

    return nil;
}

- (NSArray *)valuesForPasteboardType:(NSString *)pasteboardType inItemSet:(NSIndexSet *)itemSet
{
    auto values = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<WebItemProviderPasteboard> retainedSelf = self;
    [itemSet enumerateIndexesUsingBlock:[retainedSelf, pasteboardType, values] (NSUInteger index, BOOL *) {
        UIItemProvider *provider = [retainedSelf itemProviderAtIndex:index];
        if (!provider)
            return;

        NSString *typeIdentifierToLoad;
        Class readableClass = classForTypeIdentifier(pasteboardType, typeIdentifierToLoad);
        if (!readableClass)
            return;

        NSData *preloadedData = [retainedSelf _preLoadedDataConformingToType:pasteboardType forItemProviderAtIndex:index];
        if (!preloadedData)
            return;

        if ([readableClass respondsToSelector:@selector(objectWithItemProviderData:typeIdentifier:error:)]) {
            if (id <NSItemProviderReading> readObject = [readableClass objectWithItemProviderData:preloadedData typeIdentifier:(NSString *)typeIdentifierToLoad error:nil])
                [values addObject:readObject];
        } else {
            if (auto readObject = adoptNS([[readableClass alloc] initWithItemProviderData:preloadedData typeIdentifier:(NSString *)typeIdentifierToLoad error:nil]))
                [values addObject:readObject.get()];
        }
    }];

    return values.autorelease();
}

- (NSInteger)changeCount
{
    return _changeCount;
}

- (NSArray<NSURL *> *)fileURLsForDataInteraction
{
    NSMutableArray<NSURL *> *fileURLs = [NSMutableArray array];
    for (TypeToFileURLMap *typeToFileURLMap in _typeToFileURLMaps.get())
        [fileURLs addObjectsFromArray:[typeToFileURLMap allValues]];
    return fileURLs;
}

static BOOL typeConformsToTypes(NSString *type, NSArray *conformsToTypes)
{
    // A type is considered appropriate to load if it conforms to one or more supported types.
    for (NSString *conformsToType in conformsToTypes) {
        if (UTTypeConformsTo((CFStringRef)type, (CFStringRef)conformsToType))
            return YES;
    }
    return NO;
}

- (NSInteger)numberOfFiles
{
    NSArray *supportedFileTypes = Pasteboard::supportedFileUploadPasteboardTypes();
    NSInteger numberOfFiles = 0;
    for (UIItemProvider *itemProvider in _itemProviders.get()) {
        for (NSString *identifier in itemProvider.registeredTypeIdentifiers) {
            if (!typeConformsToTypes(identifier, supportedFileTypes))
                continue;
            ++numberOfFiles;
            break;
        }
    }
    return numberOfFiles;
}

static NSURL *temporaryFileURLForDataInteractionContent(NSURL *url, NSString *suggestedName)
{
    static NSString *defaultDataInteractionFileName = @"file";
    static NSString *dataInteractionDirectoryPrefix = @"data-interaction";
    if (!url)
        return nil;

    NSString *temporaryDataInteractionDirectory = WebCore::createTemporaryDirectory(dataInteractionDirectoryPrefix);
    if (!temporaryDataInteractionDirectory)
        return nil;

    suggestedName = suggestedName ?: defaultDataInteractionFileName;
    if (![suggestedName containsString:@"."])
        suggestedName = [suggestedName stringByAppendingPathExtension:url.pathExtension];

    return [NSURL fileURLWithPath:[temporaryDataInteractionDirectory stringByAppendingPathComponent:suggestedName ?: url.lastPathComponent]];
}

- (NSString *)typeIdentifierToLoadForRegisteredTypeIdentfiers:(NSArray<NSString *> *)registeredTypeIdentifiers
{
    NSString *highestFidelityContentType = nil;
    for (NSString *registeredTypeIdentifier in registeredTypeIdentifiers) {
        if (typeConformsToTypes(registeredTypeIdentifier, _supportedTypeIdentifiers.get()))
            return registeredTypeIdentifier;

        if (!highestFidelityContentType && UTTypeConformsTo((CFStringRef)registeredTypeIdentifier, kUTTypeContent))
            highestFidelityContentType = registeredTypeIdentifier;
    }
    return highestFidelityContentType;
}

- (void)doAfterLoadingProvidedContentIntoFileURLs:(WebItemProviderFileLoadBlock)action
{
    [self doAfterLoadingProvidedContentIntoFileURLs:action synchronousTimeout:0];
}

- (void)doAfterLoadingProvidedContentIntoFileURLs:(WebItemProviderFileLoadBlock)action synchronousTimeout:(NSTimeInterval)synchronousTimeout
{
    auto changeCountBeforeLoading = _changeCount;
    auto typeToFileURLMaps = adoptNS([[NSMutableArray alloc] initWithCapacity:[_itemProviders count]]);

    // First, figure out which item providers we want to try and load files from.
    auto itemProvidersToLoad = adoptNS([[NSMutableArray alloc] init]);
    auto typeIdentifiersToLoad = adoptNS([[NSMutableArray alloc] init]);
    auto indicesOfitemProvidersToLoad = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<WebItemProviderPasteboard> protectedSelf = self;
    [_itemProviders enumerateObjectsUsingBlock:[protectedSelf, itemProvidersToLoad, typeIdentifiersToLoad, indicesOfitemProvidersToLoad, typeToFileURLMaps] (UIItemProvider *itemProvider, NSUInteger index, BOOL *) {
        NSString *typeIdentifierToLoad = [protectedSelf typeIdentifierToLoadForRegisteredTypeIdentfiers:itemProvider.registeredTypeIdentifiers];
        if (typeIdentifierToLoad) {
            [itemProvidersToLoad addObject:itemProvider];
            [typeIdentifiersToLoad addObject:typeIdentifierToLoad];
            [indicesOfitemProvidersToLoad addObject:@(index)];
        }
        [typeToFileURLMaps addObject:@{ }];
    }];

    if (![itemProvidersToLoad count]) {
        action(@[ ]);
        return;
    }

    auto setFileURLsLock = adoptNS([[NSLock alloc] init]);
    auto synchronousFileLoadingGroup = adoptOSObject(dispatch_group_create());
    auto fileLoadingGroup = adoptOSObject(dispatch_group_create());
    for (NSUInteger index = 0; index < [itemProvidersToLoad count]; ++index) {
        RetainPtr<UIItemProvider> itemProvider = [itemProvidersToLoad objectAtIndex:index];
        RetainPtr<NSString> typeIdentifier = [typeIdentifiersToLoad objectAtIndex:index];
        NSUInteger indexInItemProviderArray = [[indicesOfitemProvidersToLoad objectAtIndex:index] unsignedIntegerValue];
        RetainPtr<NSString> suggestedName = [itemProvider suggestedName];
        dispatch_group_enter(fileLoadingGroup.get());
        dispatch_group_enter(synchronousFileLoadingGroup.get());
        [itemProvider loadFileRepresentationForTypeIdentifier:typeIdentifier.get() completionHandler:[synchronousFileLoadingGroup, setFileURLsLock, indexInItemProviderArray, suggestedName, typeIdentifier, typeToFileURLMaps, fileLoadingGroup] (NSURL *url, NSError *error) {
            // After executing this completion block, UIKit removes the file at the given URL. However, we need this data to persist longer for the web content process.
            // To address this, we hard link the given URL to a new temporary file in the temporary directory. This follows the same flow as regular file upload, in
            // WKFileUploadPanel.mm. The temporary files are cleaned up by the system at a later time.
            RetainPtr<NSURL> destinationURL = temporaryFileURLForDataInteractionContent(url, suggestedName.get());
            if (destinationURL && !error && [[NSFileManager defaultManager] linkItemAtURL:url toURL:destinationURL.get() error:nil]) {
                [setFileURLsLock lock];
                [typeToFileURLMaps setObject:[NSDictionary dictionaryWithObject:destinationURL.get() forKey:typeIdentifier.get()] atIndexedSubscript:indexInItemProviderArray];
                [setFileURLsLock unlock];
            }
            dispatch_group_leave(fileLoadingGroup.get());
            dispatch_group_leave(synchronousFileLoadingGroup.get());
        }];
    }

    RetainPtr<WebItemProviderPasteboard> retainedSelf = self;
    auto itemLoadCompletion = [retainedSelf, synchronousFileLoadingGroup, fileLoadingGroup, typeToFileURLMaps, completionBlock = makeBlockPtr(action), changeCountBeforeLoading] {
        if (changeCountBeforeLoading == retainedSelf->_changeCount)
            retainedSelf->_typeToFileURLMaps = typeToFileURLMaps;

        completionBlock([retainedSelf fileURLsForDataInteraction]);
    };

    if (synchronousTimeout > 0 && !dispatch_group_wait(synchronousFileLoadingGroup.get(), dispatch_time(DISPATCH_TIME_NOW, synchronousTimeout * NSEC_PER_SEC))) {
        itemLoadCompletion();
        return;
    }

    dispatch_group_notify(fileLoadingGroup.get(), dispatch_get_main_queue(), itemLoadCompletion);
}

- (WebItemProviderRegistrationInfoList *)registrationInfoAtIndex:(NSUInteger)index
{
    return index < [_registrationInfoLists count] ? [_registrationInfoLists objectAtIndex:index] : nil;
}

- (UIItemProvider *)itemProviderAtIndex:(NSUInteger)index
{
    return index < [_itemProviders count] ? [_itemProviders objectAtIndex:index] : nil;
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
