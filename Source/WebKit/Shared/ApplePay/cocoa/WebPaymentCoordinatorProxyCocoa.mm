/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "WebPaymentCoordinatorProxyCocoa.h"

#if ENABLE(APPLE_PAY)

#import "WebPaymentCoordinatorProxy.h"
#import "WebProcessPool.h"
#import <WebCore/PaymentAuthorizationStatus.h>
#import <WebCore/PaymentHeaders.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>

#if HAVE(PASSKIT_GRANULAR_ERRORS)
SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressStreetKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressSubLocalityKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressCityKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressSubAdministrativeAreaKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressStateKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressPostalCodeKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressCountryKey, NSString *);
SOFT_LINK_CONSTANT(Contacts, CNPostalAddressISOCountryCodeKey, NSString *);
SOFT_LINK_CONSTANT(PAL::PassKit, PKPaymentErrorDomain, NSString *);
#endif

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
#if HAVE(PASSKIT_GRANULAR_ERRORS)
        _paymentAuthorizedCompletion(adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:PKPaymentAuthorizationStatusFailure errors:@[ ]]).get());
#else
        _paymentAuthorizedCompletion(PKPaymentAuthorizationStatusFailure);
#endif
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

    [PAL::getPKPaymentAuthorizationViewControllerClass() paymentServicesMerchantURL:^(NSURL *merchantURL, NSError *error) {
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

static WebCore::ApplePaySessionPaymentRequest::ShippingMethod toShippingMethod(PKShippingMethod *shippingMethod)
{
    ASSERT(shippingMethod);

    WebCore::ApplePaySessionPaymentRequest::ShippingMethod result;
    result.label = shippingMethod.label;
    result.detail = shippingMethod.detail;
    result.amount = shippingMethod.amount.stringValue;
    result.identifier = shippingMethod.identifier;

    return result;
}

#if HAVE(PASSKIT_GRANULAR_ERRORS)

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didAuthorizePayment:(PKPayment *)payment handler:(void (^)(PKPaymentAuthorizationResult *result))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:PKPaymentAuthorizationStatusFailure errors:@[ ]]).get());
        return;
    }

    ASSERT(!_paymentAuthorizedCompletion);
    _paymentAuthorizedCompletion = completion;

    _webPaymentCoordinatorProxy->didAuthorizePayment(WebCore::Payment(payment));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod handler:(void (^)(PKPaymentRequestPaymentMethodUpdate *update))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:@[ ]]).get());
        return;
    }

    ASSERT(!_didSelectPaymentMethodCompletion);
    _didSelectPaymentMethodCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectPaymentMethod(WebCore::PaymentMethod(paymentMethod));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod handler:(void (^)(PKPaymentRequestShippingMethodUpdate *update))completion {
    if (!_webPaymentCoordinatorProxy) {
        completion(adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:@[ ]]).get());
        return;
    }

    ASSERT(!_didSelectShippingMethodCompletion);
    _didSelectShippingMethodCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingMethod(toShippingMethod(shippingMethod));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingContact:(PKContact *)contact handler:(void (^)(PKPaymentRequestShippingContactUpdate *update))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithErrors:@[ ] paymentSummaryItems:@[ ] shippingMethods:@[ ]]).get());
        return;
    }

    ASSERT(!_didSelectShippingContactCompletion);
    _didSelectShippingContactCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingContact(WebCore::PaymentContact(contact));
}

#endif

#if !HAVE(PASSKIT_GRANULAR_ERRORS)

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

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingMethod:(PKShippingMethod *)shippingMethod completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(PKPaymentAuthorizationStatusFailure, @[ ]);
        return;
    }

    ASSERT(!_didSelectShippingMethodCompletion);
    _didSelectShippingMethodCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingMethod(toShippingMethod(shippingMethod));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectPaymentMethod:(PKPaymentMethod *)paymentMethod completion:(void (^)(NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(@[ ]);
        return;
    }

    ASSERT(!_didSelectPaymentMethodCompletion);
    _didSelectPaymentMethodCompletion = completion;

    _webPaymentCoordinatorProxy->didSelectPaymentMethod(WebCore::PaymentMethod(paymentMethod));
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController *)controller didSelectShippingContact:(PKContact *)contact completion:(void (^)(PKPaymentAuthorizationStatus status, NSArray<PKShippingMethod *> *shippingMethods, NSArray<PKPaymentSummaryItem *> *summaryItems))completion
{
    if (!_webPaymentCoordinatorProxy) {
        completion(PKPaymentAuthorizationStatusFailure, @[ ], @[ ]);
        return;
    }

    ASSERT(!_didSelectShippingContactCompletion);
    _didSelectShippingContactCompletion = completion;
    _webPaymentCoordinatorProxy->didSelectShippingContact(WebCore::PaymentContact(contact));
}

