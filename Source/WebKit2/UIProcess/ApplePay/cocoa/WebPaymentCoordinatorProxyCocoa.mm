/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebPaymentCoordinatorProxyCocoa.h"

#if ENABLE(APPLE_PAY)

#import "WebPaymentCoordinatorProxy.h"
#import <PassKit/PKPaymentAuthorizationViewController_Private.h>
#import <PassKitCore/PKPaymentMerchantSession.h>
#import <WebCore/PaymentAuthorizationStatus.h>
#import <WebCore/PaymentHeaders.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/URL.h>
#import <wtf/RunLoop.h>

#if PLATFORM(MAC)
SOFT_LINK_PRIVATE_FRAMEWORK(PassKit)
#else
SOFT_LINK_FRAMEWORK(PassKit)
#endif

#if PLATFORM(MAC)
// FIXME: Once <rdar://problem/26074851> has been fixed we no longer
// have to fall back to PKInAppPaymentService.
#import <PassKitCore/PKInAppPaymentService.h>
SOFT_LINK_CLASS(PassKit, PKInAppPaymentService)
#endif

SOFT_LINK_CLASS(PassKit, PKPaymentAuthorizationViewController);
SOFT_LINK_CLASS(PassKit, PKPaymentMerchantSession);
SOFT_LINK_CLASS(PassKit, PKPaymentRequest);
SOFT_LINK_CLASS(PassKit, PKPaymentSummaryItem);
SOFT_LINK_CLASS(PassKit, PKShippingMethod);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkAmex, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkChinaUnionPay, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkDiscover, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkInterac, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkMasterCard, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkPrivateLabel, NSString *);
SOFT_LINK_CONSTANT(PassKit, PKPaymentNetworkVisa, NSString *);

typedef void (^PKCanMakePaymentsCompletion)(BOOL isValid, NSError *error);
extern "C" void PKCanMakePaymentsWithMerchantIdentifierAndDomain(NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion);

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, PassKit, PKCanMakePaymentsWithMerchantIdentifierAndDomain, void, (NSString *identifier, NSString *domain, PKCanMakePaymentsCompletion completion), (identifier, domain, completion));

@implementation WKPaymentAuthorizationViewControllerDelegate

- (instancetype)initWithPaymentCoordinatorProxy:(WebKit::WebPaymentCoordinatorProxy&)webPaymentCoordinatorProxy
{
    if (!(self = [super init]))
        return nullptr;

    _webPaymentCoordinatorProxy = &webPaymentCoordinatorProxy;

    return self;
}

- (void)invalidate
{
    _webPaymentCoordinatorProxy = nullptr;
    if (_paymentAuthorizedCompletion) {
        _paymentAuthorizedCompletion(PKPaymentAuthorizationStatusFailure);
        _paymentAuthorizedCompletion = nullptr;
    }
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller willFinishWithError:(NSError *)error
{
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didRequestMerchantSession:(void(^)(PKPaymentMerchantSession *, NSError *))sessionBlock
{
    ASSERT(!_sessionBlock);
    _sessionBlock = sessionBlock;

    [getPKPaymentAuthorizationViewControllerClass() paymentServicesMerchantURL:^(NSURL *merchantURL, NSError *error) {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        dispatch_async(dispatch_get_main_queue(), ^{
            ASSERT(_sessionBlock);

            if (!_webPaymentCoordinatorProxy) {
                _sessionBlock(nullptr, nullptr);
                return;
            }

            _webPaymentCoordinatorProxy->validateMerchant(merchantURL);
        });
    }];
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didAuthorizePayment:(PKPayment *)payment completion:(void (^)(PKPaymentAuthorizationStatus))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(PKPaymentAuthorizationStatusFailure);
        return;
    }

    ASSERT(!_paymentAuthorizedCompletion);
    _paymentAuthorizedCompletion = completion;

    _webPaymentCoordinatorProxy->didAuthorizePayment(WebCore::Payment(payment));
}

- (void)paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)controller
{
    if (!_webPaymentCoordinatorProxy)
        return;

    if (!_didReachFinalState)
        _webPaymentCoordinatorProxy->didCancelPayment();

    _webPaymentCoordinatorProxy->hidePaymentUI();
}

