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
#import <pal/cocoa/ContactsSoftLink.h>

namespace WebKit {

CoreIPCCNPhoneNumber::CoreIPCCNPhoneNumber(CNPhoneNumber *cnPhoneNumber)
    : m_digits(cnPhoneNumber.digits)
    , m_countryCode(cnPhoneNumber.countryCode)
{
}

RetainPtr<id> CoreIPCCNPhoneNumber::toID() const
{
    return [PAL::getCNPhoneNumberClass() phoneNumberWithDigits:(NSString *)m_digits countryCode:(NSString *)m_countryCode];
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

    address.get().street = (NSString *)m_street;
    address.get().subLocality = (NSString *)m_subLocality;
    address.get().city = (NSString *)m_city;
    address.get().subAdministrativeArea = (NSString *)m_subAdministrativeArea;
    address.get().state = (NSString *)m_state;
    address.get().postalCode = (NSString *)m_postalCode;
    address.get().country = (NSString *)m_country;
    address.get().ISOCountryCode = (NSString *)m_isoCountryCode;
    address.get().formattedAddress = (NSString *)m_formattedAddress;

    return address;
}

} // namespace WebKit

#endif // HAVE(CONTACTS)

