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

    const String& country() const { return m_country; }
    const Vector<String>& addressLine() const { return m_addressLine; }
    const String& region() const { return m_region; }
    const String& city() const { return m_city; }
    const String& dependentLocality() const { return m_dependentLocality; }
    const String& postalCode() const { return m_postalCode; }
    const String& sortingCode() const { return m_sortingCode; }
    const String& organization() const { return m_organization; }
    const String& recipient() const { return m_recipient; }
    const String& phone() const { return m_phone; }

private:
    PaymentAddress(const String& country, const Vector<String>& addressLine, const String& region, const String& city, const String& dependentLocality, const String& postalCode, const String& sortingCode, const String& organization, const String& recipient, const String& phone);

    String m_country;
    Vector<String> m_addressLine;
    String m_region;
    String m_city;
    String m_dependentLocality;
    String m_postalCode;
    String m_sortingCode;
    String m_organization;
    String m_recipient;
    String m_phone;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