#endif

- (void)paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController *)controller
{
    if (!_webPaymentCoordinatorProxy)
        return;

    if (!_didReachFinalState)
        _webPaymentCoordinatorProxy->didCancelPaymentSession();

    _webPaymentCoordinatorProxy->hidePaymentUI();
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
    return [PAL::getPKPaymentAuthorizationViewControllerClass() canMakePayments];
}

void WebPaymentCoordinatorProxy::platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void(bool)>&& completionHandler)
{
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication(merchantIdentifier, domainName, m_client.paymentCoordinatorSourceApplicationSecondaryIdentifier(*this), makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL canMakePayments, NSError *error) mutable {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), canMakePayments] {
            completionHandler(canMakePayments);
        });
    }).get());
#else
    PKCanMakePaymentsWithMerchantIdentifierAndDomain(merchantIdentifier, domainName, makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL canMakePayments, NSError *error) mutable {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), canMakePayments] {
            completionHandler(canMakePayments);
        });
    }).get());
#endif
}

void WebPaymentCoordinatorProxy::platformOpenPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void(bool)>&& completionHandler)
{
    auto passLibrary = adoptNS([PAL::allocPKPassLibraryInstance() init]);
    [passLibrary openPaymentSetupForMerchantIdentifier:merchantIdentifier domain:domainName completion:makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL result) mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), result] {
            completionHandler(result);
        });
    }).get()];
}

#if HAVE(PASSKIT_GRANULAR_ERRORS)
static RetainPtr<NSSet> toPKContactFields(const WebCore::ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    Vector<NSString *> result;

    if (contactFields.postalAddress)
        result.append(PAL::get_PassKit_PKContactFieldPostalAddress());
    if (contactFields.phone)
        result.append(PAL::get_PassKit_PKContactFieldPhoneNumber());
    if (contactFields.email)
        result.append(PAL::get_PassKit_PKContactFieldEmailAddress());
    if (contactFields.name)
        result.append(PAL::get_PassKit_PKContactFieldName());
    if (contactFields.phoneticName)
        result.append(PAL::get_PassKit_PKContactFieldPhoneticName());

    return adoptNS([[NSSet alloc] initWithObjects:result.data() count:result.size()]);
}
#else
static PKAddressField toPKAddressField(const WebCore::ApplePaySessionPaymentRequest::ContactFields& contactFields)
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
#endif

static PKPaymentSummaryItemType toPKPaymentSummaryItemType(WebCore::ApplePaySessionPaymentRequest::LineItem::Type type)
{
    switch (type) {
    case WebCore::ApplePaySessionPaymentRequest::LineItem::Type::Final:
        return PKPaymentSummaryItemTypeFinal;

    case WebCore::ApplePaySessionPaymentRequest::LineItem::Type::Pending:
        return PKPaymentSummaryItemTypePending;
    }
}

static NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

static RetainPtr<PKPaymentSummaryItem> toPKPaymentSummaryItem(const WebCore::ApplePaySessionPaymentRequest::LineItem& lineItem)
{
    return [PAL::getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
}

static RetainPtr<NSArray> toPKPaymentSummaryItems(const WebCore::ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
    for (auto& lineItem : totalAndLineItems.lineItems) {
        if (auto summaryItem = toPKPaymentSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem.get()];
    }

    if (auto totalItem = toPKPaymentSummaryItem(totalAndLineItems.total))
        [paymentSummaryItems addObject:totalItem.get()];

    return paymentSummaryItems;
}

static PKMerchantCapability toPKMerchantCapabilities(const WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
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

static RetainPtr<NSArray> toSupportedNetworks(const Vector<String>& supportedNetworks)
{
    auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:supportedNetworks.size()]);
    for (auto& supportedNetwork : supportedNetworks)
        [result addObject:supportedNetwork];
    return result;
}

