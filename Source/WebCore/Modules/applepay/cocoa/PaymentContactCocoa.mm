/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PaymentContact.h"

#if ENABLE(APPLE_PAY)

#import "JSMainThreadExecState.h"
#import "PassKitSPI.h"
#import "SoftLinking.h"
#import <Contacts/Contacts.h>
#import <runtime/JSONObject.h>

SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CLASS(Contacts, CNMutablePostalAddress)
SOFT_LINK_CLASS(Contacts, CNPhoneNumber)

#if PLATFORM(MAC)
SOFT_LINK_PRIVATE_FRAMEWORK(PassKit)
#else
SOFT_LINK_FRAMEWORK(PassKit)
#endif

SOFT_LINK_CLASS(PassKit, PKContact)

namespace WebCore {

static bool isValidPaymentContactPropertyName(NSString* propertyName)
{
    static NSSet *validPropertyNames = [[NSSet alloc] initWithObjects:@"familyName", @"givenName", @"emailAddress", @"phoneNumber", @"addressLines", @"locality", @"postalCode", @"administrativeArea", @"country", @"countryCode", nil ];

    return [validPropertyNames containsObject:propertyName];
}

static RetainPtr<PKContact> fromDictionary(NSDictionary *dictionary, String& errorMessage)
{
    for (NSString *propertyName in dictionary) {
        if (!isValidPaymentContactPropertyName(propertyName)) {
            errorMessage = makeString("\"" + String(propertyName), "\" is not a valid payment contact property name.");
            return nullptr;
        }
    }

    auto result = adoptNS([allocPKContactInstance() init]);

    NSString *familyName = dynamic_objc_cast<NSString>(dictionary[@"familyName"]);
    NSString *givenName = dynamic_objc_cast<NSString>(dictionary[@"givenName"]);

    if (familyName || givenName) {
        auto name = adoptNS([[NSPersonNameComponents alloc] init]);
        [name setFamilyName:familyName];
        [name setGivenName:givenName];
        [result setName:name.get()];
    }

    [result setEmailAddress:dynamic_objc_cast<NSString>(dictionary[@"emailAddress"])];

    if (NSString *phoneNumber = dynamic_objc_cast<NSString>(dictionary[@"phoneNumber"]))
        [result setPhoneNumber:adoptNS([allocCNPhoneNumberInstance() initWithStringValue:phoneNumber]).get()];

    NSArray *addressLines = dynamic_objc_cast<NSArray>(dictionary[@"addressLines"]);
    if (addressLines.count) {
        auto address = adoptNS([allocCNMutablePostalAddressInstance() init]);

        [address setStreet:[addressLines componentsJoinedByString:@"\n"]];

        if (NSString *locality = dynamic_objc_cast<NSString>(dictionary[@"locality"]))
            [address setCity:locality];
        if (NSString *postalCode = dynamic_objc_cast<NSString>(dictionary[@"postalCode"]))
            [address setPostalCode:postalCode];
        if (NSString *administrativeArea = dynamic_objc_cast<NSString>(dictionary[@"administrativeArea"]))
            [address setState:administrativeArea];
        if (NSString *country = dynamic_objc_cast<NSString>(dictionary[@"country"]))
            [address setCountry:country];
        if (NSString *countryCode = dynamic_objc_cast<NSString>(dictionary[@"countryCode"]))
            [address setISOCountryCode:countryCode];

        [result setPostalAddress:address.get()];
    }

    return result;
}

Optional<PaymentContact> PaymentContact::fromJS(JSC::ExecState& state, JSC::JSValue value, String& errorMessage)
{
    // FIXME: Don't round-trip using NSString.
    auto jsonString = JSONStringify(&state, value, 0);
    if (!jsonString)
        return Nullopt;

    auto dictionary = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[(NSString *)jsonString dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil]);
    if (!dictionary || ![dictionary isKindOfClass:[NSDictionary class]])
        return Nullopt;

    auto pkContact = fromDictionary(dictionary, errorMessage);
    if (!pkContact)
        return Nullopt;

    return PaymentContact(pkContact.get());
}

// FIXME: This should use the PassKit SPI.
RetainPtr<NSDictionary> toDictionary(PKContact *contact)
{
    ASSERT(contact);
    (void)contact;

    auto result = adoptNS([[NSMutableDictionary alloc] init]);

    if (contact.phoneNumber)
        [result setObject:contact.phoneNumber.stringValue forKey:@"phoneNumber"];
    if (contact.emailAddress)
        [result setObject:contact.emailAddress forKey:@"emailAddress"];
    if (contact.name.givenName)
        [result setObject:contact.name.givenName forKey:@"givenName"];
    if (contact.name.familyName)
        [result setObject:contact.name.familyName forKey:@"familyName"];
    if (contact.postalAddress.street.length)
        [result setObject:[contact.postalAddress.street componentsSeparatedByString:@"\n"] forKey:@"addressLines"];
    if (contact.postalAddress.city)
        [result setObject:contact.postalAddress.city forKey:@"locality"];
    if (contact.postalAddress.city)
        [result setObject:contact.postalAddress.postalCode forKey:@"postalCode"];
    if (contact.postalAddress.city)
        [result setObject:contact.postalAddress.state forKey:@"administrativeArea"];
    if (contact.postalAddress.city)
        [result setObject:contact.postalAddress.country forKey:@"country"];
    if (contact.postalAddress.city)
        [result setObject:contact.postalAddress.ISOCountryCode forKey:@"countryCode"];

    return result;
}

JSC::JSValue PaymentContact::toJS(JSC::ExecState& exec) const
{
    auto dictionary = toDictionary(m_pkContact.get());

    // FIXME: Don't round-trip using NSString.
    auto jsonString = adoptNS([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:dictionary.get() options:0 error:nullptr] encoding:NSUTF8StringEncoding]);
    return JSONParse(&exec, jsonString.get());
}


}

#endif
