/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)

#import <PassKit/PassKit.h>
#import <PassKit/PKPaymentAuthorizationViewController_Private.h>
#import <PassKit/PKPaymentRequest_Private.h>

#else

#if PLATFORM(IOS_FAMILY)

#import <PassKit/PassKit.h>

#elif PLATFORM(MAC)

#import <Contacts/Contacts.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300

@class PKPaymentAuthorizationResult;
@class PKPaymentRequestUpdate;
@class PKPaymentRequestPaymentMethodUpdate;
@class PKPaymentRequestShippingMethodUpdate;
@class PKPaymentRequestShippingContactUpdate;

typedef NSString *PKContactField;

extern NSString * const PKPaymentErrorDomain;
typedef NS_ERROR_ENUM(PKPaymentErrorDomain, PKPaymentErrorCode) {
    PKPaymentUnknownError = -1,
    PKPaymentShippingContactInvalidError = 1,
    PKPaymentBillingContactInvalidError,
    PKPaymentShippingAddressUnserviceableError,
};

#endif

typedef NS_OPTIONS(NSUInteger, PKAddressField) {
    PKAddressFieldNone = 0UL,
    PKAddressFieldPostalAddress = 1UL << 0,
    PKAddressFieldPhone = 1UL << 1,
    PKAddressFieldEmail = 1UL << 2,
    PKAddressFieldName = 1UL << 3,
    PKAddressFieldAll = (PKAddressFieldPostalAddress | PKAddressFieldPhone | PKAddressFieldEmail | PKAddressFieldName)
};

typedef NS_OPTIONS(NSUInteger, PKMerchantCapability) {
    PKMerchantCapability3DS = 1UL << 0,
    PKMerchantCapabilityEMV = 1UL << 1,
    PKMerchantCapabilityCredit = 1UL << 2,
    PKMerchantCapabilityDebit = 1UL << 3
};

typedef NS_ENUM(NSInteger, PKPaymentAuthorizationStatus) {
    PKPaymentAuthorizationStatusSuccess,
    PKPaymentAuthorizationStatusFailure,
    PKPaymentAuthorizationStatusInvalidBillingPostalAddress,
    PKPaymentAuthorizationStatusInvalidShippingPostalAddress,
    PKPaymentAuthorizationStatusInvalidShippingContact,
    PKPaymentAuthorizationStatusPINRequired,
    PKPaymentAuthorizationStatusPINIncorrect,
    PKPaymentAuthorizationStatusPINLockout,
};

typedef NS_ENUM(NSUInteger, PKPaymentMethodType) {
    PKPaymentMethodTypeUnknown = 0,
    PKPaymentMethodTypeDebit,
    PKPaymentMethodTypeCredit,
    PKPaymentMethodTypePrepaid,
    PKPaymentMethodTypeStore
};

typedef NS_ENUM(NSUInteger, PKPaymentPassActivationState) {
    PKPaymentPassActivationStateActivated,
    PKPaymentPassActivationStateRequiresActivation,
    PKPaymentPassActivationStateActivating,
    PKPaymentPassActivationStateSuspended,
    PKPaymentPassActivationStateDeactivated
};

typedef NS_ENUM(NSUInteger, PKPaymentSummaryItemType) {
    PKPaymentSummaryItemTypeFinal,
    PKPaymentSummaryItemTypePending
};

typedef NS_ENUM(NSUInteger, PKShippingType) {
    PKShippingTypeShipping,
    PKShippingTypeDelivery,
    PKShippingTypeStorePickup,
    PKShippingTypeServicePickup
};

typedef NSString * PKPaymentNetwork NS_EXTENSIBLE_STRING_ENUM;

@protocol PKPaymentAuthorizationViewControllerDelegate;

@interface PKObject : NSObject
@end

@interface PKPass : PKObject
@end

@interface PKPaymentPass : PKPass
@property (nonatomic, copy, readonly) NSString *primaryAccountIdentifier;
@property (nonatomic, copy, readonly) NSString *primaryAccountNumberSuffix;
@property (weak, readonly) NSString *deviceAccountIdentifier;
@property (weak, readonly) NSString *deviceAccountNumberSuffix;
@property (nonatomic, readonly) PKPaymentPassActivationState activationState;
@end

