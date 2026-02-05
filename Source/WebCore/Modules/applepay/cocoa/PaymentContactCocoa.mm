/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#import "PaymentContact.h"

#if ENABLE(APPLE_PAY)

#import "ApplePayPaymentContact.h"
#import <Contacts/Contacts.h>
#import <pal/cocoa/PassKitSoftLink.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/StringBuilder.h>

SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CLASS(Contacts, CNMutablePostalAddress)
SOFT_LINK_CLASS(Contacts, CNPhoneNumber)

namespace WebCore {

static RetainPtr<PKContact> convert(unsigned version, const ApplePayPaymentContact& contact)
{
    RetainPtr result = adoptNS([PAL::allocPKContactInstance() init]);

    RetainPtr<NSString> familyName;
    RetainPtr<NSString> phoneticFamilyName;
    if (!contact.familyName.isEmpty()) {
        familyName = contact.familyName.createNSString();
        if (version >= 3 && !contact.phoneticFamilyName.isEmpty())
            phoneticFamilyName = contact.phoneticFamilyName.createNSString();
    }

    RetainPtr<NSString> givenName;
    RetainPtr<NSString> phoneticGivenName;
    if (!contact.givenName.isEmpty()) {
        givenName = contact.givenName.createNSString();
        if (version >= 3 && !contact.phoneticGivenName.isEmpty())
            phoneticGivenName = contact.phoneticGivenName.createNSString();
    }

    if (familyName || givenName) {
        RetainPtr name = adoptNS([[NSPersonNameComponents alloc] init]);
        [name setFamilyName:familyName.get()];
        [name setGivenName:givenName.get()];
        if (phoneticFamilyName || phoneticGivenName) {
            auto phoneticName = adoptNS([[NSPersonNameComponents alloc] init]);
            [phoneticName setFamilyName:phoneticFamilyName.get()];
            [phoneticName setGivenName:phoneticGivenName.get()];
            [name setPhoneticRepresentation:phoneticName.get()];
        }
        [result setName:name.get()];
    }

    if (!contact.emailAddress.isEmpty())
        [result setEmailAddress:contact.emailAddress.createNSString().get()];

    if (!contact.phoneNumber.isEmpty())
        [result setPhoneNumber:adoptNS([allocCNPhoneNumberInstance() initWithStringValue:contact.phoneNumber.createNSString().get()]).get()];

    if (contact.addressLines && !contact.addressLines->isEmpty()) {
        RetainPtr address = adoptNS([allocCNMutablePostalAddressInstance() init]);

        StringBuilder builder;
        for (unsigned i = 0; i < contact.addressLines->size(); ++i) {
            builder.append(contact.addressLines->at(i));
            if (i != contact.addressLines->size() - 1)
                builder.append('\n');
        }

        [address setStreet:builder.createNSString().get()];

        if (!contact.subLocality.isEmpty())
            [address setSubLocality:contact.subLocality.createNSString().get()];
        if (!contact.locality.isEmpty())
            [address setCity:contact.locality.createNSString().get()];
        if (!contact.postalCode.isEmpty())
            [address setPostalCode:contact.postalCode.createNSString().get()];
        if (!contact.subAdministrativeArea.isEmpty())
            [address setSubAdministrativeArea:contact.subAdministrativeArea.createNSString().get()];
        if (!contact.administrativeArea.isEmpty())
            [address setState:contact.administrativeArea.createNSString().get()];
        if (!contact.country.isEmpty())
            [address setCountry:contact.country.createNSString().get()];
        if (!contact.countryCode.isEmpty())
            [address setISOCountryCode:contact.countryCode.createNSString().get()];

        [result setPostalAddress:address.get()];
    }

    return result;
}

static ApplePayPaymentContact convert(unsigned version, PKContact *contact)
{
    ASSERT(contact);

    ApplePayPaymentContact result;

    result.phoneNumber = static_cast<CNPhoneNumber *>(contact.phoneNumber).stringValue;
    result.emailAddress = contact.emailAddress;

    RetainPtr<NSPersonNameComponents> name = contact.name;
    result.givenName = name.get().givenName;
    result.familyName = name.get().familyName;

    if (version >= 3) {
        RetainPtr<NSPersonNameComponents> phoneticName = name.get().phoneticRepresentation;
        result.phoneticGivenName = phoneticName.get().givenName;
        result.phoneticFamilyName = phoneticName.get().familyName;
    }

    RetainPtr<CNPostalAddress> postalAddress = contact.postalAddress;
    if (postalAddress.get().street.length)
        result.addressLines = String(postalAddress.get().street).split('\n');
    result.subLocality = postalAddress.get().subLocality;
    result.locality = postalAddress.get().city;
    result.postalCode = postalAddress.get().postalCode;
    result.subAdministrativeArea = postalAddress.get().subAdministrativeArea;
    result.administrativeArea = postalAddress.get().state;
    result.country = postalAddress.get().country;
    result.countryCode = postalAddress.get().ISOCountryCode;

    return result;
}

static LocalizedApplePayPaymentContact convertLocalized(unsigned version, PKContact *contact)
{
    String localizedName;
    String localizedPhoneticName;

    if (RetainPtr<NSPersonNameComponents> name = contact.name) {
        localizedName = [NSPersonNameComponentsFormatter localizedStringFromPersonNameComponents:name.get() style:NSPersonNameComponentsFormatterStyleDefault options:0];

        if (version >= 3) {
            if (RetainPtr<NSPersonNameComponents> phoneticName = name.get().phoneticRepresentation)
                localizedPhoneticName = [NSPersonNameComponentsFormatter localizedStringFromPersonNameComponents:name.get() style:NSPersonNameComponentsFormatterStyleDefault options:NSPersonNameComponentsFormatterPhonetic];
        }
    }

    return LocalizedApplePayPaymentContact {
        convert(version, contact),
        WTF::move(localizedName),
        WTF::move(localizedPhoneticName),
    };
}

PaymentContact::PaymentContact() = default;

PaymentContact::PaymentContact(RetainPtr<PKContact>&& pkContact)
    : m_pkContact { WTF::move(pkContact) }
{
}

PaymentContact::~PaymentContact() = default;

PaymentContact PaymentContact::fromApplePayPaymentContact(unsigned version, const ApplePayPaymentContact& contact)
{
    return PaymentContact(convert(version, contact).get());
}

ApplePayPaymentContact PaymentContact::toApplePayPaymentContact(unsigned version) const
{
    return convert(version, m_pkContact.get());
}

LocalizedApplePayPaymentContact PaymentContact::toLocalizedApplePayPaymentContact(unsigned version) const
{
    return convertLocalized(version, m_pkContact.get());
}

RetainPtr<PKContact> PaymentContact::pkContact() const
{
    return m_pkContact;
}

}

#endif