static PKShippingType toPKShippingType(WebCore::ApplePaySessionPaymentRequest::ShippingType shippingType)
{
    switch (shippingType) {
    case WebCore::ApplePaySessionPaymentRequest::ShippingType::Shipping:
        return PKShippingTypeShipping;

    case WebCore::ApplePaySessionPaymentRequest::ShippingType::Delivery:
        return PKShippingTypeDelivery;

    case WebCore::ApplePaySessionPaymentRequest::ShippingType::StorePickup:
        return PKShippingTypeStorePickup;

    case WebCore::ApplePaySessionPaymentRequest::ShippingType::ServicePickup:
        return PKShippingTypeServicePickup;
    }
}

static RetainPtr<PKShippingMethod> toPKShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    RetainPtr<PKShippingMethod> result = [PAL::getPKShippingMethodClass() summaryItemWithLabel:shippingMethod.label amount:toDecimalNumber(shippingMethod.amount)];
    [result setIdentifier:shippingMethod.identifier];
    [result setDetail:shippingMethod.detail];

    return result;
}
    
#if HAVE(PASSKIT_GRANULAR_ERRORS)
static RetainPtr<NSSet> toNSSet(const Vector<String>& strings)
{
    if (strings.isEmpty())
        return nil;

    auto mutableSet = adoptNS([[NSMutableSet alloc] initWithCapacity:strings.size()]);
    for (auto& string : strings)
        [mutableSet addObject:string];

    return WTFMove(mutableSet);
}

static PKPaymentRequestAPIType toAPIType(WebCore::ApplePaySessionPaymentRequest::Requester requester)
{
    switch (requester) {
    case WebCore::ApplePaySessionPaymentRequest::Requester::ApplePayJS:
        return PKPaymentRequestAPITypeWebJS;
    case WebCore::ApplePaySessionPaymentRequest::Requester::PaymentRequest:
        return PKPaymentRequestAPITypeWebPaymentRequest;
    }
}
#endif

RetainPtr<PKPaymentRequest> WebPaymentCoordinatorProxy::platformPaymentRequest(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest& paymentRequest)
{
    auto result = adoptNS([PAL::allocPKPaymentRequestInstance() init]);

    [result setOriginatingURL:originatingURL];

    if ([result respondsToSelector:@selector(setThumbnailURLs:)]) {
        auto thumbnailURLs = adoptNS([[NSMutableArray alloc] init]);
        for (auto& linkIconURL : linkIconURLs)
            [thumbnailURLs addObject:static_cast<NSURL *>(linkIconURL)];

        [result setThumbnailURLs:thumbnailURLs.get()];
    } else if (!linkIconURLs.isEmpty())
        [result setThumbnailURL:linkIconURLs[0]];

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    [result setAPIType:toAPIType(paymentRequest.requester())];
#endif

    [result setCountryCode:paymentRequest.countryCode()];
    [result setCurrencyCode:paymentRequest.currencyCode()];
    [result setBillingContact:paymentRequest.billingContact().pkContact()];
    [result setShippingContact:paymentRequest.shippingContact().pkContact()];
#if HAVE(PASSKIT_GRANULAR_ERRORS)
    [result setRequiredBillingContactFields:toPKContactFields(paymentRequest.requiredBillingContactFields()).get()];
    [result setRequiredShippingContactFields:toPKContactFields(paymentRequest.requiredShippingContactFields()).get()];
#else
    [result setRequiredBillingAddressFields:toPKAddressField(paymentRequest.requiredBillingContactFields())];
    [result setRequiredShippingAddressFields:toPKAddressField(paymentRequest.requiredShippingContactFields())];
#endif

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

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    [result setSupportedCountries:toNSSet(paymentRequest.supportedCountries()).get()];
#endif

    // FIXME: Instead of using respondsToSelector, this should use a proper #if version check.
    auto& bundleIdentifier = m_client.paymentCoordinatorSourceApplicationBundleIdentifier(*this);
    if (!bundleIdentifier.isEmpty() && [result respondsToSelector:@selector(setSourceApplicationBundleIdentifier:)])
        [result setSourceApplicationBundleIdentifier:bundleIdentifier];

    auto& secondaryIdentifier = m_client.paymentCoordinatorSourceApplicationSecondaryIdentifier(*this);
    if (!secondaryIdentifier.isEmpty() && [result respondsToSelector:@selector(setSourceApplicationSecondaryIdentifier:)])
        [result setSourceApplicationSecondaryIdentifier:secondaryIdentifier];

#if PLATFORM(IOS_FAMILY)
    auto& serviceType = m_client.paymentCoordinatorCTDataConnectionServiceType(*this);
    if (!serviceType.isEmpty() && [result respondsToSelector:@selector(setCTDataConnectionServiceType:)])
        [result setCTDataConnectionServiceType:serviceType];
#endif

    return result;
}

