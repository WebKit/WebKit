/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if HAVE(CONTACTS)

#include "CoreIPCDateComponents.h"
#include "CoreIPCString.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS CNContact;
OBJC_CLASS CNPhoneNumber;
OBJC_CLASS CNPostalAddress;

namespace WebKit {

class CoreIPCCNPostalAddress {
public:
    CoreIPCCNPostalAddress(CNPostalAddress *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCCNPostalAddress, void>;

    CoreIPCCNPostalAddress(String&& street, String&& subLocality, String&& city, String&& subAdministrativeArea, String&& state, String&& postalCode, String&& country, String&& isoCountryCode, String&& formattedAddress)
        : m_street(WTFMove(street))
        , m_subLocality(WTFMove(subLocality))
        , m_city(WTFMove(city))
        , m_subAdministrativeArea(WTFMove(subAdministrativeArea))
        , m_state(WTFMove(state))
        , m_postalCode(WTFMove(postalCode))
        , m_country(WTFMove(country))
        , m_isoCountryCode(WTFMove(isoCountryCode))
        , m_formattedAddress(WTFMove(formattedAddress))
    {
    }

    String m_street;
    String m_subLocality;
    String m_city;
    String m_subAdministrativeArea;
    String m_state;
    String m_postalCode;
    String m_country;
    String m_isoCountryCode;
    String m_formattedAddress;
};

class CoreIPCCNPhoneNumber {
public:
    CoreIPCCNPhoneNumber(CNPhoneNumber *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCCNPhoneNumber, void>;

    CoreIPCCNPhoneNumber(String&& digits, String&& countryCode)
        : m_digits(WTFMove(digits))
        , m_countryCode(WTFMove(countryCode))
    {
    }

    String m_digits;
    String m_countryCode;
};

struct CoreIPCContactLabeledValue {
    String identifier;
    String label;
    Variant<CoreIPCDateComponents, CoreIPCCNPhoneNumber, CoreIPCCNPostalAddress, CoreIPCString> value;

    template <typename T> static bool allValuesAreOfType(const Vector<CoreIPCContactLabeledValue>& values) {
        for (auto& value : values) {
            if (!std::holds_alternative<T>(value.value))
                return false;
        }
        return true;
    }
};

class CoreIPCCNContact {
public:
    CoreIPCCNContact(CNContact *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCCNContact, void>;
    CoreIPCCNContact() = default;

    String m_identifier;
    bool m_personContactType { false };

    String m_namePrefix;
    String m_givenName;
    String m_middleName;
    String m_familyName;
    String m_previousFamilyName;
    String m_nameSuffix;
    String m_nickname;
    String m_organizationName;
    String m_departmentName;
    String m_jobTitle;
    String m_phoneticGivenName;
    String m_phoneticMiddleName;
    String m_phoneticFamilyName;
    String m_phoneticOrganizationName;
    String m_note;

    std::optional<CoreIPCDateComponents> m_birthday;
    std::optional<CoreIPCDateComponents> m_nonGregorianBirthday;

    Vector<CoreIPCContactLabeledValue> m_dates;
    Vector<CoreIPCContactLabeledValue> m_phoneNumbers;
    Vector<CoreIPCContactLabeledValue> m_emailAddresses;
    Vector<CoreIPCContactLabeledValue> m_postalAddresses;
    Vector<CoreIPCContactLabeledValue> m_urlAddresses;
};

} // namespace WebKit

#endif // HAVE(CONTACTS)

