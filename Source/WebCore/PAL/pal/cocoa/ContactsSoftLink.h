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

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if HAVE(CONTACTS)

#import <Contacts/Contacts.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, Contacts)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNContact)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNLabeledValue)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNPhoneNumber)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNPostalAddress)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNMutableContact)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, CNMutablePostalAddress)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactDepartmentNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactGivenNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactJobTitleKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactMiddleNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactNamePrefixKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactNameSuffixKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactNicknameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactNoteKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactOrganizationNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPhoneticFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPhoneticGivenNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPhoneticMiddleNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPhoneticOrganizationNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPreviousFamilyNameKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactBirthdayKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactNonGregorianBirthdayKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPhoneNumbersKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactEmailAddressesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactPostalAddressesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactDatesKey, NSString *);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, Contacts, CNContactUrlAddressesKey, NSString *);

#endif

#endif // !__has_feature(modules)