static WebCore::PaymentRequest::ShippingMethod toShippingMethod(PKShippingMethod *shippingMethod)
{
    ASSERT(shippingMethod);

    WebCore::PaymentRequest::ShippingMethod result;
    result.label = shippingMethod.label;
    result.detail = shippingMethod.detail;
    result.amount = [shippingMethod.amount decimalNumberByMultiplyingByPowerOf10:2].integerValue;
    result.identifier = shippingMethod.identifier;

    return result;
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(PKPaymentAuthorizationStatusFailure, @[]);
        return;
    }

    ASSERT(!_didSelectShippingMethodCompletion);
    _didSelectShippingMethodCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingMethod(toShippingMethod(shippingMethod));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingContact:(PKContact *)contact completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKShippingMethod *> *shippingMethods, NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(PKPaymentAuthorizationStatusFailure, @[], @[]);
        return;
    }

    ASSERT(!_didSelectShippingContactCompletion);
    _didSelectShippingContactCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingContact(WebCore::PaymentContact(contact));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(void (^)(NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(@[]);
        return;
    }

    ASSERT(!_didSelectPaymentMethodCompletion);
    _didSelectPaymentMethodCompletion = completion;

    _webPaymentCoordinatorProxy->didSelectPaymentMethod(WebCore::PaymentMethod(paymentMethod));
}

@end

// FIXME: Once rdar://problem/24420024 has been fixed, import PKPaymentRequest_Private.h instead.
@interface PKPaymentRequest ()
@property (nonatomic, retain) NSURL *originatingURL;
@end

@interface PKPaymentRequest ()
// FIXME: Remove this once it's in an SDK.
@property (nonatomic, strong) NSArray *thumbnailURLs;
@property (nonatomic, strong) NSURL *thumbnailURL;

@property (nonatomic, assign) BOOL expectsMerchantSession;
@end

namespace WebKit {

bool WebPaymentCoordinatorProxy::platformCanMakePayments()
{
    return [getPKPaymentAuthorizationViewControllerClass() canMakePayments];
}

void WebPaymentCoordinatorProxy::platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, std::function<void (bool)> completionHandler)
{
    if (!canLoad_PassKit_PKCanMakePaymentsWithMerchantIdentifierAndDomain()) {
        RunLoop::main().dispatch([completionHandler] {
            completionHandler(false);
        });
        return;
    }

    softLink_PassKit_PKCanMakePaymentsWithMerchantIdentifierAndDomain(merchantIdentifier, domainName, [completionHandler](BOOL canMakePayments, NSError *error) {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        RunLoop::main().dispatch([completionHandler, canMakePayments] {
            completionHandler(canMakePayments);
        });
    });
}

static PKAddressField toPKAddressField(const WebCore::PaymentRequest::ContactFields& contactFields)
{
    PKAddressField result = 0;

    if (contactFields.postalAddress)
        result |= PKAddressFieldPostalAddress;
    if (contactFields.phone)
        result |= PKAddressFieldPhone;
    if (contactFields.email)
        result |= PKAddressFieldEmail;
    if (contactFields.name)
        result |= PKAddressFieldName;

    return result;
}

static PKPaymentSummaryItemType toPKPaymentSummaryItemType(WebCore::PaymentRequest::LineItem::Type type)
{
    switch (type) {
    case WebCore::PaymentRequest::LineItem::Type::Final:
        return PKPaymentSummaryItemTypeFinal;

    case WebCore::PaymentRequest::LineItem::Type::Pending:
        return PKPaymentSummaryItemTypePending;
    }
}

static RetainPtr<NSDecimalNumber> toDecimalNumber(int64_t value)
{
    return adoptNS([[NSDecimalNumber alloc] initWithMantissa:llabs(value) exponent:-2 isNegative:value < 0]);
}

static RetainPtr<PKPaymentSummaryItem> toPKPaymentSummaryItem(const WebCore::PaymentRequest::LineItem& lineItem)
{
    return [getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount.valueOr(0)).get() type:toPKPaymentSummaryItemType(lineItem.type)];
}

static PKMerchantCapability toPKMerchantCapabilities(const WebCore::PaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    PKMerchantCapability result = 0;
    if (merchantCapabilities.supports3DS)
        result |= PKMerchantCapability3DS;
    if (merchantCapabilities.supportsEMV)
        result |= PKMerchantCapabilityEMV;
    if (merchantCapabilities.supportsCredit)
        result |= PKMerchantCapabilityCredit;
    if (merchantCapabilities.supportsDebit)
        result |= PKMerchantCapabilityDebit;

    return result;
}

