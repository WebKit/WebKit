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

#import <Contacts/Contacts.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, Contacts)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNContact)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNLabeledValue)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNPhoneNumber)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNPostalAddress)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNMutableContact)
SOFT_LINK_CLASS_FOR_HEADER(PAL, CNMutablePostalAddress)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactDepartmentNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactGivenNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactJobTitleKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactMiddleNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactNamePrefixKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactNameSuffixKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactNicknameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactNoteKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactOrganizationNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPhoneticFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPhoneticGivenNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPhoneticMiddleNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPhoneticOrganizationNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPreviousFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactBirthdayKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactNonGregorianBirthdayKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPhoneNumbersKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactEmailAddressesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactPostalAddressesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactDatesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, Contacts, CNContactUrlAddressesKey, NSString *);

#endif
