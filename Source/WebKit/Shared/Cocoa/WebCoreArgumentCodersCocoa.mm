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
#import "WebCoreArgumentCoders.h"

#import "ArgumentCodersCocoa.h"
#import "CocoaFont.h"
#import <WebCore/AttributedString.h>
#import <WebCore/DictionaryPopupInfo.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributes.h>

#if ENABLE(APPLE_PAY)
#import "DataReference.h"
#import <WebCore/PaymentAuthorizationStatus.h>
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIFont.h>
#endif

namespace IPC {
using namespace WebCore;

void ArgumentCoder<WebCore::AttributedString>::encode(Encoder& encoder, const WebCore::AttributedString& attributedString)
{
    encoder << attributedString.string << attributedString.documentAttributes;
}

Optional<WebCore::AttributedString> ArgumentCoder<WebCore::AttributedString>::decode(Decoder& decoder)
{
    RetainPtr<NSAttributedString> attributedString;
    if (!IPC::decode(decoder, attributedString))
        return WTF::nullopt;
    RetainPtr<NSDictionary> documentAttributes;
    if (!IPC::decode(decoder, documentAttributes))
        return WTF::nullopt;
    return { { WTFMove(attributedString), WTFMove(documentAttributes) } };
}

#if ENABLE(APPLE_PAY)

#if HAVE(PASSKIT_INSTALLMENTS)

void ArgumentCoder<WebCore::PaymentInstallmentConfiguration>::encode(Encoder& encoder, const WebCore::PaymentInstallmentConfiguration& configuration)
{
    encoder << configuration.platformConfiguration();
}

Optional<WebCore::PaymentInstallmentConfiguration> ArgumentCoder<WebCore::PaymentInstallmentConfiguration>::decode(Decoder& decoder)
{
    auto configuration = IPC::decode<PKPaymentInstallmentConfiguration>(decoder, PAL::getPKPaymentInstallmentConfigurationClass());
    if (!configuration)
        return WTF::nullopt;

    return { WTFMove(*configuration) };
}

#endif // HAVE(PASSKIT_INSTALLMENTS)

void ArgumentCoder<WebCore::Payment>::encode(Encoder& encoder, const WebCore::Payment& payment)
{
    encoder << payment.pkPayment();
}

Optional<WebCore::Payment> ArgumentCoder<WebCore::Payment>::decode(Decoder& decoder)
{
    auto payment = IPC::decode<PKPayment>(decoder, PAL::getPKPaymentClass());
    if (!payment)
        return WTF::nullopt;

    return Payment { WTFMove(*payment) };
}

void ArgumentCoder<WebCore::PaymentAuthorizationResult>::encode(Encoder& encoder, const WebCore::PaymentAuthorizationResult& result)
{
    encoder << result.status;
    encoder << result.errors;
}

Optional<WebCore::PaymentAuthorizationResult> ArgumentCoder<WebCore::PaymentAuthorizationResult>::decode(Decoder& decoder)
{
    Optional<PaymentAuthorizationStatus> status;
    decoder >> status;
    if (!status)
        return WTF::nullopt;

    Optional<Vector<PaymentError>> errors;
    decoder >> errors;
    if (!errors)
        return WTF::nullopt;

    return {{ WTFMove(*status), WTFMove(*errors) }};
}

void ArgumentCoder<WebCore::PaymentContact>::encode(Encoder& encoder, const WebCore::PaymentContact& paymentContact)
{
    encoder << paymentContact.pkContact();
}

Optional<WebCore::PaymentContact> ArgumentCoder<WebCore::PaymentContact>::decode(Decoder& decoder)
{
    auto contact = IPC::decode<PKContact>(decoder, PAL::getPKContactClass());
    if (!contact)
        return WTF::nullopt;

    return WebCore::PaymentContact { WTFMove(*contact) };
}

void ArgumentCoder<WebCore::PaymentError>::encode(Encoder& encoder, const WebCore::PaymentError& error)
{
    encoder << error.code;
    encoder << error.message;
    encoder << error.contactField;
}

Optional<WebCore::PaymentError> ArgumentCoder<WebCore::PaymentError>::decode(Decoder& decoder)
{
    Optional<WebCore::PaymentError::Code> code;
    decoder >> code;
    if (!code)
        return WTF::nullopt;
    
    Optional<String> message;
    decoder >> message;
    if (!message)
        return WTF::nullopt;
    
    Optional<Optional<WebCore::PaymentError::ContactField>> contactField;
    decoder >> contactField;
    if (!contactField)
        return WTF::nullopt;

    return {{ WTFMove(*code), WTFMove(*message), WTFMove(*contactField) }};
}

void ArgumentCoder<WebCore::PaymentMerchantSession>::encode(Encoder& encoder, const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    encoder << paymentMerchantSession.pkPaymentMerchantSession();
}

Optional<WebCore::PaymentMerchantSession> ArgumentCoder<WebCore::PaymentMerchantSession>::decode(Decoder& decoder)
{
    auto paymentMerchantSession = IPC::decode<PKPaymentMerchantSession>(decoder, PAL::getPKPaymentMerchantSessionClass());
    if (!paymentMerchantSession)
        return WTF::nullopt;

    return WebCore::PaymentMerchantSession { WTFMove(*paymentMerchantSession) };
}

void ArgumentCoder<WebCore::PaymentMethod>::encode(Encoder& encoder, const WebCore::PaymentMethod& paymentMethod)
{
    encoder << paymentMethod.pkPaymentMethod();
}

Optional<WebCore::PaymentMethod> ArgumentCoder<WebCore::PaymentMethod>::decode(Decoder& decoder)
{
    auto paymentMethod = IPC::decode<PKPaymentMethod>(decoder, PAL::getPKPaymentMethodClass());
    if (!paymentMethod)
        return WTF::nullopt;

    return PaymentMethod { WTFMove(*paymentMethod) };
}

void ArgumentCoder<WebCore::PaymentMethodUpdate>::encode(Encoder& encoder, const WebCore::PaymentMethodUpdate& update)
{
    encoder << update.platformUpdate();
}

Optional<WebCore::PaymentMethodUpdate> ArgumentCoder<WebCore::PaymentMethodUpdate>::decode(Decoder& decoder)
{
    auto update = IPC::decode<PKPaymentRequestPaymentMethodUpdate>(decoder, PAL::getPKPaymentRequestPaymentMethodUpdateClass());
    if (!update)
        return WTF::nullopt;

    return PaymentMethodUpdate { WTFMove(*update) };
}

void ArgumentCoder<ApplePaySessionPaymentRequest>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest& request)
{
    encoder << request.countryCode();
    encoder << request.currencyCode();
    encoder << request.requiredBillingContactFields();
    encoder << request.billingContact();
    encoder << request.requiredShippingContactFields();
    encoder << request.shippingContact();
    encoder << request.merchantCapabilities();
    encoder << request.supportedNetworks();
    encoder << request.shippingType();
    encoder << request.shippingMethods();
    encoder << request.lineItems();
    encoder << request.total();
    encoder << request.applicationData();
    encoder << request.supportedCountries();
    encoder << request.requester();
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    encoder << request.installmentConfiguration();
#endif
}

