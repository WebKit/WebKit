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

#import "config.h"
#import "CoreIPCContacts.h"

#if HAVE(CONTACTS)

#import <pal/spi/cocoa/ContactsSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/ContactsSoftLink.h>

namespace WebKit {

CoreIPCCNPhoneNumber::CoreIPCCNPhoneNumber(CNPhoneNumber *cnPhoneNumber)
    : m_digits(StringImpl::create(cnPhoneNumber.digits))
    , m_countryCode(StringImpl::create(cnPhoneNumber.countryCode))
{
}

RetainPtr<id> CoreIPCCNPhoneNumber::toID() const
{
    return [PAL::getCNPhoneNumberClassSingleton() phoneNumberWithDigits:createNSString(m_digits).get() countryCode:createNSString(m_countryCode).get()];
}

CoreIPCCNPostalAddress::CoreIPCCNPostalAddress(CNPostalAddress *cnPostalAddress)
    : m_street(StringImpl::create(cnPostalAddress.street))
    , m_subLocality(StringImpl::create(cnPostalAddress.subLocality))
    , m_city(StringImpl::create(cnPostalAddress.city))
    , m_subAdministrativeArea(StringImpl::create(cnPostalAddress.subAdministrativeArea))
    , m_state(StringImpl::create(cnPostalAddress.state))
    , m_postalCode(StringImpl::create(cnPostalAddress.postalCode))
    , m_country(StringImpl::create(cnPostalAddress.country))
    , m_isoCountryCode(StringImpl::create(cnPostalAddress.ISOCountryCode))
    , m_formattedAddress(StringImpl::create(cnPostalAddress.formattedAddress))
{
}

RetainPtr<id> CoreIPCCNPostalAddress::toID() const
{
    RetainPtr<CNMutablePostalAddress> address = adoptNS([[PAL::getCNMutablePostalAddressClassSingleton() alloc] init]);

    address.get().street = nsStringNilIfNull(m_street).get();
    address.get().subLocality = nsStringNilIfNull(m_subLocality).get();
    address.get().city = nsStringNilIfNull(m_city).get();
    address.get().subAdministrativeArea = nsStringNilIfNull(m_subAdministrativeArea).get();
    address.get().state = nsStringNilIfNull(m_state).get();
    address.get().postalCode = nsStringNilIfNull(m_postalCode).get();
    address.get().country = nsStringNilIfNull(m_country).get();
    address.get().ISOCountryCode = nsStringNilIfNull(m_isoCountryCode).get();
    address.get().formattedAddress = nsStringNilIfNull(m_formattedAddress).get();

    return address;
}

CoreIPCCNContact::CoreIPCCNContact(CNContact *contact)
    : m_identifier { StringImpl::create(contact.identifier) }
    , m_personContactType { contact.contactType == CNContactTypePerson }
{
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNamePrefixKeySingleton()] && contact.namePrefix)
        m_namePrefix = StringImpl::create(contact.namePrefix);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactGivenNameKeySingleton()] && contact.givenName)
        m_givenName = StringImpl::create(contact.givenName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactMiddleNameKeySingleton()] && contact.middleName)
        m_middleName = StringImpl::create(contact.middleName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactFamilyNameKeySingleton()] && contact.familyName)
        m_familyName = StringImpl::create(contact.familyName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPreviousFamilyNameKeySingleton()] && contact.previousFamilyName)
        m_previousFamilyName = StringImpl::create(contact.previousFamilyName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNameSuffixKeySingleton()] && contact.nameSuffix)
        m_nameSuffix = StringImpl::create(contact.nameSuffix);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNicknameKeySingleton()] && contact.nickname)
        m_nickname = StringImpl::create(contact.nickname);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactOrganizationNameKeySingleton()] && contact.organizationName)
        m_organizationName = StringImpl::create(contact.organizationName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactDepartmentNameKeySingleton()] && contact.departmentName)
        m_departmentName = StringImpl::create(contact.departmentName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactJobTitleKeySingleton()] && contact.jobTitle)
        m_jobTitle = StringImpl::create(contact.jobTitle);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticGivenNameKeySingleton()] && contact.phoneticGivenName)
        m_phoneticGivenName = StringImpl::create(contact.phoneticGivenName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticMiddleNameKeySingleton()] && contact.phoneticMiddleName)
        m_phoneticMiddleName = StringImpl::create(contact.phoneticMiddleName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticFamilyNameKeySingleton()] && contact.phoneticFamilyName)
        m_phoneticFamilyName = StringImpl::create(contact.phoneticFamilyName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticOrganizationNameKeySingleton()] && contact.phoneticOrganizationName)
        m_phoneticOrganizationName = StringImpl::create(contact.phoneticOrganizationName);
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNoteKeySingleton()] && contact.note)
        m_note = StringImpl::create(contact.note);

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactBirthdayKeySingleton()] && contact.birthday)
        m_birthday = contact.birthday;

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNonGregorianBirthdayKeySingleton()] && contact.nonGregorianBirthday)
        m_nonGregorianBirthday = contact.nonGregorianBirthday;

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactDatesKeySingleton()] && contact.dates) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL CNLabeledValue *labeledValue in contact.dates)
            m_dates.append({ StringImpl::create(labeledValue.identifier), StringImpl::create(labeledValue.label), CoreIPCDateComponents(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneNumbersKeySingleton()] && contact.phoneNumbers) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL CNLabeledValue *labeledValue in contact.phoneNumbers)
            m_phoneNumbers.append({ StringImpl::create(labeledValue.identifier), StringImpl::create(labeledValue.label), CoreIPCCNPhoneNumber(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactEmailAddressesKeySingleton()] && contact.emailAddresses) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL CNLabeledValue *labeledValue in contact.emailAddresses)
            m_emailAddresses.append({ StringImpl::create(labeledValue.identifier), StringImpl::create(labeledValue.label), (NSString *)labeledValue.value });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPostalAddressesKeySingleton()] && contact.postalAddresses) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL CNLabeledValue *labeledValue in contact.postalAddresses)
            m_postalAddresses.append({ StringImpl::create(labeledValue.identifier), StringImpl::create(labeledValue.label), CoreIPCCNPostalAddress(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactUrlAddressesKeySingleton()] && contact.urlAddresses) {
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL CNLabeledValue *labeledValue in contact.urlAddresses)
            m_urlAddresses.append({ StringImpl::create(labeledValue.identifier), StringImpl::create(labeledValue.label), (NSString *)labeledValue.value });
    }
}

