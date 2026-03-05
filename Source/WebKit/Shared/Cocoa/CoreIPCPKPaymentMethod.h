/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CNContact;
OBJC_CLASS PKPaymentMethod;
OBJC_CLASS PKSecureElementPass;

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTMETHOD)

namespace WebKit {

enum class PKPaymentMethodType : uint8_t {
    Unknown = 0,
    Debit = 1,
    Credit = 2,
    Prepaid = 3,
    Store = 4,
    EMoney = 5,
};

struct CoreIPCPKPaymentMethodData {
    std::optional<PKPaymentMethodType> type;
    RetainPtr<NSString> displayName;
    RetainPtr<NSString> network;
    RetainPtr<PKSecureElementPass> paymentPass;
    RetainPtr<NSString> peerPaymentQuoteIdentifier;
    RetainPtr<CNContact> billingAddress;
    RetainPtr<NSString> installmentBindToken;
    std::optional<bool> usePeerPaymentBalance;
};

class CoreIPCPKPaymentMethod {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCPKPaymentMethod);
public:
    CoreIPCPKPaymentMethod(PKPaymentMethod *);
    CoreIPCPKPaymentMethod(std::optional<CoreIPCPKPaymentMethodData>&&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPKPaymentMethod>;

    std::optional<CoreIPCPKPaymentMethodData> m_data;
};

} // namespace WebKit

#endif