bool ArgumentCoder<ApplePaySessionPaymentRequest>::decode(Decoder& decoder, ApplePaySessionPaymentRequest& request)
{
    String countryCode;
    if (!decoder.decode(countryCode))
        return false;
    request.setCountryCode(countryCode);

    String currencyCode;
    if (!decoder.decode(currencyCode))
        return false;
    request.setCurrencyCode(currencyCode);

    ApplePaySessionPaymentRequest::ContactFields requiredBillingContactFields;
    if (!decoder.decode((requiredBillingContactFields)))
        return false;
    request.setRequiredBillingContactFields(requiredBillingContactFields);

    Optional<PaymentContact> billingContact;
    decoder >> billingContact;
    if (!billingContact)
        return false;
    request.setBillingContact(*billingContact);

    ApplePaySessionPaymentRequest::ContactFields requiredShippingContactFields;
    if (!decoder.decode((requiredShippingContactFields)))
        return false;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    Optional<PaymentContact> shippingContact;
    decoder >> shippingContact;
    if (!shippingContact)
        return false;
    request.setShippingContact(*shippingContact);

    ApplePaySessionPaymentRequest::MerchantCapabilities merchantCapabilities;
    if (!decoder.decode(merchantCapabilities))
        return false;
    request.setMerchantCapabilities(merchantCapabilities);

    Vector<String> supportedNetworks;
    if (!decoder.decode(supportedNetworks))
        return false;
    request.setSupportedNetworks(supportedNetworks);

    ApplePaySessionPaymentRequest::ShippingType shippingType;
    if (!decoder.decode(shippingType))
        return false;
    request.setShippingType(shippingType);

    Vector<ApplePaySessionPaymentRequest::ShippingMethod> shippingMethods;
    if (!decoder.decode(shippingMethods))
        return false;
    request.setShippingMethods(shippingMethods);

    Vector<ApplePaySessionPaymentRequest::LineItem> lineItems;
    if (!decoder.decode(lineItems))
        return false;
    request.setLineItems(lineItems);

    Optional<ApplePaySessionPaymentRequest::LineItem> total;
    decoder >> total;
    if (!total)
        return false;
    request.setTotal(*total);

    String applicationData;
    if (!decoder.decode(applicationData))
        return false;
    request.setApplicationData(applicationData);

    Vector<String> supportedCountries;
    if (!decoder.decode(supportedCountries))
        return false;
    request.setSupportedCountries(WTFMove(supportedCountries));

    ApplePaySessionPaymentRequest::Requester requester;
    if (!decoder.decode(requester))
        return false;
    request.setRequester(requester);
    
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    Optional<WebCore::PaymentInstallmentConfiguration> installmentConfiguration;
    decoder >> installmentConfiguration;
    if (!installmentConfiguration)
        return false;

    request.setInstallmentConfiguration(WTFMove(*installmentConfiguration));
#endif

    return true;
}

