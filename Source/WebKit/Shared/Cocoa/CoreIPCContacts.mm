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
    : m_digits(cnPhoneNumber.digits)
    , m_countryCode(cnPhoneNumber.countryCode)
{
}

RetainPtr<id> CoreIPCCNPhoneNumber::toID() const
{
    return [PAL::getCNPhoneNumberClass() phoneNumberWithDigits:m_digits.createNSString().get() countryCode:m_countryCode.createNSString().get()];
}

CoreIPCCNPostalAddress::CoreIPCCNPostalAddress(CNPostalAddress *cnPostalAddress)
    : m_street(cnPostalAddress.street)
    , m_subLocality(cnPostalAddress.subLocality)
    , m_city(cnPostalAddress.city)
    , m_subAdministrativeArea(cnPostalAddress.subAdministrativeArea)
    , m_state(cnPostalAddress.state)
    , m_postalCode(cnPostalAddress.postalCode)
    , m_country(cnPostalAddress.country)
    , m_isoCountryCode(cnPostalAddress.ISOCountryCode)
    , m_formattedAddress(cnPostalAddress.formattedAddress)
{
}

RetainPtr<id> CoreIPCCNPostalAddress::toID() const
{
    RetainPtr<CNMutablePostalAddress> address = adoptNS([[PAL::getCNMutablePostalAddressClass() alloc] init]);

    address.get().street = nsStringNilIfNull(m_street);
    address.get().subLocality = nsStringNilIfNull(m_subLocality);
    address.get().city = nsStringNilIfNull(m_city);
    address.get().subAdministrativeArea = nsStringNilIfNull(m_subAdministrativeArea);
    address.get().state = nsStringNilIfNull(m_state);
    address.get().postalCode = nsStringNilIfNull(m_postalCode);
    address.get().country = nsStringNilIfNull(m_country);
    address.get().ISOCountryCode = nsStringNilIfNull(m_isoCountryCode);
    address.get().formattedAddress = nsStringNilIfNull(m_formattedAddress);

    return address;
}

