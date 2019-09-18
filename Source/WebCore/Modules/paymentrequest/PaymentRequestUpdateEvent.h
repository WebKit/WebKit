/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#if ENABLE(PAYMENT_REQUEST)

#include "Event.h"
#include "PaymentRequestUpdateEventInit.h"

namespace WebCore {

class DOMPromise;
struct PaymentRequestUpdateEventInit;

class PaymentRequestUpdateEvent : public Event {
public:
    template <typename... Args> static Ref<PaymentRequestUpdateEvent> create(Args&&... args)
    {
        return adoptRef(*new PaymentRequestUpdateEvent(std::forward<Args>(args)...));
    }
    ~PaymentRequestUpdateEvent();
    ExceptionOr<void> updateWith(Ref<DOMPromise>&&);

protected:
    explicit PaymentRequestUpdateEvent(const AtomString& type);
    PaymentRequestUpdateEvent(const AtomString& type, const PaymentRequestUpdateEventInit&);

    // Event
    EventInterface eventInterface() const override;

private:
    bool m_waitForUpdate { false };
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