void ArgumentCoder<ApplePaySessionPaymentRequest::ContactFields>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    encoder << contactFields.postalAddress;
    encoder << contactFields.phone;
    encoder << contactFields.email;
    encoder << contactFields.name;
    encoder << contactFields.phoneticName;
}

bool ArgumentCoder<ApplePaySessionPaymentRequest::ContactFields>::decode(Decoder& decoder, ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    if (!decoder.decode(contactFields.postalAddress))
        return false;
    if (!decoder.decode(contactFields.phone))
        return false;
    if (!decoder.decode(contactFields.email))
        return false;
    if (!decoder.decode(contactFields.name))
        return false;
    if (!decoder.decode(contactFields.phoneticName))
        return false;

    return true;
}

void ArgumentCoder<ApplePaySessionPaymentRequest::LineItem>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::LineItem& lineItem)
{
    encoder << lineItem.type;
    encoder << lineItem.label;
    encoder << lineItem.amount;
}

Optional<ApplePaySessionPaymentRequest::LineItem> ArgumentCoder<ApplePaySessionPaymentRequest::LineItem>::decode(Decoder& decoder)
{
    WebCore::ApplePaySessionPaymentRequest::LineItem lineItem;
    if (!decoder.decode(lineItem.type))
        return WTF::nullopt;
    if (!decoder.decode(lineItem.label))
        return WTF::nullopt;
    if (!decoder.decode(lineItem.amount))
        return WTF::nullopt;

    return WTFMove(lineItem);
}

void ArgumentCoder<ApplePaySessionPaymentRequest::MerchantCapabilities>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    encoder << merchantCapabilities.supports3DS;
    encoder << merchantCapabilities.supportsEMV;
    encoder << merchantCapabilities.supportsCredit;
    encoder << merchantCapabilities.supportsDebit;
}

bool ArgumentCoder<ApplePaySessionPaymentRequest::MerchantCapabilities>::decode(Decoder& decoder, ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    if (!decoder.decode(merchantCapabilities.supports3DS))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsEMV))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsCredit))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsDebit))
        return false;

    return true;
}

void ArgumentCoder<ApplePaySessionPaymentRequest::ShippingMethod>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::ShippingMethod& shippingMethod)
{
    encoder << shippingMethod.label;
    encoder << shippingMethod.detail;
    encoder << shippingMethod.amount;
    encoder << shippingMethod.identifier;
}

Optional<ApplePaySessionPaymentRequest::ShippingMethod> ArgumentCoder<ApplePaySessionPaymentRequest::ShippingMethod>::decode(Decoder& decoder)
{
    ApplePaySessionPaymentRequest::ShippingMethod shippingMethod;
    if (!decoder.decode(shippingMethod.label))
        return WTF::nullopt;
    if (!decoder.decode(shippingMethod.detail))
        return WTF::nullopt;
    if (!decoder.decode(shippingMethod.amount))
        return WTF::nullopt;
    if (!decoder.decode(shippingMethod.identifier))
        return WTF::nullopt;
    return WTFMove(shippingMethod);
}

void ArgumentCoder<ApplePaySessionPaymentRequest::TotalAndLineItems>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    encoder << totalAndLineItems.total;
    encoder << totalAndLineItems.lineItems;
}

