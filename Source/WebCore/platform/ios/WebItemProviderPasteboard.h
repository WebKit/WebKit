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

#import <WebCore/AbstractPasteboard.h>

#if TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000

@class UIItemProvider;
@protocol UIItemProviderWriting;

struct CGSize;

typedef NS_ENUM(NSInteger, WebPreferredPresentationStyle) {
    WebPreferredPresentationStyleUnspecified,
    WebPreferredPresentationStyleInline,
    WebPreferredPresentationStyleAttachment
};

NS_ASSUME_NONNULL_BEGIN

/*! A WebItemProviderRegistrationInfo represents a single call to register something to an item provider.
 @discussion Either the representing object exists and the type identifier and data are nil, or the
 representing object is nil and the type identifier and data exist. The former represents a call to
 register an entire UIItemProviderWriting-conformant object to the item provider, while the latter
 represents a call to register only a data representation for the given type identifier.
 */
WEBCORE_EXPORT @interface WebItemProviderRegistrationInfo : NSObject

@property (nonatomic, readonly, nullable, strong) id <UIItemProviderWriting> representingObject;
@property (nonatomic, readonly, nullable, strong) NSString *typeIdentifier;
@property (nonatomic, readonly, nullable, strong) NSData *data;

@end

/*! A WebItemProviderRegistrationInfoList represents a series of registration calls used to set up a
 single item provider.
 @discussion The order of items specified in the list (lowest indices first) is the order in which
 objects or data are registered to the item provider, and therefore indicates the relative fidelity
 of each item. Private UTI types, such as those vended through the injected editing bundle SPI, are
 considered to be higher fidelity than the other default types.
 */
WEBCORE_EXPORT @interface WebItemProviderRegistrationInfoList : NSObject

- (void)addRepresentingObject:(id <UIItemProviderWriting>)object;
- (void)addData:(NSData *)data forType:(NSString *)typeIdentifier;

@property (nonatomic) CGSize preferredPresentationSize;
@property (nonatomic, copy) NSString *suggestedName;
@property (nonatomic, readonly, nullable) UIItemProvider *itemProvider;

@property (nonatomic) WebPreferredPresentationStyle preferredPresentationStyle;
@property (nonatomic, copy) NSData *teamData;

- (NSUInteger)numberOfItems;
- (nullable WebItemProviderRegistrationInfo *)itemAtIndex:(NSUInteger)index;
- (void)enumerateItems:(void(^)(WebItemProviderRegistrationInfo *item, NSUInteger index))block;

@end

typedef void (^WebItemProviderFileLoadBlock)(NSArray<NSURL *> *);

WEBCORE_EXPORT @interface WebItemProviderPasteboard : NSObject<AbstractPasteboard>

+ (instancetype)sharedInstance;

@property (copy, nonatomic, nullable) NSArray<__kindof NSItemProvider *> *itemProviders;
@property (readonly, nonatomic) NSInteger numberOfItems;
@property (readonly, nonatomic) NSInteger changeCount;

// This will only be non-empty when an operation is being performed.
@property (readonly, nonatomic) NSArray<NSURL *> *allDroppedFileURLs;

// The preferred file URL corresponds to the highest fidelity non-private UTI that was loaded.
- (nullable NSURL *)preferredFileUploadURLAtIndex:(NSUInteger)index fileType:(NSString *_Nullable *_Nullable)outFileType;

@property (readonly, nonatomic) BOOL hasPendingOperation;
- (void)incrementPendingOperationCount;
- (void)decrementPendingOperationCount;

- (void)enumerateItemProvidersWithBlock:(void (^)(UIItemProvider *itemProvider, NSUInteger index, BOOL *stop))block;

// The given completion block is always dispatched on the main thread.
- (void)doAfterLoadingProvidedContentIntoFileURLs:(WebItemProviderFileLoadBlock)action;
- (void)doAfterLoadingProvidedContentIntoFileURLs:(WebItemProviderFileLoadBlock)action synchronousTimeout:(NSTimeInterval)synchronousTimeout;

@end

NS_ASSUME_NONNULL_END

#endif // TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
