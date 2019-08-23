/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)

#if USE(APPLE_INTERNAL_SDK)

#import <NearField/NearField.h>

#else

typedef NS_OPTIONS(NSUInteger, NFFeature) {
    NFFeatureReaderMode                 = 1 << 0,
};

typedef NS_ENUM(uint32_t, NFTagType) {
    NFTagTypeUnknown    = 0,
    NFTagTypeGeneric4A  = 3,
};

typedef NS_OPTIONS(uint32_t, NFTechnology) {
    NFTechnologyNone                 = 0,
};

typedef NS_ENUM(uint32_t, NFNdefAvailability) {
    NFNdefAvailabilityUnknown = 0,
};

@protocol NFTagA
@end

@protocol NFTagB
@end

@protocol NFTagF
@end

@protocol NFTag <NSObject>
@property (nonatomic, readonly) NFTagType type;
@property (nonatomic, readonly) NFTechnology technology;
@property (nonatomic, readonly, copy) NSData *tagID;
@property (nonatomic, readonly, copy) NSData *UID;
@property (nonatomic, readonly, assign) NFNdefAvailability ndefAvailability;
@property (nonatomic, readonly, assign) size_t ndefMessageSize;
@property (nonatomic, readonly, assign) size_t ndefContainerSize;
@property (nonatomic, readonly, copy) NSData *AppData NS_DEPRECATED(10_12, 10_15, 10_0, 13_0);

@property (nonatomic, readonly) id<NFTagA> tagA;
@property (nonatomic, readonly) id<NFTagB> tagB;
@property (nonatomic, readonly) id<NFTagF> tagF;

- (instancetype)initWithNFTag:(id<NFTag>)tag;
- (NSString *)description;
- (BOOL)isEqualToNFTag:(id<NFTag>)tag;
@end

@interface NFTag : NSObject <NSSecureCoding, NFTag, NFTagA, NFTagB, NFTagF>
@end

@protocol NFSession <NSObject>
- (void)endSession;
@end

@interface NFSession : NSObject <NFSession>
@end

@protocol NFReaderSessionDelegate;

@interface NFReaderSession : NFSession
@property (assign) id<NFReaderSessionDelegate> delegate;

- (BOOL)startPolling;
- (BOOL)stopPolling;
- (BOOL)connectTag:(NFTag*)tag;
- (BOOL)disconnectTag;
- (NSData*)transceive:(NSData*)capdu;
- (NSError *)updateUIAlertMessage:(NSString *)message;
@end

@protocol NFReaderSessionDelegate <NSObject>
@optional
- (void)readerSession:(NFReaderSession*)theSession didDetectTags:(NSArray<NFTag *> *)tags;
@end

@interface NFHardwareManager : NSObject
+ (instancetype)sharedHardwareManager;
- (NSObject<NFSession> *)startReaderSessionWithActionSheetUI:(void(^)(NFReaderSession *session, NSError *error))theStartCallback;
- (BOOL)areFeaturesSupported:(NFFeature)featureMask outError:(NSError**)outError;
@end

#endif // USE(APPLE_INTERNAL_SDK)

#endif // ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)