#if HAVE(PASSKIT_GRANULAR_ERRORS)
static PKPaymentAuthorizationStatus toPKPaymentAuthorizationStatus(WebCore::PaymentAuthorizationStatus status)
{
    switch (status) {
    case WebCore::PaymentAuthorizationStatus::Success:
        return PKPaymentAuthorizationStatusSuccess;
    case WebCore::PaymentAuthorizationStatus::Failure:
        return PKPaymentAuthorizationStatusFailure;
    case WebCore::PaymentAuthorizationStatus::PINRequired:
        return PKPaymentAuthorizationStatusPINRequired;
    case WebCore::PaymentAuthorizationStatus::PINIncorrect:
        return PKPaymentAuthorizationStatusPINIncorrect;
    case WebCore::PaymentAuthorizationStatus::PINLockout:
        return PKPaymentAuthorizationStatusPINLockout;
    }
}

static PKPaymentErrorCode toPKPaymentErrorCode(WebCore::PaymentError::Code code)
{
    switch (code) {
    case WebCore::PaymentError::Code::Unknown:
        return PKPaymentUnknownError;
    case WebCore::PaymentError::Code::ShippingContactInvalid:
        return PKPaymentShippingContactInvalidError;
    case WebCore::PaymentError::Code::BillingContactInvalid:
        return PKPaymentBillingContactInvalidError;
    case WebCore::PaymentError::Code::AddressUnserviceable:
        return PKPaymentShippingAddressUnserviceableError;
    }
}

static RetainPtr<NSError> toNSError(const WebCore::PaymentError& error)
{
    auto userInfo = adoptNS([[NSMutableDictionary alloc] init]);
    [userInfo setObject:error.message forKey:NSLocalizedDescriptionKey];

    if (error.contactField) {
        NSString *pkContactField = nil;
        NSString *postalAddressKey = nil;

        switch (*error.contactField) {
        case WebCore::PaymentError::ContactField::PhoneNumber:
            pkContactField = PAL::get_PassKit_PKContactFieldPhoneNumber();
            break;

        case WebCore::PaymentError::ContactField::EmailAddress:
            pkContactField = PAL::get_PassKit_PKContactFieldEmailAddress();
            break;

        case WebCore::PaymentError::ContactField::Name:
            pkContactField = PAL::get_PassKit_PKContactFieldName();
            break;

        case WebCore::PaymentError::ContactField::PhoneticName:
            pkContactField = PAL::get_PassKit_PKContactFieldPhoneticName();
            break;

        case WebCore::PaymentError::ContactField::PostalAddress:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            break;

        case WebCore::PaymentError::ContactField::AddressLines:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressStreetKey();
            break;

        case WebCore::PaymentError::ContactField::SubLocality:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressSubLocalityKey();
            break;

        case WebCore::PaymentError::ContactField::Locality:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressCityKey();
            break;

        case WebCore::PaymentError::ContactField::PostalCode:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressPostalCodeKey();
            break;

        case WebCore::PaymentError::ContactField::SubAdministrativeArea:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressSubAdministrativeAreaKey();
            break;

        case WebCore::PaymentError::ContactField::AdministrativeArea:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressStateKey();
            break;

        case WebCore::PaymentError::ContactField::Country:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressCountryKey();
            break;

        case WebCore::PaymentError::ContactField::CountryCode:
            pkContactField = PAL::get_PassKit_PKContactFieldPostalAddress();
            postalAddressKey = getCNPostalAddressISOCountryCodeKey();
            break;
        }

        [userInfo setObject:pkContactField forKey:PAL::get_PassKit_PKPaymentErrorContactFieldUserInfoKey()];
        if (postalAddressKey)
            [userInfo setObject:postalAddressKey forKey:PAL::get_PassKit_PKPaymentErrorPostalAddressUserInfoKey()];
    }

    return adoptNS([[NSError alloc] initWithDomain:getPKPaymentErrorDomain() code:toPKPaymentErrorCode(error.code) userInfo:userInfo.get()]);
}

