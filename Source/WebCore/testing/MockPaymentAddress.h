/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ApplePayPaymentContact.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MockPaymentAddress {
    String phoneNumber;
    String emailAddress;
    String givenName;
    String familyName;
    String phoneticGivenName;
    String phoneticFamilyName;
    std::optional<Vector<String>> addressLines;
    String subLocality;
    String locality;
    String postalCode;
    String subAdministrativeArea;
    String administrativeArea;
    String country;
    String countryCode;
    String localizedName;
    String localizedPhoneticName;
};

inline MockPaymentAddress toMockPaymentAddress(const LocalizedApplePayPaymentContact& contact)
{
    return {
        contact.phoneNumber,
        contact.emailAddress,
        contact.givenName,
        contact.familyName,
        contact.phoneticGivenName,
        contact.phoneticFamilyName,
        contact.addressLines,
        contact.subLocality,
        contact.locality,
        contact.postalCode,
        contact.subAdministrativeArea,
        contact.administrativeArea,
        contact.country,
        contact.countryCode,
        contact.localizedName,
        contact.localizedPhoneticName
    };
}

inline ApplePayPaymentContact toApplePayPaymentContact(const MockPaymentAddress& address)
{
    return {
        .phoneNumber = address.phoneNumber,
        .emailAddress = address.emailAddress,
        .givenName = address.givenName,
        .familyName = address.familyName,
        .phoneticGivenName = address.phoneticGivenName,
        .phoneticFamilyName = address.phoneticFamilyName,
        .addressLines = address.addressLines,
        .subLocality = address.subLocality,
        .locality = address.locality,
        .postalCode = address.postalCode,
        .subAdministrativeArea = address.subAdministrativeArea,
        .administrativeArea = address.administrativeArea,
        .country = address.country,
        .countryCode = address.countryCode
    };
}

inline LocalizedApplePayPaymentContact toLocalizedApplePayPaymentContact(const MockPaymentAddress& address)
{
    LocalizedApplePayPaymentContact contact;
    contact.phoneNumber = address.phoneNumber;
    contact.emailAddress = address.emailAddress;
    contact.givenName = address.givenName;
    contact.familyName = address.familyName;
    contact.phoneticGivenName = address.phoneticGivenName;
    contact.phoneticFamilyName = address.phoneticFamilyName;
    contact.addressLines = address.addressLines;
    contact.subLocality = address.subLocality;
    contact.locality = address.locality;
    contact.postalCode = address.postalCode;
    contact.subAdministrativeArea = address.subAdministrativeArea;
    contact.administrativeArea = address.administrativeArea;
    contact.country = address.country;
    contact.countryCode = address.countryCode;
    contact.localizedName = address.localizedName;
    contact.localizedPhoneticName = address.localizedPhoneticName;
    return contact;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
