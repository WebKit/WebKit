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

#import "WKPaymentAuthorizationDelegate.h"
#import "WebPaymentCoordinatorProxy.h"
#import <WebCore/PaymentAuthorizationStatus.h>
#import <WebCore/PaymentHeaders.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>

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
#if PLATFORM(MAC)
    if (!PAL::isPassKitFrameworkAvailable())
        return false;
#endif

    return [PAL::getPKPaymentAuthorizationViewControllerClass() canMakePayments];
}

void WebPaymentCoordinatorProxy::platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, PAL::SessionID sessionID, WTF::Function<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    if (!PAL::isPassKitFrameworkAvailable())
        return completionHandler(false);
#endif

#if HAVE(PASSKIT_GRANULAR_ERRORS)
    PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication(merchantIdentifier, domainName, m_client.paymentCoordinatorSourceApplicationSecondaryIdentifier(*this, sessionID), makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL canMakePayments, NSError *error) mutable {
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
#if PLATFORM(MAC)
    if (!PAL::isPassKitFrameworkAvailable())
        return completionHandler(false);
#endif

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

PKPaymentSummaryItemType toPKPaymentSummaryItemType(WebCore::ApplePaySessionPaymentRequest::LineItem::Type type)
{
    switch (type) {
    case WebCore::ApplePaySessionPaymentRequest::LineItem::Type::Final:
        return PKPaymentSummaryItemTypeFinal;

    case WebCore::ApplePaySessionPaymentRequest::LineItem::Type::Pending:
        return PKPaymentSummaryItemTypePending;
    }
}

NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
}

PKPaymentSummaryItem *toPKPaymentSummaryItem(const WebCore::ApplePaySessionPaymentRequest::LineItem& lineItem)
{
    return [PAL::getPKPaymentSummaryItemClass() summaryItemWithLabel:lineItem.label amount:toDecimalNumber(lineItem.amount) type:toPKPaymentSummaryItemType(lineItem.type)];
}

NSArray *toPKPaymentSummaryItems(const WebCore::ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    NSMutableArray *paymentSummaryItems = [NSMutableArray arrayWithCapacity:totalAndLineItems.lineItems.size() + 1];
    for (auto& lineItem : totalAndLineItems.lineItems) {
        if (PKPaymentSummaryItem *summaryItem = toPKPaymentSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem];
    }

    if (PKPaymentSummaryItem *totalItem = toPKPaymentSummaryItem(totalAndLineItems.total))
        [paymentSummaryItems addObject:totalItem];

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

PKShippingMethod *toPKShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    PKShippingMethod *result = [PAL::getPKShippingMethodClass() summaryItemWithLabel:shippingMethod.label amount:toDecimalNumber(shippingMethod.amount)];
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
#endif

#if HAVE(PASSKIT_API_TYPE)
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

RetainPtr<PKPaymentRequest> WebPaymentCoordinatorProxy::platformPaymentRequest(const URL& originatingURL, const Vector<URL>& linkIconURLs, PAL::SessionID sessionID, const WebCore::ApplePaySessionPaymentRequest& paymentRequest)
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

#if HAVE(PASSKIT_API_TYPE)
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
        [shippingMethods addObject:toPKShippingMethod(shippingMethod)];
    [result setShippingMethods:shippingMethods.get()];

    auto paymentSummaryItems = adoptNS([[NSMutableArray alloc] init]);
    for (auto& lineItem : paymentRequest.lineItems()) {
        if (PKPaymentSummaryItem *summaryItem = toPKPaymentSummaryItem(lineItem))
            [paymentSummaryItems addObject:summaryItem];
    }

    if (PKPaymentSummaryItem *totalItem = toPKPaymentSummaryItem(paymentRequest.total()))
        [paymentSummaryItems addObject:totalItem];

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
    auto& bundleIdentifier = m_client.paymentCoordinatorSourceApplicationBundleIdentifier(*this, sessionID);
    if (!bundleIdentifier.isEmpty() && [result respondsToSelector:@selector(setSourceApplicationBundleIdentifier:)])
        [result setSourceApplicationBundleIdentifier:bundleIdentifier];

    auto& secondaryIdentifier = m_client.paymentCoordinatorSourceApplicationSecondaryIdentifier(*this, sessionID);
    if (!secondaryIdentifier.isEmpty() && [result respondsToSelector:@selector(setSourceApplicationSecondaryIdentifier:)])
        [result setSourceApplicationSecondaryIdentifier:secondaryIdentifier];

#if PLATFORM(IOS_FAMILY)
    auto& serviceType = m_client.paymentCoordinatorCTDataConnectionServiceType(*this, sessionID);
    if (!serviceType.isEmpty() && [result respondsToSelector:@selector(setCTDataConnectionServiceType:)])
        [result setCTDataConnectionServiceType:serviceType];
#endif

    return result;
}

void WebPaymentCoordinatorProxy::platformCompletePaymentSession(const Optional<WebCore::PaymentAuthorizationResult>& result)
{
    m_authorizationPresenter->completePaymentSession(result);
}

void WebPaymentCoordinatorProxy::platformCompleteMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    m_authorizationPresenter->completeMerchantValidation(paymentMerchantSession);
}

void WebPaymentCoordinatorProxy::platformCompleteShippingMethodSelection(const Optional<WebCore::ShippingMethodUpdate>& update)
{
    m_authorizationPresenter->completeShippingMethodSelection(update);
}

void WebPaymentCoordinatorProxy::platformCompleteShippingContactSelection(const Optional<WebCore::ShippingContactUpdate>& update)
{
    m_authorizationPresenter->completeShippingContactSelection(update);
}

void WebPaymentCoordinatorProxy::platformCompletePaymentMethodSelection(const Optional<WebCore::PaymentMethodUpdate>& update)
{
    m_authorizationPresenter->completePaymentMethodSelection(update);
}

Vector<String> WebPaymentCoordinatorProxy::platformAvailablePaymentNetworks()
{
#if PLATFORM(MAC)
    if (!PAL::isPassKitFrameworkAvailable())
        return { };
#endif

    NSArray<PKPaymentNetwork> *availableNetworks = [PAL::getPKPaymentRequestClass() availableNetworks];
    Vector<String> result;
    result.reserveInitialCapacity(availableNetworks.count);
    for (PKPaymentNetwork network in availableNetworks)
        result.uncheckedAppend(network);
    return result;
}

} // namespace WebKit

#endif // ENABLE(APPLE_PAY)