static RetainPtr<NSArray> toNSErrors(const Vector<WebCore::PaymentError>& errors)
{
    auto result = adoptNS([[NSMutableArray alloc] init]);

    for (const auto& error : errors) {
        if (auto nsError = toNSError(error))
            [result addObject:nsError.get()];
    }

    return result;
}
#else
static PKPaymentAuthorizationStatus toPKPaymentAuthorizationStatus(const Optional<WebCore::PaymentAuthorizationResult>& result)
{
    if (!result)
        return PKPaymentAuthorizationStatusSuccess;

    if (result->errors.size() == 1) {
        auto& error = result->errors[0];
        switch (error.code) {
        case WebCore::PaymentError::Code::Unknown:
        case WebCore::PaymentError::Code::AddressUnserviceable:
            return PKPaymentAuthorizationStatusFailure;

        case WebCore::PaymentError::Code::BillingContactInvalid:
            return PKPaymentAuthorizationStatusInvalidBillingPostalAddress;

        case WebCore::PaymentError::Code::ShippingContactInvalid:
            if (error.contactField && error.contactField == WebCore::PaymentError::ContactField::PostalAddress)
                return PKPaymentAuthorizationStatusInvalidShippingPostalAddress;

            return PKPaymentAuthorizationStatusInvalidShippingContact;
        }
    }

    switch (result->status) {
    case WebCore::PaymentAuthorizationStatus::Success:
        return PKPaymentAuthorizationStatusSuccess;
    case WebCore::PaymentAuthorizationStatus::Failure:
        return PKPaymentAuthorizationStatusFailure;
    case WebCore::PaymentAuthorizationStatus::PINRequired:
        return PKPaymentAuthorizationStatusPINRequired;
    case WebCore::PaymentAuthorizationStatus::PINIncorrect:
        return PKPaymentAuthorizationStatusPINIncorrect;
    case WebCore::PaymentAuthorizationStatus::PINLockout:
        return PKPaymentAuthorizationStatusPINLockout;
    }

    return PKPaymentAuthorizationStatusFailure;
}
#endif

void WebPaymentCoordinatorProxy::platformCompletePaymentSession(const Optional<WebCore::PaymentAuthorizationResult>& result)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    m_paymentAuthorizationViewControllerDelegate->_didReachFinalState = WebCore::isFinalStateResult(result);

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto status = result ? result->status : WebCore::PaymentAuthorizationStatus::Success;
    auto pkPaymentAuthorizationResult = adoptNS([PAL::allocPKPaymentAuthorizationResultInstance() initWithStatus:toPKPaymentAuthorizationStatus(status) errors:result ? toNSErrors(result->errors).get() : @[ ]]);
    m_paymentAuthorizationViewControllerDelegate->_paymentAuthorizedCompletion(pkPaymentAuthorizationResult.get());
#else
    m_paymentAuthorizationViewControllerDelegate->_paymentAuthorizedCompletion(toPKPaymentAuthorizationStatus(result));
#endif
    m_paymentAuthorizationViewControllerDelegate->_paymentAuthorizedCompletion = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompleteMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    m_paymentAuthorizationViewControllerDelegate->_sessionBlock(paymentMerchantSession.pkPaymentMerchantSession(), nullptr);
    m_paymentAuthorizationViewControllerDelegate->_sessionBlock = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompleteShippingMethodSelection(const Optional<WebCore::ShippingMethodUpdate>& update)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (update)
        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = toPKPaymentSummaryItems(update->newTotalAndLineItems);

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto pkShippingMethodUpdate = adoptNS([PAL::allocPKPaymentRequestShippingMethodUpdateInstance() initWithPaymentSummaryItems:m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get()]);
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingMethodCompletion(pkShippingMethodUpdate.get());
#else
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingMethodCompletion(PKPaymentAuthorizationStatusSuccess, m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
#endif
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingMethodCompletion = nullptr;
}

