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

#import "APIUIClient.h"
#import "ApplePayPaymentSetupFeaturesWebKit.h"
#import "AutomaticReloadPaymentRequest.h"
#import "PaymentSetupConfigurationWebKit.h"
#import "PaymentTokenContext.h"
#import "RecurringPaymentRequest.h"
#import "WKPaymentAuthorizationDelegate.h"
#import "WebPageProxy.h"
#import "WebPaymentCoordinatorProxy.h"
#import "WebPaymentCoordinatorProxyMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/ApplePayCouponCodeUpdate.h>
#import <WebCore/ApplePayPaymentMethodUpdate.h>
#import <WebCore/ApplePayShippingContactUpdate.h>
#import <WebCore/ApplePayShippingMethod.h>
#import <WebCore/ApplePayShippingMethodUpdate.h>
#import <WebCore/PaymentHeaders.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/PassKitSoftLink.h>

// FIXME: We don't support any platforms without -setThumbnailURLs:, so this can be removed.
@interface PKPaymentRequest ()
@property (nonatomic, strong) NSURL *thumbnailURL;
@end

namespace WebKit {

WebPaymentCoordinatorProxy::WebPaymentCoordinatorProxy(WebPaymentCoordinatorProxy::Client& client)
    : m_client(client)
    , m_canMakePaymentsQueue(WorkQueue::create("com.apple.WebKit.CanMakePayments"))
{
    m_client.paymentCoordinatorAddMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName(), *this);
}

WebPaymentCoordinatorProxy::~WebPaymentCoordinatorProxy()
{
    if (!canBegin())
        didReachFinalState();

    m_client.paymentCoordinatorRemoveMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName());
}

void WebPaymentCoordinatorProxy::platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    if (!PAL::isPassKitCoreFrameworkAvailable())
        return completionHandler(false);
#endif

    PKCanMakePaymentsWithMerchantIdentifierDomainAndSourceApplication(merchantIdentifier, domainName, m_client.paymentCoordinatorSourceApplicationSecondaryIdentifier(*this), makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL canMakePayments, NSError *error) mutable {
        if (error)
            LOG_ERROR("PKCanMakePaymentsWithMerchantIdentifierAndDomain error %@", error);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), canMakePayments] {
            completionHandler(canMakePayments);
        });
    }).get());
}

void WebPaymentCoordinatorProxy::platformOpenPaymentSetup(const String& merchantIdentifier, const String& domainName, WTF::Function<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    if (!PAL::isPassKitCoreFrameworkAvailable())
        return completionHandler(false);
#endif

    auto passLibrary = adoptNS([PAL::allocPKPassLibraryInstance() init]);
    [passLibrary openPaymentSetupForMerchantIdentifier:merchantIdentifier domain:domainName completion:makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL result) mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), result] {
            completionHandler(result);
        });
    }).get()];
}

static RetainPtr<NSSet> toPKContactFields(const WebCore::ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    Vector<NSString *> result;

    if (contactFields.postalAddress)
        result.append(PKContactFieldPostalAddress);
    if (contactFields.phone)
        result.append(PKContactFieldPhoneNumber);
    if (contactFields.email)
        result.append(PKContactFieldEmailAddress);
    if (contactFields.name)
        result.append(PKContactFieldName);
    if (contactFields.phoneticName)
        result.append(PKContactFieldPhoneticName);

    return adoptNS([[NSSet alloc] initWithObjects:result.data() count:result.size()]);
}

NSDecimalNumber *toDecimalNumber(const String& amount)
{
    if (!amount)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber decimalNumberWithString:amount locale:@{ NSLocaleDecimalSeparator : @"." }];
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

#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)

static RetainPtr<NSDateComponents> toNSDateComponents(const WebCore::ApplePayDateComponents& dateComponents)
{
    auto result = adoptNS([[NSDateComponents alloc] init]);
    [result setCalendar:[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian]];
    if (dateComponents.years)
        [result setYear:*dateComponents.years];
    if (dateComponents.months)
        [result setMonth:*dateComponents.months];
    if (dateComponents.days)
        [result setDay:*dateComponents.days];
    if (dateComponents.hours)
        [result setHour:*dateComponents.hours];
    return result;
}

