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
    friend struct IPC::ArgumentCoder<CoreIPCCNPostalAddress>;

    CoreIPCCNPostalAddress(RefPtr<StringImpl>&& street, RefPtr<StringImpl>&& subLocality, RefPtr<StringImpl>&& city, RefPtr<StringImpl>&& subAdministrativeArea, RefPtr<StringImpl>&& state, RefPtr<StringImpl>&& postalCode, RefPtr<StringImpl>&& country, RefPtr<StringImpl>&& isoCountryCode, RefPtr<StringImpl>&& formattedAddress)
        : m_street(WTF::move(street))
        , m_subLocality(WTF::move(subLocality))
        , m_city(WTF::move(city))
        , m_subAdministrativeArea(WTF::move(subAdministrativeArea))
        , m_state(WTF::move(state))
        , m_postalCode(WTF::move(postalCode))
        , m_country(WTF::move(country))
        , m_isoCountryCode(WTF::move(isoCountryCode))
        , m_formattedAddress(WTF::move(formattedAddress))
    {
    }

    RefPtr<StringImpl> m_street;
    RefPtr<StringImpl> m_subLocality;
    RefPtr<StringImpl> m_city;
    RefPtr<StringImpl> m_subAdministrativeArea;
    RefPtr<StringImpl> m_state;
    RefPtr<StringImpl> m_postalCode;
    RefPtr<StringImpl> m_country;
    RefPtr<StringImpl> m_isoCountryCode;
    RefPtr<StringImpl> m_formattedAddress;
};

class CoreIPCCNPhoneNumber {
public:
    CoreIPCCNPhoneNumber(CNPhoneNumber *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCCNPhoneNumber>;

    CoreIPCCNPhoneNumber(RefPtr<StringImpl>&& digits, RefPtr<StringImpl>&& countryCode)
        : m_digits(WTF::move(digits))
        , m_countryCode(WTF::move(countryCode))
    {
    }

    RefPtr<StringImpl> m_digits;
    RefPtr<StringImpl> m_countryCode;
};

struct CoreIPCContactLabeledValue {
    RefPtr<StringImpl> identifier;
    RefPtr<StringImpl> label;
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
    friend struct IPC::ArgumentCoder<CoreIPCCNContact>;
    CoreIPCCNContact() = default;

    RefPtr<StringImpl> m_identifier;
    bool m_personContactType { false };

    RefPtr<StringImpl> m_namePrefix;
    RefPtr<StringImpl> m_givenName;
    RefPtr<StringImpl> m_middleName;
    RefPtr<StringImpl> m_familyName;
    RefPtr<StringImpl> m_previousFamilyName;
    RefPtr<StringImpl> m_nameSuffix;
    RefPtr<StringImpl> m_nickname;
    RefPtr<StringImpl> m_organizationName;
    RefPtr<StringImpl> m_departmentName;
    RefPtr<StringImpl> m_jobTitle;
    RefPtr<StringImpl> m_phoneticGivenName;
    RefPtr<StringImpl> m_phoneticMiddleName;
    RefPtr<StringImpl> m_phoneticFamilyName;
    RefPtr<StringImpl> m_phoneticOrganizationName;
    RefPtr<StringImpl> m_note;

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

