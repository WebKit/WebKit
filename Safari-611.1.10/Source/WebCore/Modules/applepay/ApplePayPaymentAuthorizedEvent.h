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

#pragma once

#if ENABLE(APPLE_PAY)

#include "ApplePayPayment.h"
#include "Event.h"

namespace WebCore {

class Payment;

class ApplePayPaymentAuthorizedEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(ApplePayPaymentAuthorizedEvent);
public:
    static Ref<ApplePayPaymentAuthorizedEvent> create(const AtomString& type, unsigned version, const Payment& payment)
    {
        return adoptRef(*new ApplePayPaymentAuthorizedEvent(type, version, payment));
    }

    virtual ~ApplePayPaymentAuthorizedEvent();

    const ApplePayPayment& payment() const { return m_payment; }

private:
    ApplePayPaymentAuthorizedEvent(const AtomString& type, unsigned version, const Payment&);

    // Event.
    EventInterface eventInterface() const override;

    const ApplePayPayment m_payment;
};

}

#endif
