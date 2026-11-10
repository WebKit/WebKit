/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class PaymentAddress final : public RefCounted<PaymentAddress> {
public:
    template <typename... Args> static Ref<PaymentAddress> create(Args&&... args)
    {
        return adoptRef(*new PaymentAddress(std::forward<Args>(args)...));
    }

    const RefPtr<StringImpl>& country() const LIFETIME_BOUND { return m_country; }
    const Vector<RefPtr<StringImpl>>& addressLine() const LIFETIME_BOUND { return m_addressLine; }
    const RefPtr<StringImpl>& region() const LIFETIME_BOUND { return m_region; }
    const RefPtr<StringImpl>& city() const LIFETIME_BOUND { return m_city; }
    const RefPtr<StringImpl>& dependentLocality() const LIFETIME_BOUND { return m_dependentLocality; }
    const RefPtr<StringImpl>& postalCode() const LIFETIME_BOUND { return m_postalCode; }
    const RefPtr<StringImpl>& sortingCode() const LIFETIME_BOUND { return m_sortingCode; }
    const RefPtr<StringImpl>& organization() const LIFETIME_BOUND { return m_organization; }
    const RefPtr<StringImpl>& recipient() const LIFETIME_BOUND { return m_recipient; }
    const RefPtr<StringImpl>& phone() const LIFETIME_BOUND { return m_phone; }

private:
    PaymentAddress(StringImpl* country, const Vector<RefPtr<StringImpl>>& addressLine, StringImpl* region, StringImpl* city, StringImpl* dependentLocality, StringImpl* postalCode, StringImpl* sortingCode, StringImpl* organization, StringImpl* recipient, StringImpl* phone);

    RefPtr<StringImpl> m_country;
    Vector<RefPtr<StringImpl>> m_addressLine;
    RefPtr<StringImpl> m_region;
    RefPtr<StringImpl> m_city;
    RefPtr<StringImpl> m_dependentLocality;
    RefPtr<StringImpl> m_postalCode;
    RefPtr<StringImpl> m_sortingCode;
    RefPtr<StringImpl> m_organization;
    RefPtr<StringImpl> m_recipient;
    RefPtr<StringImpl> m_phone;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