static RetainPtr<PKDateComponentsRange> toPKDateComponentsRange(const WebCore::ApplePayDateComponentsRange& dateComponentsRange)
{
    return adoptNS([PAL::allocPKDateComponentsRangeInstance() initWithStartDateComponents:toNSDateComponents(dateComponentsRange.startDateComponents).get() endDateComponents:toNSDateComponents(dateComponentsRange.endDateComponents).get()]);
}

#endif // HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)

PKShippingMethod *toPKShippingMethod(const WebCore::ApplePayShippingMethod& shippingMethod)
{
    PKShippingMethod *result = [PAL::getPKShippingMethodClass() summaryItemWithLabel:shippingMethod.label amount:toDecimalNumber(shippingMethod.amount)];
    [result setIdentifier:shippingMethod.identifier];
    [result setDetail:shippingMethod.detail];
#if HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
    if (auto& dateComponentsRange = shippingMethod.dateComponentsRange)
        [result setDateComponentsRange:toPKDateComponentsRange(*dateComponentsRange).get()];
#endif
    return result;
}

#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)

PKShippingMethods *toPKShippingMethods(const Vector<WebCore::ApplePayShippingMethod>& webShippingMethods)
{
    RetainPtr<PKShippingMethod> defaultMethod;
    auto methods = createNSArray(webShippingMethods, [&defaultMethod] (const auto& webShippingMethod) {
        auto pkShippingMethod = toPKShippingMethod(webShippingMethod);
        if (webShippingMethod.selected)
            defaultMethod = pkShippingMethod;
        return pkShippingMethod;
    });
    return [PAL::allocPKShippingMethodsInstance() initWithMethods:methods.get() defaultMethod:defaultMethod.get()];
}

#endif // HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)

#if HAVE(PASSKIT_SHIPPING_CONTACT_EDITING_MODE)

static PKShippingContactEditingMode toPKShippingContactEditingMode(WebCore::ApplePayShippingContactEditingMode shippingContactEditingMode)
{
    switch (shippingContactEditingMode) {
    case WebCore::ApplePayShippingContactEditingMode::Enabled:
        return PKShippingContactEditingModeEnabled;

    case WebCore::ApplePayShippingContactEditingMode::StorePickup:
        return PKShippingContactEditingModeStorePickup;
    }

    ASSERT_NOT_REACHED();
    return PKShippingContactEditingModeEnabled;
}

#endif // HAVE(PASSKIT_SHIPPING_CONTACT_EDITING_MODE)

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

