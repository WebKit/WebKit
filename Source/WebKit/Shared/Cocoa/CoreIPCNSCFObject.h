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

#if PLATFORM(COCOA)

#include "ArgumentCodersCocoa.h"
#include "CoreIPCPKDateComponentsRange.h"
#if USE(PASSKIT)
#if HAVE(WK_SECURE_CODING_PKPAYMENTMERCHANTSESSION)
#include "CoreIPCPKPaymentMerchantSession.h"
#endif
#if HAVE(WK_SECURE_CODING_PKPAYMENTTOKEN)
#include "CoreIPCPKPaymentToken.h"
#endif
#if HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)
#include "CoreIPCPKPaymentSetupFeature.h"
#endif
#endif
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class CoreIPCArray;
class CoreIPCCFType;
class CoreIPCColor;
#if USE(PASSKIT)
class CoreIPCPKPaymentMethod;
#if !HAVE(WK_SECURE_CODING_PKPAYMENTMERCHANTSESSION)
class CoreIPCPKPaymentMerchantSession;
#endif
#if !HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)
class CoreIPCPKPaymentSetupFeature;
#endif
class CoreIPCPKContact;
class CoreIPCPKSecureElementPass;
class CoreIPCPKPayment;
#if !HAVE(WK_SECURE_CODING_PKPAYMENTTOKEN)
class CoreIPCPKPaymentToken;
#endif
class CoreIPCPKShippingMethod;
class CoreIPCCNContact;
class CoreIPCCNPhoneNumber;
class CoreIPCCNPostalAddress;
#endif
class CoreIPCData;
class CoreIPCDate;
class CoreIPCDateComponents;
class CoreIPCDictionary;
class CoreIPCError;
class CoreIPCLocale;
class CoreIPCNSShadow;
class CoreIPCNSValue;
class CoreIPCNumber;
class CoreIPCNull;
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
class CoreIPCSecureCoding;
#endif
class CoreIPCString;
class CoreIPCURL;

using ObjectValue = Variant<
    std::nullptr_t,
    CoreIPCArray,
    CoreIPCCFType,
    CoreIPCColor,
    CoreIPCData,
    CoreIPCDate,
    CoreIPCDictionary,
    CoreIPCError,
    CoreIPCLocale,
    CoreIPCNSShadow,
    CoreIPCNSValue,
    CoreIPCNumber,
    CoreIPCNull,
#if USE(PASSKIT)
    CoreIPCPKPaymentMethod,
    CoreIPCPKPaymentMerchantSession,
    CoreIPCPKPaymentSetupFeature,
    CoreIPCPKContact,
    CoreIPCPKSecureElementPass,
    CoreIPCPKPayment,
    CoreIPCPKPaymentToken,
    CoreIPCPKShippingMethod,
    CoreIPCPKDateComponentsRange,
    CoreIPCCNContact,
    CoreIPCCNPhoneNumber,
    CoreIPCCNPostalAddress,
#endif
    CoreIPCDateComponents,
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    CoreIPCSecureCoding,
#endif // HAVE(WK_SECURE_CODING_NSURLREQUEST)
    CoreIPCString,
    CoreIPCURL
>;

class CoreIPCNSCFObject {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CoreIPCNSCFObject);
public:
    CoreIPCNSCFObject(id);
    CoreIPCNSCFObject(UniqueRef<ObjectValue>&&);

    RetainPtr<id> toID() const;

    static bool valueIsAllowed(IPC::Decoder&, ObjectValue&);

    const UniqueRef<ObjectValue>& value() const { return m_value; }
private:
    UniqueRef<ObjectValue> m_value;
};

} // namespace WebKit

namespace IPC {

// This ArgumentCoders specialization for UniqueRef<ObjectValue> is to allow us to use
// makeUniqueRefWithoutFastMallocCheck<>, since we can't make the variant fast malloc'ed
template<> struct ArgumentCoder<UniqueRef<WebKit::ObjectValue>> {
    static void encode(Encoder&, const UniqueRef<WebKit::ObjectValue>&);
    static std::optional<UniqueRef<WebKit::ObjectValue>> decode(Decoder&);
};

} // namespace IPC

#endif // PLATFORM(COCOA)
