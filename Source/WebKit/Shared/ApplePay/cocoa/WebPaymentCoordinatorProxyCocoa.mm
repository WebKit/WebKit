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
#import "PaymentSetupConfigurationWebKit.h"
#import "WKPaymentAuthorizationDelegate.h"
#import "WebPageProxy.h"
#import "WebPaymentCoordinatorProxy.h"
#import "WebPaymentCoordinatorProxyMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/PaymentAuthorizationStatus.h>
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

static void finishCreating(PKPaymentRequest *platformRequest, const WebCore::ApplePaySessionPaymentRequest& request)
{
#if HAVE(PASSKIT_INSTALLMENTS)
    if (PKPaymentInstallmentConfiguration *configuration = request.installmentConfiguration().platformConfiguration()) {
        platformRequest.installmentConfiguration = configuration;
        platformRequest.requestType = PKPaymentRequestTypeInstallment;
    }
#else
    UNUSED_PARAM(platformRequest);
    UNUSED_PARAM(request);
#endif
}

WebPaymentCoordinatorProxy::WebPaymentCoordinatorProxy(WebPaymentCoordinatorProxy::Client& client)
    : m_client(client)
    , m_canMakePaymentsQueue(WorkQueue::create("com.apple.WebKit.CanMakePayments"))
{
    m_client.paymentCoordinatorAddMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName(), *this);
}

WebPaymentCoordinatorProxy::~WebPaymentCoordinatorProxy()
{
    if (m_state != State::Idle)
        hidePaymentUI();

    m_client.paymentCoordinatorRemoveMessageReceiver(*this, Messages::WebPaymentCoordinatorProxy::messageReceiverName());
}

void WebPaymentCoordinatorProxy::platformCanMakePaymentsWithActiveCard(const String& merchantIdentifier, const String& domainName, WTF::Function<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    if (!PAL::isPassKitFrameworkAvailable())
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

PKShippingMethod *toPKShippingMethod(const WebCore::ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    PKShippingMethod *result = [PAL::getPKShippingMethodClass() summaryItemWithLabel:shippingMethod.label amount:toDecimalNumber(shippingMethod.amount)];
    [result setIdentifier:shippingMethod.identifier];
    [result setDetail:shippingMethod.detail];

    return result;
}
    
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

    [result setShippingMethods:createNSArray(paymentRequest.shippingMethods(), [] (auto& method) {
        return toPKShippingMethod(method);
    }).get()];

    [result setPaymentSummaryItems:WebCore::platformSummaryItems(paymentRequest.total(), paymentRequest.lineItems())];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [result setExpectsMerchantSession:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!paymentRequest.applicationData().isNull()) {
        auto applicationData = adoptNS([[NSData alloc] initWithBase64EncodedString:paymentRequest.applicationData() options:0]);
        [result setApplicationData:applicationData.get()];
    }

    [result setSupportedCountries:toNSSet(paymentRequest.supportedCountries()).get()];

#if HAVE(PASSKIT_BOUND_INTERFACE_IDENTIFIER)
    auto& boundInterfaceIdentifier = m_client.paymentCoordinatorBoundInterfaceIdentifier(*this);
    if (!boundInterfaceIdentifier.isEmpty())
        [result setBoundInterfaceIdentifier:boundInterfaceIdentifier];
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

    finishCreating(result.get(), paymentRequest);

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

void WebPaymentCoordinatorProxy::getSetupFeatures(const PaymentSetupConfiguration& configuration, Messages::WebPaymentCoordinatorProxy::GetSetupFeatures::AsyncReply&& reply)
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

void WebPaymentCoordinatorProxy::beginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, Messages::WebPaymentCoordinatorProxy::BeginApplePaySetup::AsyncReply&& reply)
{
    platformBeginApplePaySetup(configuration, features, WTFMove(reply));
}

void WebPaymentCoordinatorProxy::endApplePaySetup()
{
    platformEndApplePaySetup();
}

#if PLATFORM(MAC)

void WebPaymentCoordinatorProxy::platformBeginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, Messages::WebPaymentCoordinatorProxy::BeginApplePaySetup::AsyncReply&& reply)
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

void WebPaymentCoordinatorProxy::platformBeginApplePaySetup(const PaymentSetupConfiguration& configuration, const PaymentSetupFeatures& features, Messages::WebPaymentCoordinatorProxy::BeginApplePaySetup::AsyncReply&& reply)
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