@interface PKPaymentMethod : NSObject
@property (nonatomic, copy, readonly, nullable) NSString *displayName;
@property (nonatomic, copy, readonly, nullable) PKPaymentNetwork network;
@property (nonatomic, readonly) PKPaymentMethodType type;
@property (nonatomic, copy, readonly, nullable) PKPaymentPass *paymentPass;
@end

@interface PKPaymentToken : NSObject
@property (nonatomic, strong, readonly) PKPaymentMethod *paymentMethod;
@property (nonatomic, copy, readonly) NSString *transactionIdentifier;
@property (nonatomic, copy, readonly) NSData *paymentData;
@end

@interface PKContact : NSObject
@property (nonatomic, strong, nullable) NSPersonNameComponents *name;
@property (nonatomic, strong, nullable) CNPostalAddress *postalAddress;
@property (nonatomic, strong, nullable) NSString *emailAddress;
@property (nonatomic, strong, nullable) CNPhoneNumber *phoneNumber;
@property (nonatomic, retain, nullable) NSString *supplementarySubLocality;
@end

@interface PKPayment : NSObject
@property (nonatomic, strong, readonly, nonnull) PKPaymentToken *token;
@property (nonatomic, strong, readonly, nullable) PKContact *billingContact;
@property (nonatomic, strong, readonly, nullable) PKContact *shippingContact;
@end

@interface PKPaymentSummaryItem : NSObject
+ (instancetype)summaryItemWithLabel:(NSString *)label amount:(NSDecimalNumber *)amount;
+ (instancetype)summaryItemWithLabel:(NSString *)label amount:(NSDecimalNumber *)amount type:(PKPaymentSummaryItemType)type;
@property (nonatomic, copy) NSString *label;
@property (nonatomic, copy) NSDecimalNumber *amount;
@end

@interface PKShippingMethod : PKPaymentSummaryItem
@property (nonatomic, copy, nullable) NSString *identifier;
@property (nonatomic, copy, nullable) NSString *detail;
@end

@interface PKPaymentRequest : NSObject
+ (NSArray<PKPaymentNetwork> *)availableNetworks;
@property (nonatomic, copy) NSString *countryCode;
@property (nonatomic, copy) NSArray<PKPaymentNetwork> *supportedNetworks;
@property (nonatomic, assign) PKMerchantCapability merchantCapabilities;
@property (nonatomic, copy) NSArray<PKPaymentSummaryItem *> *paymentSummaryItems;
@property (nonatomic, copy) NSString *currencyCode;
@property (nonatomic, assign) PKAddressField requiredBillingAddressFields;
@property (nonatomic, strong, nullable) PKContact *billingContact;
@property (nonatomic, assign) PKAddressField requiredShippingAddressFields;
@property (nonatomic, strong, nullable) PKContact *shippingContact;
@property (nonatomic, copy, nullable) NSArray<PKShippingMethod *> *shippingMethods;
@property (nonatomic, assign) PKShippingType shippingType;
@property (nonatomic, copy, nullable) NSData *applicationData;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
@property (nonatomic, copy, nullable) NSSet<NSString *> *supportedCountries;
@property (nonatomic, strong) NSSet<PKContactField> *requiredShippingContactFields;
@property (nonatomic, strong) NSSet<PKContactField> *requiredBillingContactFields;
#endif
@end

@interface PKPaymentAuthorizationViewController : NSViewController
+ (void)requestViewControllerWithPaymentRequest:(PKPaymentRequest *)paymentRequest completion:(void(^)(PKPaymentAuthorizationViewController *viewController, NSError *error))completion;
+ (BOOL)canMakePayments;
@property (nonatomic, assign, nullable) id<PKPaymentAuthorizationViewControllerDelegate> delegate;
@end

@protocol PKPaymentAuthorizationViewControllerDelegate <NSObject>
@required

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didAuthorizePayment:(PKPayment *)payment handler:(void (^)(PKPaymentAuthorizationResult *result))completion;
#else
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didAuthorizePayment:(PKPayment *)payment completion:(void (^)(PKPaymentAuthorizationStatus status))completion;
#endif

- (void)paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)controller;

@optional
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKPaymentSummaryItem *> *summaryItems))completion;
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingContact:(PKContact *)contact completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKShippingMethod *> *shippingMethods, NSArray<PKPaymentSummaryItem *> *summaryItems))completion;
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(void (^)(NSArray<PKPaymentSummaryItem *> *summaryItems))completion;
@end