CoreIPCCNContact::CoreIPCCNContact(CNContact *contact)
    : m_identifier { contact.identifier }
    , m_personContactType { contact.contactType == CNContactTypePerson }
{
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNamePrefixKey()] && contact.namePrefix)
        m_namePrefix = contact.namePrefix;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactGivenNameKey()] && contact.givenName)
        m_givenName = contact.givenName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactMiddleNameKey()] && contact.middleName)
        m_middleName = contact.middleName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactFamilyNameKey()] && contact.familyName)
        m_familyName = contact.familyName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPreviousFamilyNameKey()] && contact.previousFamilyName)
        m_previousFamilyName = contact.previousFamilyName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNameSuffixKey()] && contact.nameSuffix)
        m_nameSuffix = contact.nameSuffix;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNicknameKey()] && contact.nickname)
        m_nickname = contact.nickname;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactOrganizationNameKey()] && contact.organizationName)
        m_organizationName = contact.organizationName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactDepartmentNameKey()] && contact.departmentName)
        m_departmentName = contact.departmentName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactJobTitleKey()] && contact.jobTitle)
        m_jobTitle = contact.jobTitle;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticGivenNameKey()] && contact.phoneticGivenName)
        m_phoneticGivenName = contact.phoneticGivenName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticMiddleNameKey()] && contact.phoneticMiddleName)
        m_phoneticMiddleName = contact.phoneticMiddleName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticFamilyNameKey()] && contact.phoneticFamilyName)
        m_phoneticFamilyName = contact.phoneticFamilyName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneticOrganizationNameKey()] && contact.phoneticOrganizationName)
        m_phoneticOrganizationName = contact.phoneticOrganizationName;
    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNoteKey()] && contact.note)
        m_note = contact.note;

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactBirthdayKey()] && contact.birthday)
        m_birthday = contact.birthday;

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactNonGregorianBirthdayKey()] && contact.nonGregorianBirthday)
        m_nonGregorianBirthday = contact.nonGregorianBirthday;

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactDatesKey()] && contact.dates) {
        for (CNLabeledValue *labeledValue in contact.dates)
            m_dates.append({ labeledValue.identifier, labeledValue.label, CoreIPCDateComponents(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPhoneNumbersKey()] && contact.phoneNumbers) {
        for (CNLabeledValue *labeledValue in contact.phoneNumbers)
            m_phoneNumbers.append({ labeledValue.identifier, labeledValue.label, CoreIPCCNPhoneNumber(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactEmailAddressesKey()] && contact.emailAddresses) {
        for (CNLabeledValue *labeledValue in contact.emailAddresses)
            m_emailAddresses.append({ labeledValue.identifier, labeledValue.label, (NSString *)labeledValue.value });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactPostalAddressesKey()] && contact.postalAddresses) {
        for (CNLabeledValue *labeledValue in contact.postalAddresses)
            m_postalAddresses.append({ labeledValue.identifier, labeledValue.label, CoreIPCCNPostalAddress(labeledValue.value) });
    }

    if ([contact isKeyAvailable:PAL::get_Contacts_CNContactUrlAddressesKey()] && contact.urlAddresses) {
        for (CNLabeledValue *labeledValue in contact.urlAddresses)
            m_urlAddresses.append({ labeledValue.identifier, labeledValue.label, (NSString *)labeledValue.value });
    }
}

static RetainPtr<NSArray> nsArrayFromVectorOfLabeledValues(const Vector<CoreIPCContactLabeledValue>& labeledValues)
{
    return createNSArray(labeledValues, [] (auto& labeledValue) -> RetainPtr<id> {
        auto theValue = WTF::visit([] (auto& actualValue) -> RetainPtr<id> {
            return actualValue.toID();
        }, labeledValue.value);

        return adoptNS([[PAL::getCNLabeledValueClass() alloc] initWithIdentifier:labeledValue.identifier.createNSString().get() label:labeledValue.label.createNSString().get() value:theValue.get()]);
    });
}

RetainPtr<id> CoreIPCCNContact::toID() const
{
    RetainPtr<CNMutableContact> result = adoptNS([[PAL::getCNMutableContactClass() alloc] initWithIdentifier:m_identifier.createNSString().get()]);
    result.get().contactType = m_personContactType ? CNContactTypePerson : CNContactTypeOrganization;

    if (!m_namePrefix.isNull())
        result.get().namePrefix = m_namePrefix.createNSString().get();
    if (!m_givenName.isNull())
        result.get().givenName = m_givenName.createNSString().get();
    if (!m_middleName.isNull())
        result.get().middleName = m_middleName.createNSString().get();
    if (!m_familyName.isNull())
        result.get().familyName = m_familyName.createNSString().get();
    if (!m_previousFamilyName.isNull())
        result.get().previousFamilyName = m_previousFamilyName.createNSString().get();
    if (!m_nameSuffix.isNull())
        result.get().nameSuffix = m_nameSuffix.createNSString().get();
    if (!m_nickname.isNull())
        result.get().nickname = m_nickname.createNSString().get();
    if (!m_organizationName.isNull())
        result.get().organizationName = m_organizationName.createNSString().get();
    if (!m_departmentName.isNull())
        result.get().departmentName = m_departmentName.createNSString().get();
    if (!m_jobTitle.isNull())
        result.get().jobTitle = m_jobTitle.createNSString().get();
    if (!m_phoneticGivenName.isNull())
        result.get().phoneticGivenName = m_phoneticGivenName.createNSString().get();
    if (!m_phoneticMiddleName.isNull())
        result.get().phoneticMiddleName = m_phoneticMiddleName.createNSString().get();
    if (!m_phoneticFamilyName.isNull())
        result.get().phoneticFamilyName = m_phoneticFamilyName.createNSString().get();
    if (!m_phoneticOrganizationName.isNull())
        result.get().phoneticOrganizationName = m_phoneticOrganizationName.createNSString().get();
    if (!m_note.isNull())
        result.get().note = m_note.createNSString().get();

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

