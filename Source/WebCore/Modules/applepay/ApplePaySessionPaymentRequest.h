/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY)

#include "PaymentContact.h"
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class PaymentAuthorizationStatus;

class ApplePaySessionPaymentRequest {
public:
    WEBCORE_EXPORT ApplePaySessionPaymentRequest();
    WEBCORE_EXPORT ~ApplePaySessionPaymentRequest();

    unsigned version() const { return m_version; }
    void setVersion(unsigned version) { m_version = version; }

    const String& countryCode() const { return m_countryCode; }
    void setCountryCode(const String& countryCode) { m_countryCode = countryCode; }

    const String& currencyCode() const { return m_currencyCode; }
    void setCurrencyCode(const String& currencyCode) { m_currencyCode = currencyCode; }

    struct ContactFields {
        bool postalAddress { false };
        bool phone { false };
        bool email { false };
        bool name { false };
        bool phoneticName { false };
    };

    const ContactFields& requiredBillingContactFields() const { return m_requiredBillingContactFields; }
    void setRequiredBillingContactFields(const ContactFields& requiredBillingContactFields) { m_requiredBillingContactFields = requiredBillingContactFields; }

    const PaymentContact& billingContact() const { return m_billingContact; }
    void setBillingContact(const PaymentContact& billingContact) { m_billingContact = billingContact; }

    const ContactFields& requiredShippingContactFields() const { return m_requiredShippingContactFields; }
    void setRequiredShippingContactFields(const ContactFields& requiredShippingContactFields) { m_requiredShippingContactFields = requiredShippingContactFields; }

    const PaymentContact& shippingContact() const { return m_shippingContact; }
    void setShippingContact(const PaymentContact& shippingContact) { m_shippingContact = shippingContact; }

    const Vector<String>& supportedNetworks() const { return m_supportedNetworks; }
    void setSupportedNetworks(const Vector<String>& supportedNetworks) { m_supportedNetworks = supportedNetworks; }

    struct MerchantCapabilities {
        bool supports3DS { false };
        bool supportsEMV { false };
        bool supportsCredit { false };
        bool supportsDebit { false };
    };

    const MerchantCapabilities& merchantCapabilities() const { return m_merchantCapabilities; }
    void setMerchantCapabilities(const MerchantCapabilities& merchantCapabilities) { m_merchantCapabilities = merchantCapabilities; }

    struct LineItem {
        enum class Type {
            Pending,
            Final,
        } type { Type::Final };

        String amount;
        String label;
    };

    enum class ShippingType {
        Shipping,
        Delivery,
        StorePickup,
        ServicePickup,
    };
    ShippingType shippingType() const { return m_shippingType; }
    void setShippingType(ShippingType shippingType) { m_shippingType = shippingType; }

    struct ShippingMethod {
        String label;
        String detail;
        String amount;

        String identifier;
    };
    const Vector<ShippingMethod>& shippingMethods() const { return m_shippingMethods; }
    void setShippingMethods(const Vector<ShippingMethod>& shippingMethods) { m_shippingMethods = shippingMethods; }

    const Vector<LineItem>& lineItems() const { return m_lineItems; }
    void setLineItems(const Vector<LineItem>& lineItems) { m_lineItems = lineItems; }

    const LineItem& total() const { return m_total; };
    void setTotal(const LineItem& total) { m_total = total; }

    struct TotalAndLineItems {
        ApplePaySessionPaymentRequest::LineItem total;
        Vector<ApplePaySessionPaymentRequest::LineItem> lineItems;
    };

    const String& applicationData() const { return m_applicationData; }
    void setApplicationData(const String& applicationData) { m_applicationData = applicationData; }

    const Vector<String>& supportedCountries() const { return m_supportedCountries; }
    void setSupportedCountries(Vector<String>&& supportedCountries) { m_supportedCountries = WTFMove(supportedCountries); }

    enum class Requester {
        ApplePayJS,
        PaymentRequest,
    };

    Requester requester() const { return m_requester; }
    void setRequester(Requester requester) { m_requester = requester; }

private:
    unsigned m_version { 0 };

    String m_countryCode;
    String m_currencyCode;

    ContactFields m_requiredBillingContactFields;
    PaymentContact m_billingContact;

    ContactFields m_requiredShippingContactFields;
    PaymentContact m_shippingContact;

    Vector<String> m_supportedNetworks;
    MerchantCapabilities m_merchantCapabilities;

    ShippingType m_shippingType { ShippingType::Shipping };
    Vector<ShippingMethod> m_shippingMethods;

    Vector<LineItem> m_lineItems;
    LineItem m_total;

    String m_applicationData;
    Vector<String> m_supportedCountries;

    Requester m_requester { Requester::ApplePayJS };
};

struct PaymentError {
    enum class Code {
        Unknown,
        ShippingContactInvalid,
        BillingContactInvalid,
        AddressUnserviceable,
    };

    enum class ContactField {
        PhoneNumber,
        EmailAddress,
        Name,
        PhoneticName,
        PostalAddress,
        AddressLines,
        SubLocality,
        Locality,
        PostalCode,
        SubAdministrativeArea,
        AdministrativeArea,
        Country,
        CountryCode,
    };

    Code code;
    String message;
    std::optional<ContactField> contactField;
};

struct PaymentAuthorizationResult {
    PaymentAuthorizationStatus status;
    Vector<PaymentError> errors;
};

struct PaymentMethodUpdate {
    ApplePaySessionPaymentRequest::TotalAndLineItems newTotalAndLineItems;
};

struct ShippingContactUpdate {
    Vector<PaymentError> errors;

    Vector<ApplePaySessionPaymentRequest::ShippingMethod> newShippingMethods;
    ApplePaySessionPaymentRequest::TotalAndLineItems newTotalAndLineItems;
};

struct ShippingMethodUpdate {
    ApplePaySessionPaymentRequest::TotalAndLineItems newTotalAndLineItems;
};

WEBCORE_EXPORT bool isFinalStateResult(const std::optional<PaymentAuthorizationResult>&);

}

namespace WTF {

template<> struct EnumTraits<WebCore::PaymentError::Code> {
    using values = EnumValues<
        WebCore::PaymentError::Code,
        WebCore::PaymentError::Code::Unknown,
        WebCore::PaymentError::Code::ShippingContactInvalid,
        WebCore::PaymentError::Code::BillingContactInvalid,
        WebCore::PaymentError::Code::AddressUnserviceable
    >;
};

template<> struct EnumTraits<WebCore::PaymentError::ContactField> {
    using values = EnumValues<
        WebCore::PaymentError::ContactField,
        WebCore::PaymentError::ContactField::PhoneNumber,
        WebCore::PaymentError::ContactField::EmailAddress,
        WebCore::PaymentError::ContactField::Name,
        WebCore::PaymentError::ContactField::PhoneticName,
        WebCore::PaymentError::ContactField::PostalAddress,
        WebCore::PaymentError::ContactField::AddressLines,
        WebCore::PaymentError::ContactField::SubLocality,
        WebCore::PaymentError::ContactField::Locality,
        WebCore::PaymentError::ContactField::PostalCode,
        WebCore::PaymentError::ContactField::SubAdministrativeArea,
        WebCore::PaymentError::ContactField::AdministrativeArea,
        WebCore::PaymentError::ContactField::Country,
        WebCore::PaymentError::ContactField::CountryCode
    >;
};

}

#endif
