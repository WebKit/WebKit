/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "ApplePayError.h"
#include "ApplePayLineItem.h"
#include "ApplePayShippingContactEditingMode.h"
#include "ApplePayShippingMethod.h"
#include "PaymentContact.h"
#include "PaymentInstallmentConfigurationWebCore.h"
#include <wtf/EnumTraits.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/ApplePaySessionPaymentRequestAdditions.h>
#endif

namespace WebCore {

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

    enum class ShippingType {
        Shipping,
        Delivery,
        StorePickup,
        ServicePickup,
    };
    ShippingType shippingType() const { return m_shippingType; }
    void setShippingType(ShippingType shippingType) { m_shippingType = shippingType; }

    const Vector<ApplePayShippingMethod>& shippingMethods() const { return m_shippingMethods; }
    void setShippingMethods(const Vector<ApplePayShippingMethod>& shippingMethods) { m_shippingMethods = shippingMethods; }

    const Vector<ApplePayLineItem>& lineItems() const { return m_lineItems; }
    void setLineItems(const Vector<ApplePayLineItem>& lineItems) { m_lineItems = lineItems; }

    const ApplePayLineItem& total() const { return m_total; };
    void setTotal(const ApplePayLineItem& total) { m_total = total; }

    const String& applicationData() const { return m_applicationData; }
    void setApplicationData(const String& applicationData) { m_applicationData = applicationData; }

    const Vector<String>& supportedCountries() const { return m_supportedCountries; }
    void setSupportedCountries(Vector<String>&& supportedCountries) { m_supportedCountries = WTFMove(supportedCountries); }

    enum class Requester : bool {
        ApplePayJS,
        PaymentRequest,
    };

    Requester requester() const { return m_requester; }
    void setRequester(Requester requester) { m_requester = requester; }

#if HAVE(PASSKIT_INSTALLMENTS)
    const PaymentInstallmentConfiguration& installmentConfiguration() const { return m_installmentConfiguration; }
    void setInstallmentConfiguration(PaymentInstallmentConfiguration&& installmentConfiguration) { m_installmentConfiguration = WTFMove(installmentConfiguration); }
#endif

#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<bool> supportsCouponCode() const { return m_supportsCouponCode; }
    void setSupportsCouponCode(std::optional<bool> supportsCouponCode) { m_supportsCouponCode = supportsCouponCode; }

    const String& couponCode() const { return m_couponCode; }
    void setCouponCode(const String& couponCode) { m_couponCode = couponCode; }
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    const std::optional<ApplePayShippingContactEditingMode>& shippingContactEditingMode() const { return m_shippingContactEditingMode; }
    void setShippingContactEditingMode(const std::optional<ApplePayShippingContactEditingMode>& shippingContactEditingMode) { m_shippingContactEditingMode = shippingContactEditingMode; }
#endif

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
    Vector<ApplePayShippingMethod> m_shippingMethods;

    Vector<ApplePayLineItem> m_lineItems;
    ApplePayLineItem m_total;

    String m_applicationData;
    Vector<String> m_supportedCountries;

    Requester m_requester { Requester::ApplePayJS };

#if HAVE(PASSKIT_INSTALLMENTS)
    PaymentInstallmentConfiguration m_installmentConfiguration;
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    std::optional<ApplePayShippingContactEditingMode> m_shippingContactEditingMode;
#endif

#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<bool> m_supportsCouponCode;
    String m_couponCode;
#endif

#if defined(ApplePaySessionPaymentRequestAdditions_members)
    ApplePaySessionPaymentRequestAdditions_members
#endif
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ApplePaySessionPaymentRequest::ShippingType> {
    using values = EnumValues<
        WebCore::ApplePaySessionPaymentRequest::ShippingType,
        WebCore::ApplePaySessionPaymentRequest::ShippingType::Shipping,
        WebCore::ApplePaySessionPaymentRequest::ShippingType::Delivery,
        WebCore::ApplePaySessionPaymentRequest::ShippingType::StorePickup,
        WebCore::ApplePaySessionPaymentRequest::ShippingType::ServicePickup
    >;
};

} // namespace WTF

#endif