static RetainPtr<NSArray> toSupportedNetworks(const WebCore::PaymentRequest::SupportedNetworks& supportedNetworks)
{
    auto result = adoptNS([[NSMutableArray alloc] init]);

    if (supportedNetworks.amex)
        [result addObject:getPKPaymentNetworkAmex()];
    if (supportedNetworks.chinaUnionPay)
        [result addObject:getPKPaymentNetworkChinaUnionPay()];
    if (supportedNetworks.discover)
        [result addObject:getPKPaymentNetworkDiscover()];
    if (supportedNetworks.interac)
        [result addObject:getPKPaymentNetworkInterac()];
    if (supportedNetworks.masterCard)
        [result addObject:getPKPaymentNetworkMasterCard()];
    if (supportedNetworks.privateLabel)
        [result addObject:getPKPaymentNetworkPrivateLabel()];
    if (supportedNetworks.visa)
        [result addObject:getPKPaymentNetworkVisa()];

    return result;
}

static PKShippingType toPKShippingType(WebCore::PaymentRequest::ShippingType shippingType)
{
    switch (shippingType) {
    case WebCore::PaymentRequest::ShippingType::Shipping:
        return PKShippingTypeShipping;

    case WebCore::PaymentRequest::ShippingType::Delivery:
        return PKShippingTypeDelivery;

    case WebCore::PaymentRequest::ShippingType::StorePickup:
        return PKShippingTypeStorePickup;

    case WebCore::PaymentRequest::ShippingType::ServicePickup:
        return PKShippingTypeServicePickup;
    }
}

static RetainPtr<PKShippingMethod> toPKShippingMethod(const WebCore::PaymentRequest::ShippingMethod& shippingMethod)
{
    RetainPtr<PKShippingMethod> result = [getPKShippingMethodClass() summaryItemWithLabel:shippingMethod.label amount:toDecimalNumber(shippingMethod.amount).get()];
    [result setIdentifier:shippingMethod.identifier];
    [result setDetail:shippingMethod.detail];

    return result;
}

RetainPtr<PKPaymentRequest> toPKPaymentRequest(const WebCore::URL& originatingURL, const Vector<WebCore::URL>& linkIconURLs, const WebCore::PaymentRequest& paymentRequest)
{
    auto result = adoptNS([allocPKPaymentRequestInstance() init]);

    [result setOriginatingURL:originatingURL];

    if ([result respondsToSelector:@selector(setThumbnailURLs:)]) {
        auto thumbnailURLs = adoptNS([[NSMutableArray alloc] init]);
        for (auto& linkIconURL : linkIconURLs)
            [thumbnailURLs addObject:static_cast<NSURL *>(linkIconURL)];

        [result setThumbnailURLs:thumbnailURLs.get()];
    } else if (!linkIconURLs.isEmpty())
        [result setThumbnailURL:linkIconURLs[0]];

    [result setCountryCode:paymentRequest.countryCode()];
    [result setCurrencyCode:paymentRequest.currencyCode()];
    [result setRequiredBillingAddressFields:toPKAddressField(paymentRequest.requiredBillingContactFields())];
    [result setBillingContact:paymentRequest.billingContact().pkContact()];
    [result setRequiredShippingAddressFields:toPKAddressField(paymentRequest.requiredShippingContactFields())];
    [result setShippingContact:paymentRequest.shippingContact().pkContact()];

    [result setSupportedNetworks:toSupportedNetworks(paymentRequest.supportedNetworks()).get()];
    [result setMerchantCapabilities:toPKMerchantCapabilities(paymentRequest.merchantCapabilities())];

    [result setShippingType:toPKShippingType(paymentRequest.shippingType())];

    auto shippingMethods = adoptNS([[NSMutableArray alloc] init]);
    for (auto& shippingMethod : paymentRequest.shippingMethods())
        [shippingMethods addObject:toPKShippingMethod(shippingMethod).get()];
    [result setShippingMethods:shippingMethods.get()];

    auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
    for (auto& lineItem : paymentRequest.lineItems()) {
        if (auto summaryItem = toPKPaymentSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem.get()];
    }

    if (auto totalItem = toPKPaymentSummaryItem(paymentRequest.total()))
        [paymentSummaryItems addObject:totalItem.get()];

    [result setPaymentSummaryItems:paymentSummaryItems.get()];

    [result setExpectsMerchantSession:YES];

    if (!paymentRequest.applicationData().isNull()) {
        auto applicationData = adoptNS([[NSData alloc] initWithBase64EncodedString:paymentRequest.applicationData() options:0]);
        [result setApplicationData:applicationData.get()];
    }

    return result;
}