RetainPtr<PKPaymentRequest> WebPaymentCoordinatorProxy::platformPaymentRequest(const URL& originatingURL, const Vector<URL>& linkIconURLs, const WebCore::ApplePaySessionPaymentRequest& paymentRequest)
{
    auto result = adoptNS([PAL::allocPKPaymentRequestInstance() init]);

    [result setOriginatingURL:originatingURL];

    [result setThumbnailURLs:createNSArray(linkIconURLs).get()];

    [result setAPIType:toAPIType(paymentRequest.requester())];

    [result setCountryCode:paymentRequest.countryCode()];
    [result setCurrencyCode:paymentRequest.currencyCode()];
    [result setBillingContact:paymentRequest.billingContact().pkContact()];
    [result setShippingContact:paymentRequest.shippingContact().pkContact()];
    [result setRequiredBillingContactFields:toPKContactFields(paymentRequest.requiredBillingContactFields()).get()];
    [result setRequiredShippingContactFields:toPKContactFields(paymentRequest.requiredShippingContactFields()).get()];

    [result setSupportedNetworks:createNSArray(paymentRequest.supportedNetworks()).get()];
    [result setMerchantCapabilities:toPKMerchantCapabilities(paymentRequest.merchantCapabilities())];

    [result setShippingType:toPKShippingType(paymentRequest.shippingType())];

#if HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
    [result setAvailableShippingMethods:toPKShippingMethods(paymentRequest.shippingMethods())];
#else
    [result setShippingMethods:createNSArray(paymentRequest.shippingMethods(), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];
#endif

    [result setPaymentSummaryItems:WebCore::platformSummaryItems(paymentRequest.total(), paymentRequest.lineItems())];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [result setExpectsMerchantSession:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!paymentRequest.applicationData().isNull()) {
        auto applicationData = adoptNS([[NSData alloc] initWithBase64EncodedString:paymentRequest.applicationData() options:0]);
        [result setApplicationData:applicationData.get()];
    }

    [result setSupportedCountries:toNSSet(paymentRequest.supportedCountries()).get()];

    auto& boundInterfaceIdentifier = m_client.paymentCoordinatorBoundInterfaceIdentifier(*this);
    if (!boundInterfaceIdentifier.isEmpty())
        [result setBoundInterfaceIdentifier:boundInterfaceIdentifier];

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

#if HAVE(PASSKIT_INSTALLMENTS)
    if (PKPaymentInstallmentConfiguration *configuration = paymentRequest.installmentConfiguration().platformConfiguration()) {
        [result setInstallmentConfiguration:configuration];
        [result setRequestType:PKPaymentRequestTypeInstallment];
    }
#endif

#if HAVE(PASSKIT_COUPON_CODE)
    if (auto supportsCouponCode = paymentRequest.supportsCouponCode())
        [result setSupportsCouponCode:*supportsCouponCode];

    if (auto& couponCode = paymentRequest.couponCode(); !couponCode.isNull())
        [result setCouponCode:couponCode];
#endif

#if HAVE(PASSKIT_SHIPPING_CONTACT_EDITING_MODE)
    if (auto& shippingContactEditingMode = paymentRequest.shippingContactEditingMode())
        [result setShippingContactEditingMode:toPKShippingContactEditingMode(*shippingContactEditingMode)];
#endif

#if HAVE(PASSKIT_RECURRING_PAYMENTS)
    if (auto& recurringPaymentRequest = paymentRequest.recurringPaymentRequest())
        [result setRecurringPaymentRequest:platformRecurringPaymentRequest(*recurringPaymentRequest).get()];
#endif

#if HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
    if (auto& automaticReloadPaymentRequest = paymentRequest.automaticReloadPaymentRequest())
        [result setAutomaticReloadPaymentRequest:platformAutomaticReloadPaymentRequest(*automaticReloadPaymentRequest).get()];
#endif

#if HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
    if (auto& multiTokenContexts = paymentRequest.multiTokenContexts())
        [result setMultiTokenContexts:platformPaymentTokenContexts(*multiTokenContexts).get()];
#endif

    return result;
}

void WebPaymentCoordinatorProxy::platformCompletePaymentSession(WebCore::ApplePayPaymentAuthorizationResult&& result)
{
    m_authorizationPresenter->completePaymentSession(WTFMove(result));
}

void WebPaymentCoordinatorProxy::platformCompleteMerchantValidation(const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    m_authorizationPresenter->completeMerchantValidation(paymentMerchantSession);
}

void WebPaymentCoordinatorProxy::platformCompleteShippingMethodSelection(std::optional<WebCore::ApplePayShippingMethodUpdate>&& update)
{
    m_authorizationPresenter->completeShippingMethodSelection(WTFMove(update));
}

void WebPaymentCoordinatorProxy::platformCompleteShippingContactSelection(std::optional<WebCore::ApplePayShippingContactUpdate>&& update)
{
    m_authorizationPresenter->completeShippingContactSelection(WTFMove(update));
}

void WebPaymentCoordinatorProxy::platformCompletePaymentMethodSelection(std::optional<WebCore::ApplePayPaymentMethodUpdate>&& update)
{
    m_authorizationPresenter->completePaymentMethodSelection(WTFMove(update));
}

#if ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::platformCompleteCouponCodeChange(std::optional<WebCore::ApplePayCouponCodeUpdate>&& update)
{
    m_authorizationPresenter->completeCouponCodeChange(WTFMove(update));
}

#endif // ENABLE(APPLE_PAY_COUPON_CODE)

void WebPaymentCoordinatorProxy::getSetupFeatures(const PaymentSetupConfiguration& configuration, CompletionHandler<void(PaymentSetupFeatures&&)>&& reply)
{
#if PLATFORM(MAC)
    if (!PAL::getPKPaymentSetupControllerClass()) {
        reply({ });
        return;
    }
#endif

    auto completion = makeBlockPtr([reply = WTFMove(reply)](NSArray<PKPaymentSetupFeature *> *features) mutable {
        RunLoop::main().dispatch([reply = WTFMove(reply), features = retainPtr(features)]() mutable {
            reply(PaymentSetupFeatures { WTFMove(features) });
        });
    });

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    [PAL::getPKPaymentSetupControllerClass() paymentSetupFeaturesForConfiguration:configuration.platformConfiguration() completion:completion.get()];
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

void WebPaymentCoordinatorProxy::beginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, CompletionHandler<void(bool)>&& reply)
{
    platformBeginApplePaySetup(configuration, features, WTFMove(reply));
}

void WebPaymentCoordinatorProxy::endApplePaySetup()
{
    platformEndApplePaySetup();
}

#if PLATFORM(MAC)

void WebPaymentCoordinatorProxy::platformBeginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, CompletionHandler<void(bool)>&& reply)
{
    if (!PAL::getPKPaymentSetupRequestClass()) {
        reply(false);
        return;
    }

    auto request = adoptNS([PAL::allocPKPaymentSetupRequestInstance() init]);
    [request setConfiguration:configuration.platformConfiguration()];
    [request setPaymentSetupFeatures:features.platformFeatures()];

    auto completion = makeBlockPtr([reply = WTFMove(reply)](BOOL success) mutable {
        RunLoop::main().dispatch([reply = WTFMove(reply), success]() mutable {
            reply(success);
        });
    });

    auto paymentSetupController = adoptNS([PAL::allocPKPaymentSetupControllerInstance() init]);

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    [paymentSetupController presentPaymentSetupRequest:request.get() completion:completion.get()];
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

void WebPaymentCoordinatorProxy::platformEndApplePaySetup()
{
}

#else // PLATFORM(MAC)

void WebPaymentCoordinatorProxy::platformBeginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, CompletionHandler<void(bool)>&& reply)
{
    UIViewController *presentingViewController = m_client.paymentCoordinatorPresentingViewController(*this);
    if (!presentingViewController) {
        reply(false);
        return;
    }

    auto request = adoptNS([PAL::allocPKPaymentSetupRequestInstance() init]);
    [request setConfiguration:configuration.platformConfiguration()];
    [request setPaymentSetupFeatures:features.platformFeatures()];

    auto paymentSetupViewController = adoptNS([PAL::allocPKPaymentSetupViewControllerInstance() initWithPaymentSetupRequest:request.get()]);
    if (!paymentSetupViewController) {
        reply(false);
        return;
    }

    auto completion = makeBlockPtr([reply = WTFMove(reply)]() mutable {
        RunLoop::main().dispatch([reply = WTFMove(reply)]() mutable {
            reply(true);
        });
    });

    endApplePaySetup();
    [presentingViewController presentViewController:paymentSetupViewController.get() animated:YES completion:completion.get()];
    m_paymentSetupViewController = paymentSetupViewController.get();
}

void WebPaymentCoordinatorProxy::platformEndApplePaySetup()
{
    [m_paymentSetupViewController dismissViewControllerAnimated:YES completion:nil];
    m_paymentSetupViewController = nil;
}

#endif // PLATFORM(MAC)

} // namespace WebKit

#endif // ENABLE(APPLE_PAY)
