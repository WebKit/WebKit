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

#import "config.h"
#import "WebCoreArgumentCoders.h"

#if ENABLE(APPLE_PAY)

#import "DataReference.h"
#import <WebCore/PassKitSPI.h>
#import <WebCore/PaymentAuthorizationStatus.h>
#import <WebCore/SoftLinking.h>

#if PLATFORM(MAC)
SOFT_LINK_PRIVATE_FRAMEWORK(PassKit)
#else
SOFT_LINK_FRAMEWORK(PassKit)
#endif

SOFT_LINK_CLASS(PassKit, PKContact);
SOFT_LINK_CLASS(PassKit, PKPayment);
SOFT_LINK_CLASS(PassKit, PKPaymentMethod);
SOFT_LINK_CLASS(PassKit, PKPaymentMerchantSession);

using namespace WebCore;

namespace IPC {

void ArgumentCoder<WebCore::Payment>::encode(Encoder& encoder, const WebCore::Payment& payment)
{
    auto data = adoptNS([[NSMutableData alloc] init]);
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

    [archiver setRequiresSecureCoding:YES];

    [archiver encodeObject:payment.pkPayment() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::Payment>::decode(Decoder& decoder, WebCore::Payment& payment)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        PKPayment *pkPayment = [unarchiver decodeObjectOfClass:getPKPaymentClass() forKey:NSKeyedArchiveRootObjectKey];
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

bool ArgumentCoder<WebCore::PaymentAuthorizationResult>::decode(Decoder& decoder, WebCore::PaymentAuthorizationResult& result)
{
    if (!decoder.decode(result.status))
        return false;
    if (!decoder.decode(result.errors))
        return false;

    return true;
}

void ArgumentCoder<WebCore::PaymentContact>::encode(Encoder& encoder, const WebCore::PaymentContact& paymentContact)
{
    auto data = adoptNS([[NSMutableData alloc] init]);
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

    [archiver setRequiresSecureCoding:YES];

    [archiver encodeObject:paymentContact.pkContact() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentContact>::decode(Decoder& decoder, WebCore::PaymentContact& paymentContact)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        PKContact *pkContact = [unarchiver decodeObjectOfClass:getPKContactClass() forKey:NSKeyedArchiveRootObjectKey];
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

bool ArgumentCoder<WebCore::PaymentError>::decode(Decoder& decoder, WebCore::PaymentError& error)
{
    if (!decoder.decode(error.code))
        return false;
    if (!decoder.decode(error.message))
        return false;
    if (!decoder.decode(error.contactField))
        return false;

    return true;
}

void ArgumentCoder<WebCore::PaymentMerchantSession>::encode(Encoder& encoder, const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    auto data = adoptNS([[NSMutableData alloc] init]);
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

    [archiver setRequiresSecureCoding:YES];

    [archiver encodeObject:paymentMerchantSession.pkPaymentMerchantSession() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentMerchantSession>::decode(Decoder& decoder, WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        PKPaymentMerchantSession *pkPaymentMerchantSession = [unarchiver decodeObjectOfClass:getPKPaymentMerchantSessionClass() forKey:NSKeyedArchiveRootObjectKey];
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
    auto data = adoptNS([[NSMutableData alloc] init]);
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);

    [archiver setRequiresSecureCoding:YES];

    [archiver encodeObject:paymentMethod.pkPaymentMethod() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    encoder << DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool ArgumentCoder<WebCore::PaymentMethod>::decode(Decoder& decoder, WebCore::PaymentMethod& paymentMethod)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(dataReference.data())) length:dataReference.size() freeWhenDone:NO]);
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        PKPaymentMethod *pkPaymentMethod = [unarchiver decodeObjectOfClass:getPKPaymentMethodClass() forKey:NSKeyedArchiveRootObjectKey];
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
    encoder << update.status;
    encoder << update.newTotalAndLineItems;
}

bool ArgumentCoder<WebCore::PaymentMethodUpdate>::decode(Decoder& decoder, WebCore::PaymentMethodUpdate& update)
{
    if (!decoder.decode(update.status))
        return false;
    if (!decoder.decode(update.newTotalAndLineItems))
        return false;

    return true;
}

void ArgumentCoder<PaymentRequest>::encode(Encoder& encoder, const PaymentRequest& request)
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
}

bool ArgumentCoder<PaymentRequest>::decode(Decoder& decoder, PaymentRequest& request)
{
    String countryCode;
    if (!decoder.decode(countryCode))
        return false;
    request.setCountryCode(countryCode);

    String currencyCode;
    if (!decoder.decode(currencyCode))
        return false;
    request.setCurrencyCode(currencyCode);

    PaymentRequest::ContactFields requiredBillingContactFields;
    if (!decoder.decode((requiredBillingContactFields)))
        return false;
    request.setRequiredBillingContactFields(requiredBillingContactFields);

    PaymentContact billingContact;
    if (!decoder.decode(billingContact))
        return false;
    request.setBillingContact(billingContact);

    PaymentRequest::ContactFields requiredShippingContactFields;
    if (!decoder.decode((requiredShippingContactFields)))
        return false;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    PaymentContact shippingContact;
    if (!decoder.decode(shippingContact))
        return false;
    request.setShippingContact(shippingContact);

    PaymentRequest::MerchantCapabilities merchantCapabilities;
    if (!decoder.decode(merchantCapabilities))
        return false;
    request.setMerchantCapabilities(merchantCapabilities);

    Vector<String> supportedNetworks;
    if (!decoder.decode(supportedNetworks))
        return false;
    request.setSupportedNetworks(supportedNetworks);

    PaymentRequest::ShippingType shippingType;
    if (!decoder.decodeEnum(shippingType))
        return false;
    request.setShippingType(shippingType);

    Vector<PaymentRequest::ShippingMethod> shippingMethods;
    if (!decoder.decode(shippingMethods))
        return false;
    request.setShippingMethods(shippingMethods);

    Vector<PaymentRequest::LineItem> lineItems;
    if (!decoder.decode(lineItems))
        return false;
    request.setLineItems(lineItems);

    PaymentRequest::LineItem total;
    if (!decoder.decode(total))
        return false;
    request.setTotal(total);

    String applicationData;
    if (!decoder.decode(applicationData))
        return false;
    request.setApplicationData(applicationData);

    return true;
}

void ArgumentCoder<PaymentRequest::ContactFields>::encode(Encoder& encoder, const PaymentRequest::ContactFields& contactFields)
{
    encoder << contactFields.postalAddress;
    encoder << contactFields.phone;
    encoder << contactFields.email;
    encoder << contactFields.name;
}

bool ArgumentCoder<PaymentRequest::ContactFields>::decode(Decoder& decoder, PaymentRequest::ContactFields& contactFields)
{
    if (!decoder.decode(contactFields.postalAddress))
        return false;
    if (!decoder.decode(contactFields.phone))
        return false;
    if (!decoder.decode(contactFields.email))
        return false;
    if (!decoder.decode(contactFields.name))
        return false;

    return true;
}

void ArgumentCoder<PaymentRequest::LineItem>::encode(Encoder& encoder, const PaymentRequest::LineItem& lineItem)
{
    encoder.encodeEnum(lineItem.type);
    encoder << lineItem.label;
    encoder << lineItem.amount;
}

bool ArgumentCoder<PaymentRequest::LineItem>::decode(Decoder& decoder, PaymentRequest::LineItem& lineItem)
{
    if (!decoder.decodeEnum(lineItem.type))
        return false;
    if (!decoder.decode(lineItem.label))
        return false;
    if (!decoder.decode(lineItem.amount))
        return false;

    return true;
}

void ArgumentCoder<PaymentRequest::MerchantCapabilities>::encode(Encoder& encoder, const PaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    encoder << merchantCapabilities.supports3DS;
    encoder << merchantCapabilities.supportsEMV;
    encoder << merchantCapabilities.supportsCredit;
    encoder << merchantCapabilities.supportsDebit;
}

bool ArgumentCoder<PaymentRequest::MerchantCapabilities>::decode(Decoder& decoder, PaymentRequest::MerchantCapabilities& merchantCapabilities)
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

void ArgumentCoder<PaymentRequest::ShippingMethod>::encode(Encoder& encoder, const PaymentRequest::ShippingMethod& shippingMethod)
{
    encoder << shippingMethod.label;
    encoder << shippingMethod.detail;
    encoder << shippingMethod.amount;
    encoder << shippingMethod.identifier;
}

bool ArgumentCoder<PaymentRequest::ShippingMethod>::decode(Decoder& decoder, PaymentRequest::ShippingMethod& shippingMethod)
{
    if (!decoder.decode(shippingMethod.label))
        return false;
    if (!decoder.decode(shippingMethod.detail))
        return false;
    if (!decoder.decode(shippingMethod.amount))
        return false;
    if (!decoder.decode(shippingMethod.identifier))
        return false;
    return true;
}

void ArgumentCoder<PaymentRequest::TotalAndLineItems>::encode(Encoder& encoder, const PaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    encoder << totalAndLineItems.total;
    encoder << totalAndLineItems.lineItems;
}

bool ArgumentCoder<PaymentRequest::TotalAndLineItems>::decode(Decoder& decoder, PaymentRequest::TotalAndLineItems& totalAndLineItems)
{
    if (!decoder.decode(totalAndLineItems.total))
        return false;
    if (!decoder.decode(totalAndLineItems.lineItems))
        return false;
    return true;
}

void ArgumentCoder<WebCore::ShippingContactUpdate>::encode(Encoder& encoder, const WebCore::ShippingContactUpdate& update)
{
    encoder << update.status;
    encoder << update.errors;
    encoder << update.newShippingMethods;
    encoder << update.newTotalAndLineItems;
}

bool ArgumentCoder<WebCore::ShippingContactUpdate>::decode(Decoder& decoder, WebCore::ShippingContactUpdate& update)
{
    if (!decoder.decode(update.status))
        return false;
    if (!decoder.decode(update.errors))
        return false;
    if (!decoder.decode(update.newShippingMethods))
        return false;
    if (!decoder.decode(update.newTotalAndLineItems))
        return false;

    return true;
}

void ArgumentCoder<WebCore::ShippingMethodUpdate>::encode(Encoder& encoder, const WebCore::ShippingMethodUpdate& update)
{
    encoder << update.status;
    encoder << update.newTotalAndLineItems;
}

bool ArgumentCoder<WebCore::ShippingMethodUpdate>::decode(Decoder& decoder, WebCore::ShippingMethodUpdate& update)
{
    if (!decoder.decode(update.status))
        return false;
    if (!decoder.decode(update.newTotalAndLineItems))
        return false;

    return true;
}

}

#endif