static RetainPtr<NSArray> nsArrayFromVectorOfLabeledValues(const Vector<CoreIPCContactLabeledValue>& labeledValues)
{
    return createNSArray(labeledValues, [] (auto& labeledValue) -> RetainPtr<id> {
        auto theValue = WTF::visit([] (auto& actualValue) -> RetainPtr<id> {
            return actualValue.toID();
        }, labeledValue.value);

        return adoptNS([[PAL::getCNLabeledValueClassSingleton() alloc] initWithIdentifier:createNSString(labeledValue.identifier).get() label:createNSString(labeledValue.label).get() value:theValue.get()]);
    });
}

RetainPtr<id> CoreIPCCNContact::toID() const
{
    RetainPtr<CNMutableContact> result = adoptNS([[PAL::getCNMutableContactClassSingleton() alloc] initWithIdentifier:createNSString(m_identifier).get()]);
    result.get().contactType = m_personContactType ? CNContactTypePerson : CNContactTypeOrganization;

    if (m_namePrefix)
        result.get().namePrefix = createNSString(m_namePrefix).get();
    if (m_givenName)
        result.get().givenName = createNSString(m_givenName).get();
    if (m_middleName)
        result.get().middleName = createNSString(m_middleName).get();
    if (m_familyName)
        result.get().familyName = createNSString(m_familyName).get();
    if (m_previousFamilyName)
        result.get().previousFamilyName = createNSString(m_previousFamilyName).get();
    if (m_nameSuffix)
        result.get().nameSuffix = createNSString(m_nameSuffix).get();
    if (m_nickname)
        result.get().nickname = createNSString(m_nickname).get();
    if (m_organizationName)
        result.get().organizationName = createNSString(m_organizationName).get();
    if (m_departmentName)
        result.get().departmentName = createNSString(m_departmentName).get();
    if (m_jobTitle)
        result.get().jobTitle = createNSString(m_jobTitle).get();
    if (m_phoneticGivenName)
        result.get().phoneticGivenName = createNSString(m_phoneticGivenName).get();
    if (m_phoneticMiddleName)
        result.get().phoneticMiddleName = createNSString(m_phoneticMiddleName).get();
    if (m_phoneticFamilyName)
        result.get().phoneticFamilyName = createNSString(m_phoneticFamilyName).get();
    if (m_phoneticOrganizationName)
        result.get().phoneticOrganizationName = createNSString(m_phoneticOrganizationName).get();
    if (m_note)
        result.get().note = createNSString(m_note).get();

    if (m_birthday)
        result.get().birthday = m_birthday->toID().get();
    if (m_nonGregorianBirthday)
        result.get().nonGregorianBirthday = m_nonGregorianBirthday->toID().get();

    if (!m_dates.isEmpty())
        result.get().dates = nsArrayFromVectorOfLabeledValues(m_dates).get();
    if (!m_phoneNumbers.isEmpty())
        result.get().phoneNumbers = nsArrayFromVectorOfLabeledValues(m_phoneNumbers).get();
    if (!m_emailAddresses.isEmpty()) {
        auto emailAddresses = nsArrayFromVectorOfLabeledValues(m_emailAddresses);

        result.get().emailAddresses = emailAddresses.get();
    }
    if (!m_postalAddresses.isEmpty()) {
        auto postal = nsArrayFromVectorOfLabeledValues(m_postalAddresses);
        result.get().postalAddresses = postal.get();

    }
    if (!m_urlAddresses.isEmpty())
        result.get().urlAddresses = nsArrayFromVectorOfLabeledValues(m_urlAddresses).get();

    return result;
}

} // namespace WebKit

#endif // HAVE(CONTACTS)