static PKPaymentAuthorizationStatus toPKPaymentAuthorizationStatus(WebCore::PaymentAuthorizationStatus status)
{
    switch (status) {
    case WebCore::PaymentAuthorizationStatus::Success:
        return PKPaymentAuthorizationStatusSuccess;
    case WebCore::PaymentAuthorizationStatus::Failure:
        return PKPaymentAuthorizationStatusFailure;
    case WebCore::PaymentAuthorizationStatus::InvalidBillingPostalAddress:
        return PKPaymentAuthorizationStatusInvalidBillingPostalAddress;
    case WebCore::PaymentAuthorizationStatus::InvalidShippingPostalAddress:
        return PKPaymentAuthorizationStatusInvalidShippingPostalAddress;
    case WebCore::PaymentAuthorizationStatus::InvalidShippingContact:
        return PKPaymentAuthorizationStatusInvalidShippingContact;
    case WebCore::PaymentAuthorizationStatus::PINRequired:
        return PKPaymentAuthorizationStatusPINRequired;
    case WebCore::PaymentAuthorizationStatus::PINIncorrect:
        return PKPaymentAuthorizationStatusPINIncorrect;
    case WebCore::PaymentAuthorizationStatus::PINLockout:
        return PKPaymentAuthorizationStatusPINLockout;
    }
}

void WebPaymentCoordinatorProxy::platformCompletePaymentSession(WebCore::PaymentAuthorizationStatus status)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    m_paymentAuthorizationViewControllerDelegate->_didReachFinalState = WebCore::isFinalStateStatus(status);
    m_paymentAuthorizationViewControllerDelegate->_paymentAuthorizedCompletion(toPKPaymentAuthorizationStatus(status));
    m_paymentAuthorizationViewControllerDelegate->_paymentAuthorizedCompletion = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompleteMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    m_paymentAuthorizationViewControllerDelegate->_sessionBlock(paymentMerchantSession.pkPaymentMerchantSession(), nullptr);
    m_paymentAuthorizationViewControllerDelegate->_sessionBlock = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompleteShippingMethodSelection(WebCore::PaymentAuthorizationStatus status, const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (newTotalAndLineItems) {
        auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
        for (auto& lineItem : newTotalAndLineItems->lineItems) {
            if (auto summaryItem = toPKPaymentSummaryItem(lineItem))
                [paymentSummaryItems addObject:summaryItem.get()];
        }

        if (auto totalItem = toPKPaymentSummaryItem(newTotalAndLineItems->total))
            [paymentSummaryItems addObject:totalItem.get()];

        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = WTFMove(paymentSummaryItems);
    }

    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingMethodCompletion(toPKPaymentAuthorizationStatus(status), m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingMethodCompletion = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompleteShippingContactSelection(WebCore::PaymentAuthorizationStatus status, const Vector<WebCore::PaymentRequest::ShippingMethod>& newShippingMethods, const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (newTotalAndLineItems) {
        auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
        for (auto& lineItem : newTotalAndLineItems->lineItems) {
            if (auto summaryItem = toPKPaymentSummaryItem(lineItem))
                [paymentSummaryItems addObject:summaryItem.get()];
        }

        if (auto totalItem = toPKPaymentSummaryItem(newTotalAndLineItems->total))
            [paymentSummaryItems addObject:totalItem.get()];

        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = WTFMove(paymentSummaryItems);

        auto shippingMethods = adoptNS([[NSMutableArray alloc] init]);
        for (auto& shippingMethod : newShippingMethods)
            [shippingMethods addObject:toPKShippingMethod(shippingMethod).get()];

        m_paymentAuthorizationViewControllerDelegate->_shippingMethods = WTFMove(shippingMethods);
    }

    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingContactCompletion(toPKPaymentAuthorizationStatus(status), m_paymentAuthorizationViewControllerDelegate->_shippingMethods.get(), m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingContactCompletion = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompletePaymentMethodSelection(const Optional<WebCore::PaymentRequest::TotalAndLineItems>& newTotalAndLineItems)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (newTotalAndLineItems) {
        auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
        for (auto& lineItem : newTotalAndLineItems->lineItems) {
            if (auto summaryItem = toPKPaymentSummaryItem(lineItem))
                [paymentSummaryItems addObject:summaryItem.get()];
        }

        if (auto totalItem = toPKPaymentSummaryItem(newTotalAndLineItems->total))
            [paymentSummaryItems addObject:totalItem.get()];

        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = WTFMove(paymentSummaryItems);
    }

    m_paymentAuthorizationViewControllerDelegate->_didSelectPaymentMethodCompletion(m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
    m_paymentAuthorizationViewControllerDelegate->_didSelectPaymentMethodCompletion = nullptr;
}

}

#endif