#if !HAVE(PASSKIT_GRANULAR_ERRORS)
static PKPaymentAuthorizationStatus toPKPaymentAuthorizationStatus(const Optional<WebCore::ShippingContactUpdate>& update)
{
    if (!update || update->errors.isEmpty())
        return PKPaymentAuthorizationStatusSuccess;

    if (update->errors.size() == 1) {
        auto& error = update->errors[0];
        switch (error.code) {
        case WebCore::PaymentError::Code::Unknown:
        case WebCore::PaymentError::Code::AddressUnserviceable:
            return PKPaymentAuthorizationStatusFailure;

        case WebCore::PaymentError::Code::BillingContactInvalid:
            return PKPaymentAuthorizationStatusInvalidBillingPostalAddress;

        case WebCore::PaymentError::Code::ShippingContactInvalid:
            if (error.contactField && error.contactField == WebCore::PaymentError::ContactField::PostalAddress)
                return PKPaymentAuthorizationStatusInvalidShippingPostalAddress;

            return PKPaymentAuthorizationStatusInvalidShippingContact;
        }
    }

    return PKPaymentAuthorizationStatusFailure;
}
#endif

void WebPaymentCoordinatorProxy::platformCompleteShippingContactSelection(const Optional<WebCore::ShippingContactUpdate>& update)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (update) {
        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = toPKPaymentSummaryItems(update->newTotalAndLineItems);

        auto shippingMethods = adoptNS([[NSMutableArray alloc] init]);
        for (auto& shippingMethod : update->newShippingMethods)
            [shippingMethods addObject:toPKShippingMethod(shippingMethod).get()];

        m_paymentAuthorizationViewControllerDelegate->_shippingMethods = WTFMove(shippingMethods);
    }

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto pkShippingContactUpdate = adoptNS([PAL::allocPKPaymentRequestShippingContactUpdateInstance() initWithErrors:update ? toNSErrors(update->errors).get() : @[ ] paymentSummaryItems:m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get() shippingMethods:m_paymentAuthorizationViewControllerDelegate->_shippingMethods.get()]);
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingContactCompletion(pkShippingContactUpdate.get());
#else
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingContactCompletion(toPKPaymentAuthorizationStatus(update), m_paymentAuthorizationViewControllerDelegate->_shippingMethods.get(), m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
#endif
    m_paymentAuthorizationViewControllerDelegate->_didSelectShippingContactCompletion = nullptr;
}

void WebPaymentCoordinatorProxy::platformCompletePaymentMethodSelection(const Optional<WebCore::PaymentMethodUpdate>& update)
{
    ASSERT(m_paymentAuthorizationViewController);
    ASSERT(m_paymentAuthorizationViewControllerDelegate);

    if (update)
        m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems = toPKPaymentSummaryItems(update->newTotalAndLineItems);

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    auto pkPaymentMethodUpdate = adoptNS([PAL::allocPKPaymentRequestPaymentMethodUpdateInstance() initWithPaymentSummaryItems:m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get()]);
    m_paymentAuthorizationViewControllerDelegate->_didSelectPaymentMethodCompletion(pkPaymentMethodUpdate.get());
#else
    m_paymentAuthorizationViewControllerDelegate->_didSelectPaymentMethodCompletion(m_paymentAuthorizationViewControllerDelegate->_paymentSummaryItems.get());
#endif
    m_paymentAuthorizationViewControllerDelegate->_didSelectPaymentMethodCompletion = nullptr;
}

Vector<String> WebPaymentCoordinatorProxy::platformAvailablePaymentNetworks()
{
    NSArray<PKPaymentNetwork> *availableNetworks = [PAL::getPKPaymentRequestClass() availableNetworks];
    Vector<String> result;
    result.reserveInitialCapacity(availableNetworks.count);
    for (PKPaymentNetwork network in availableNetworks)
        result.uncheckedAppend(network);
    return result;
}

} // namespace WebKit

#endif // ENABLE(APPLE_PAY)
