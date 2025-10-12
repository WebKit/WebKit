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
#import "CoreIPCPassKit.h"

#if USE(PASSKIT)

#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

CoreIPCPKContact::CoreIPCPKContact(PKContact *contact)
    : m_emailAddress(contact.emailAddress)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    , m_supplementarySublocality(contact.supplementarySubLocality)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    if (contact.name)
        m_name = contact.name;
    if (contact.phoneNumber)
        m_phoneNumber = contact.phoneNumber;
    if (contact.postalAddress)
        m_postalAddress = contact.postalAddress;
}

RetainPtr<id> CoreIPCPKContact::toID() const
{
    RetainPtr<PKContact> contact = adoptNS([[PAL::getPKContactClassSingleton() alloc] init]);

    if (m_name)
        contact.get().name = (NSPersonNameComponents *)m_name->toID();
    if (m_phoneNumber)
        contact.get().phoneNumber = (CNPhoneNumber *)m_phoneNumber->toID();
    if (m_postalAddress)
        contact.get().postalAddress = (CNPostalAddress *)m_postalAddress->toID();

    contact.get().emailAddress = nsStringNilIfNull(m_emailAddress);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    contact.get().supplementarySubLocality = nsStringNilIfNull(m_supplementarySublocality);
ALLOW_DEPRECATED_DECLARATIONS_END

    return contact;
}

} // namespace WebKit

#endif // USE(PASSKIT)
