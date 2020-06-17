/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAL_PASSKIT_SPI_GUARD_AGAINST_INDIRECT_INCLUSION
#error "Please #include <pal/spi/cocoa/PassKitSPI.h> instead of this file directly."
#endif

#if HAVE(PASSKIT_INSTALLMENTS_IN_SDK)

#import <PassKit/PKPaymentRequest_Private.h>
#import <PassKitCore/PKPaymentMethod_Private.h>
#import <PassKitCore/PKPaymentRequestStatus_Private.h>
#import <PassKitCore/PKPayment_Private.h>

#elif HAVE(PASSKIT_INSTALLMENTS)

typedef NS_ENUM(NSInteger, PKInstallmentItemType) {
    PKInstallmentItemTypeGeneric = 0,
    PKInstallmentItemTypePhone,
    PKInstallmentItemTypePad,
    PKInstallmentItemTypeWatch,
    PKInstallmentItemTypeMac,
};

typedef NS_ENUM(NSInteger, PKInstallmentRetailChannel) {
    PKInstallmentRetailChannelUnknown = 0,
    PKInstallmentRetailChannelApp,
    PKInstallmentRetailChannelWeb,
    PKInstallmentRetailChannelInStore,
};

typedef NS_ENUM(NSUInteger, PKPaymentRequestType) {
    PKPaymentRequestTypeInstallment = 5,
};

@interface PKPaymentInstallmentConfiguration : NSObject <NSSecureCoding>
@end

@interface PKPaymentInstallmentItem : NSObject <NSSecureCoding>
@end

@interface PKPayment () <NSSecureCoding>
@property (nonatomic, copy) NSString *installmentAuthorizationToken;
@end

@interface PKPaymentInstallmentConfiguration ()
@property (nonatomic, assign) PKPaymentSetupFeatureType feature;
@property (nonatomic, copy) NSData *merchandisingImageData;
@property (nonatomic, strong) NSDecimalNumber *openToBuyThresholdAmount;
@property (nonatomic, strong) NSDecimalNumber *bindingTotalAmount;
@property (nonatomic, copy) NSString *currencyCode;
@property (nonatomic, assign, getter=isInStorePurchase) BOOL inStorePurchase;
@property (nonatomic, copy) NSString *installmentMerchantIdentifier;
@property (nonatomic, copy) NSString *referrerIdentifier;
@property (nonatomic, copy) NSArray<PKPaymentInstallmentItem *> *installmentItems;
@property (nonatomic, copy) NSDictionary<NSString *, id> *applicationMetadata;
@property (nonatomic, assign) PKInstallmentRetailChannel retailChannel;
@end

@interface PKPaymentInstallmentItem ()
@property (nonatomic, assign) PKInstallmentItemType installmentItemType;
@property (nonatomic, copy) NSDecimalNumber *amount;
@property (nonatomic, copy) NSString *currencyCode;
@property (nonatomic, copy) NSString *programIdentifier;
@property (nonatomic, copy) NSDecimalNumber *apr;
@property (nonatomic, copy) NSString *programTerms;
@end

@interface PKPaymentMethod () <NSSecureCoding>
@property (nonatomic, copy) NSString *bindToken;
@end

@interface PKPaymentRequest ()
@property (nonatomic, assign) PKPaymentRequestAPIType APIType;
@property (nonatomic, strong) PKPaymentInstallmentConfiguration *installmentConfiguration;
@property (nonatomic, assign) PKPaymentRequestType requestType;
@end

@interface PKPaymentRequestPaymentMethodUpdate ()
@property (nonatomic, copy) NSString *installmentGroupIdentifier;
@end

#endif // HAVE(PASSKIT_INSTALLMENTS)