@interface PKPassLibrary : NSObject
@end

NS_ASSUME_NONNULL_END

#endif

NS_ASSUME_NONNULL_BEGIN

@protocol PKPaymentAuthorizationViewControllerPrivateDelegate;

@interface PKPaymentMerchantSession : NSObject <NSSecureCoding, NSCopying>
- (instancetype)initWithDictionary:(NSDictionary *)dictionary;
@end

@interface PKPaymentAuthorizationViewController ()
+ (void)paymentServicesMerchantURL:(void(^)(NSURL *merchantURL, NSError *error))completion;
@property (nonatomic, assign, nullable) id<PKPaymentAuthorizationViewControllerPrivateDelegate> privateDelegate;
@end

@protocol PKPaymentAuthorizationViewControllerPrivateDelegate <NSObject>
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller willFinishWithError:(NSError *)error;

@optional
- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didRequestMerchantSession:(void(^)(PKPaymentMerchantSession *, NSError *))sessionBlock;
@end

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101304) || PLATFORM(IOS_FAMILY)
typedef NS_ENUM(NSUInteger, PKPaymentRequestAPIType) {
    PKPaymentRequestAPITypeInApp = 0,
    PKPaymentRequestAPITypeWebJS,
    PKPaymentRequestAPITypeWebPaymentRequest,
};

@interface PKPaymentRequest ()
@property (nonatomic, assign) PKPaymentRequestAPIType APIType;
@end
#endif

NS_ASSUME_NONNULL_END

#endif

#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK)
typedef NS_ENUM(NSInteger, PKPaymentButtonStyle) {
    PKPaymentButtonStyleWhite = 0,
    PKPaymentButtonStyleWhiteOutline,
    PKPaymentButtonStyleBlack
};

typedef NS_ENUM(NSInteger, PKPaymentButtonType) {
    PKPaymentButtonTypePlain = 0,
    PKPaymentButtonTypeBuy,
    PKPaymentButtonTypeSetUp,
    PKPaymentButtonTypeInStore,
    PKPaymentButtonTypeDonate,
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400) || PLATFORM(IOS_FAMILY)
    PKPaymentButtonTypeCheckout,
    PKPaymentButtonTypeBook,
    PKPaymentButtonTypeSubscribe,
#endif
};
#endif

#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300

NS_ASSUME_NONNULL_BEGIN

@interface PKPaymentAuthorizationResult : NSObject
- (instancetype)initWithStatus:(PKPaymentAuthorizationStatus)status errors:(nullable NSArray<NSError *> *)errors;
@property (nonatomic, assign) PKPaymentAuthorizationStatus status;
@end

@interface PKPaymentRequestPaymentMethodUpdate : NSObject
- (instancetype)initWithPaymentSummaryItems:(nonnull NSArray<PKPaymentSummaryItem *> *)paymentSummaryItems;
@end

@interface PKPaymentRequestUpdate : NSObject
@end

@interface PKPaymentRequestShippingContactUpdate : PKPaymentRequestUpdate
- (instancetype)initWithPaymentSummaryItems:(nonnull NSArray<PKPaymentSummaryItem *> *)summaryItems shippingMethods:(nonnull NSArray<PKShippingMethod *> *)shippingMethods;
- (instancetype)initWithErrors:(nullable NSArray<NSError *> *)errors paymentSummaryItems:(nonnull NSArray<PKPaymentSummaryItem *> *)summaryItems shippingMethods:(nonnull NSArray<PKShippingMethod *> *)shippingMethods;
@end

NS_ASSUME_NONNULL_END

#endif

extern "C"
void PKDrawApplePayButton(_Nonnull CGContextRef, CGRect drawRect, CGFloat scale, PKPaymentButtonType, PKPaymentButtonStyle, NSString * _Nullable languageCode);

NS_ASSUME_NONNULL_BEGIN

@interface PKPassLibrary ()
- (void)openPaymentSetupForMerchantIdentifier:(NSString *)identifier domain:(NSString *)domain completion:(void(^)(BOOL success))completion;
@end

@interface PKPaymentRequest ()
@property (nonatomic, strong) NSString *sourceApplicationBundleIdentifier;
@property (nonatomic, strong) NSString *sourceApplicationSecondaryIdentifier;
@property (nonatomic, strong) NSString *CTDataConnectionServiceType;
@end

NS_ASSUME_NONNULL_END
