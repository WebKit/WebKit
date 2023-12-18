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

#if USE(PASSKIT)

#include "CoreIPCContacts.h"
#include "CoreIPCPersonNameComponents.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS PKContact;

namespace WebKit {

class CoreIPCPKContact {
public:
    CoreIPCPKContact(PKContact *);

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPKContact, void>;

    CoreIPCPKContact(CoreIPCPersonNameComponents&& name, String&& emailAddress, CoreIPCCNPhoneNumber&& phoneNumber, CoreIPCCNPostalAddress&& postalAddress, String&& supplementarySublocality)
        : m_name(WTFMove(name))
        , m_emailAddress(WTFMove(emailAddress))
        , m_phoneNumber(WTFMove(phoneNumber))
        , m_postalAddress(WTFMove(postalAddress))
        , m_supplementarySublocality(WTFMove(supplementarySublocality))
    {
    }

    CoreIPCPersonNameComponents m_name;
    String m_emailAddress;
    CoreIPCCNPhoneNumber m_phoneNumber;
    CoreIPCCNPostalAddress m_postalAddress;
    String m_supplementarySublocality;
};

} // namespace WebKit

#endif // USE(PASSKIT)