Optional<ApplePaySessionPaymentRequest::TotalAndLineItems> ArgumentCoder<ApplePaySessionPaymentRequest::TotalAndLineItems>::decode(Decoder& decoder)
{
    Optional<ApplePaySessionPaymentRequest::LineItem> total;
    decoder >> total;
    if (!total)
        return WTF::nullopt;
    
    Optional<Vector<ApplePaySessionPaymentRequest::LineItem>> lineItems;
    decoder >> lineItems;
    if (!lineItems)
        return WTF::nullopt;
    
    return {{ WTFMove(*total), WTFMove(*lineItems) }};
}

void ArgumentCoder<WebCore::ShippingContactUpdate>::encode(Encoder& encoder, const WebCore::ShippingContactUpdate& update)
{
    encoder << update.errors;
    encoder << update.newShippingMethods;
    encoder << update.newTotalAndLineItems;
}

Optional<WebCore::ShippingContactUpdate> ArgumentCoder<WebCore::ShippingContactUpdate>::decode(Decoder& decoder)
{
    Optional<Vector<PaymentError>> errors;
    decoder >> errors;
    if (!errors)
        return WTF::nullopt;
    
    Optional<Vector<ApplePaySessionPaymentRequest::ShippingMethod>> newShippingMethods;
    decoder >> newShippingMethods;
    if (!newShippingMethods)
        return WTF::nullopt;
    
    Optional<ApplePaySessionPaymentRequest::TotalAndLineItems> newTotalAndLineItems;
    decoder >> newTotalAndLineItems;
    if (!newTotalAndLineItems)
        return WTF::nullopt;
    
    return {{ WTFMove(*errors), WTFMove(*newShippingMethods), WTFMove(*newTotalAndLineItems) }};
}

void ArgumentCoder<WebCore::ShippingMethodUpdate>::encode(Encoder& encoder, const WebCore::ShippingMethodUpdate& update)
{
    encoder << update.newTotalAndLineItems;
}

Optional<WebCore::ShippingMethodUpdate> ArgumentCoder<WebCore::ShippingMethodUpdate>::decode(Decoder& decoder)
{
    Optional<ApplePaySessionPaymentRequest::TotalAndLineItems> newTotalAndLineItems;
    decoder >> newTotalAndLineItems;
    if (!newTotalAndLineItems)
        return WTF::nullopt;
    return {{ WTFMove(*newTotalAndLineItems) }};
}

void ArgumentCoder<WebCore::PaymentSessionError>::encode(Encoder& encoder, const WebCore::PaymentSessionError& error)
{
    encoder << error.platformError();
}

Optional<WebCore::PaymentSessionError> ArgumentCoder<WebCore::PaymentSessionError>::decode(Decoder& decoder)
{
    auto platformError = IPC::decode<NSError>(decoder);
    if (!platformError)
        return WTF::nullopt;

    return { WTFMove(*platformError) };
}

#endif // ENABLE(APPLEPAY)

void ArgumentCoder<WebCore::DictionaryPopupInfo>::encodePlatformData(Encoder& encoder, const WebCore::DictionaryPopupInfo& info)
{
    encoder << info.options << info.attributedString;
}

bool ArgumentCoder<WebCore::DictionaryPopupInfo>::decodePlatformData(Decoder& decoder, WebCore::DictionaryPopupInfo& result)
{
    if (!IPC::decode(decoder, result.options))
        return false;
    if (!IPC::decode(decoder, result.attributedString))
        return false;
    return true;
}

void ArgumentCoder<WebCore::FontAttributes>::encodePlatformData(Encoder& encoder, const WebCore::FontAttributes& attributes)
{
    encoder << attributes.font;
}

Optional<FontAttributes> ArgumentCoder<WebCore::FontAttributes>::decodePlatformData(Decoder& decoder, WebCore::FontAttributes& attributes)
{
    if (!IPC::decode(decoder, attributes.font))
        return WTF::nullopt;
    return attributes;
}

void ArgumentCoder<FontHandle>::encodePlatformData(Encoder& encoder, const FontHandle& handle)
{
    auto ctFont = handle.font && !handle.font->fontFaceData() ? handle.font->getCTFont() : nil;
    encoder << !!ctFont;
    if (ctFont)
        encoder << (__bridge CocoaFont *)ctFont;
}

bool ArgumentCoder<FontHandle>::decodePlatformData(Decoder& decoder, FontHandle& handle)
{
    bool hasPlatformFont;
    if (!decoder.decode(hasPlatformFont))
        return false;

    if (!hasPlatformFont)
        return true;

    RetainPtr<CocoaFont> font;
    if (!IPC::decode(decoder, font))
        return false;

    handle.font = Font::create({ (__bridge CTFontRef)font.get(), static_cast<float>([font pointSize]) });
    return true;
}

} // namespace IPC
