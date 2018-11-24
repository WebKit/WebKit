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

#import "config.h"
#import "WebCoreArgumentCoders.h"

#if ENABLE(APPLE_PAY)

#import "DataReference.h"
#import <WebCore/PaymentAuthorizationStatus.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>

namespace IPC {
using namespace WebCore;

void ArgumentCoder<WebCore::Payment>::encode(Encoder& encoder, const WebCore::Payment& payment)
{
    auto archiver = secureArchiver();
    [archiver encodeObject:payment.pkPayment() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    auto data = archiver.get().encodedData;
    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::Payment>::decode(Decoder& decoder, WebCore::Payment& payment)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = secureUnarchiverFromData(data.get());
    @try {
        PKPayment *pkPayment = [unarchiver decodeObjectOfClass:PAL::getPKPaymentClass() forKey:NSKeyedArchiveRootObjectKey];
        payment = Payment(pkPayment);
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode PKPayment: %@", exception);
        return false;
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<WebCore::PaymentAuthorizationResult>::encode(Encoder& encoder, const WebCore::PaymentAuthorizationResult& result)
{
    encoder << result.status;
    encoder << result.errors;
}

std::optional<WebCore::PaymentAuthorizationResult> ArgumentCoder<WebCore::PaymentAuthorizationResult>::decode(Decoder& decoder)
{
    std::optional<PaymentAuthorizationStatus> status;
    decoder >> status;
    if (!status)
        return std::nullopt;

    std::optional<Vector<PaymentError>> errors;
    decoder >> errors;
    if (!errors)
        return std::nullopt;
    
    return {{ WTFMove(*status), WTFMove(*errors) }};
}

void ArgumentCoder<WebCore::PaymentContact>::encode(Encoder& encoder, const WebCore::PaymentContact& paymentContact)
{
    auto archiver = secureArchiver();
    [archiver encodeObject:paymentContact.pkContact() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    auto data = archiver.get().encodedData;
    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentContact>::decode(Decoder& decoder, WebCore::PaymentContact& paymentContact)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = secureUnarchiverFromData(data.get());
    @try {
        PKContact *pkContact = [unarchiver decodeObjectOfClass:PAL::getPKContactClass() forKey:NSKeyedArchiveRootObjectKey];
        paymentContact = PaymentContact(pkContact);
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode PKContact: %@", exception);
        return false;
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<WebCore::PaymentError>::encode(Encoder& encoder, const WebCore::PaymentError& error)
{
    encoder << error.code;
    encoder << error.message;
    encoder << error.contactField;
}

std::optional<WebCore::PaymentError> ArgumentCoder<WebCore::PaymentError>::decode(Decoder& decoder)
{
    std::optional<WebCore::PaymentError::Code> code;
    decoder >> code;
    if (!code)
        return std::nullopt;
    
    std::optional<String> message;
    decoder >> message;
    if (!message)
        return std::nullopt;
    
    std::optional<std::optional<WebCore::PaymentError::ContactField>> contactField;
    decoder >> contactField;
    if (!contactField)
        return std::nullopt;

    return {{ WTFMove(*code), WTFMove(*message), WTFMove(*contactField) }};
}

void ArgumentCoder<WebCore::PaymentMerchantSession>::encode(Encoder& encoder, const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    auto archiver = secureArchiver();
    [archiver encodeObject:paymentMerchantSession.pkPaymentMerchantSession() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    auto data = archiver.get().encodedData;
    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentMerchantSession>::decode(Decoder& decoder, WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = secureUnarchiverFromData(data.get());
    @try {
        PKPaymentMerchantSession *pkPaymentMerchantSession = [unarchiver decodeObjectOfClass:PAL::getPKPaymentMerchantSessionClass() forKey:NSKeyedArchiveRootObjectKey];
        paymentMerchantSession = PaymentMerchantSession(pkPaymentMerchantSession);
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode PKPaymentMerchantSession: %@", exception);
        return false;
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<WebCore::PaymentMethod>::encode(Encoder& encoder, const WebCore::PaymentMethod& paymentMethod)
{
    auto archiver = secureArchiver();
    [archiver encodeObject:paymentMethod.pkPaymentMethod() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    auto data = archiver.get().encodedData;
    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentMethod>::decode(Decoder& decoder, WebCore::PaymentMethod& paymentMethod)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = secureUnarchiverFromData(data.get());
    @try {
        PKPaymentMethod *pkPaymentMethod = [unarchiver decodeObjectOfClass:PAL::getPKPaymentMethodClass() forKey:NSKeyedArchiveRootObjectKey];
        paymentMethod = PaymentMethod(pkPaymentMethod);
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode PKPayment: %@", exception);
        return false;
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<WebCore::PaymentMethodUpdate>::encode(Encoder& encoder, const WebCore::PaymentMethodUpdate& update)
{
    encoder << update.newTotalAndLineItems;
}

std::optional<WebCore::PaymentMethodUpdate> ArgumentCoder<WebCore::PaymentMethodUpdate>::decode(Decoder& decoder)
{
    std::optional<ApplePaySessionPaymentRequest::TotalAndLineItems> newTotalAndLineItems;
    decoder >> newTotalAndLineItems;
    if (!newTotalAndLineItems)
        return std::nullopt;
    return {{ WTFMove(*newTotalAndLineItems) }};
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
    encoder.encodeEnum(request.shippingType());
    encoder << request.shippingMethods();
    encoder << request.lineItems();
    encoder << request.total();
    encoder << request.applicationData();
    encoder << request.supportedCountries();
    encoder.encodeEnum(request.requester());
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

    PaymentContact billingContact;
    if (!decoder.decode(billingContact))
        return false;
    request.setBillingContact(billingContact);

    ApplePaySessionPaymentRequest::ContactFields requiredShippingContactFields;
    if (!decoder.decode((requiredShippingContactFields)))
        return false;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    PaymentContact shippingContact;
    if (!decoder.decode(shippingContact))
        return false;
    request.setShippingContact(shippingContact);

    ApplePaySessionPaymentRequest::MerchantCapabilities merchantCapabilities;
    if (!decoder.decode(merchantCapabilities))
        return false;
    request.setMerchantCapabilities(merchantCapabilities);

    Vector<String> supportedNetworks;
    if (!decoder.decode(supportedNetworks))
        return false;
    request.setSupportedNetworks(supportedNetworks);

    ApplePaySessionPaymentRequest::ShippingType shippingType;
    if (!decoder.decodeEnum(shippingType))
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

    std::optional<ApplePaySessionPaymentRequest::LineItem> total;
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
    if (!decoder.decodeEnum(requester))
        return false;
    request.setRequester(requester);

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
    encoder.encodeEnum(lineItem.type);
    encoder << lineItem.label;
    encoder << lineItem.amount;
}

std::optional<ApplePaySessionPaymentRequest::LineItem> ArgumentCoder<ApplePaySessionPaymentRequest::LineItem>::decode(Decoder& decoder)
{
    WebCore::ApplePaySessionPaymentRequest::LineItem lineItem;
    if (!decoder.decodeEnum(lineItem.type))
        return std::nullopt;
    if (!decoder.decode(lineItem.label))
        return std::nullopt;
    if (!decoder.decode(lineItem.amount))
        return std::nullopt;

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

std::optional<ApplePaySessionPaymentRequest::ShippingMethod> ArgumentCoder<ApplePaySessionPaymentRequest::ShippingMethod>::decode(Decoder& decoder)
{
    ApplePaySessionPaymentRequest::ShippingMethod shippingMethod;
    if (!decoder.decode(shippingMethod.label))
        return std::nullopt;
    if (!decoder.decode(shippingMethod.detail))
        return std::nullopt;
    if (!decoder.decode(shippingMethod.amount))
        return std::nullopt;
    if (!decoder.decode(shippingMethod.identifier))
        return std::nullopt;
    return WTFMove(shippingMethod);
}

void ArgumentCoder<ApplePaySessionPaymentRequest::TotalAndLineItems>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    encoder << totalAndLineItems.total;
    encoder << totalAndLineItems.lineItems;
}

std::optional<ApplePaySessionPaymentRequest::TotalAndLineItems> ArgumentCoder<ApplePaySessionPaymentRequest::TotalAndLineItems>::decode(Decoder& decoder)
{
    std::optional<ApplePaySessionPaymentRequest::LineItem> total;
    decoder >> total;
    if (!total)
        return std::nullopt;
    
    std::optional<Vector<ApplePaySessionPaymentRequest::LineItem>> lineItems;
    decoder >> lineItems;
    if (!lineItems)
        return std::nullopt;
    
    return {{ WTFMove(*total), WTFMove(*lineItems) }};
}

void ArgumentCoder<WebCore::ShippingContactUpdate>::encode(Encoder& encoder, const WebCore::ShippingContactUpdate& update)
{
    encoder << update.errors;
    encoder << update.newShippingMethods;
    encoder << update.newTotalAndLineItems;
}

std::optional<WebCore::ShippingContactUpdate> ArgumentCoder<WebCore::ShippingContactUpdate>::decode(Decoder& decoder)
{
    std::optional<Vector<PaymentError>> errors;
    decoder >> errors;
    if (!errors)
        return std::nullopt;
    
    std::optional<Vector<ApplePaySessionPaymentRequest::ShippingMethod>> newShippingMethods;
    decoder >> newShippingMethods;
    if (!newShippingMethods)
        return std::nullopt;
    
    std::optional<ApplePaySessionPaymentRequest::TotalAndLineItems> newTotalAndLineItems;
    decoder >> newTotalAndLineItems;
    if (!newTotalAndLineItems)
        return std::nullopt;
    
    return {{ WTFMove(*errors), WTFMove(*newShippingMethods), WTFMove(*newTotalAndLineItems) }};
}

void ArgumentCoder<WebCore::ShippingMethodUpdate>::encode(Encoder& encoder, const WebCore::ShippingMethodUpdate& update)
{
    encoder << update.newTotalAndLineItems;
}

std::optional<WebCore::ShippingMethodUpdate> ArgumentCoder<WebCore::ShippingMethodUpdate>::decode(Decoder& decoder)
{
    std::optional<ApplePaySessionPaymentRequest::TotalAndLineItems> newTotalAndLineItems;
    decoder >> newTotalAndLineItems;
    if (!newTotalAndLineItems)
        return std::nullopt;
    return {{ WTFMove(*newTotalAndLineItems) }};
}

}
#endif
