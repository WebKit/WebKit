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

#if ENABLE(APPLE_PAY)

#include "ApplePayErrorCode.h"
#include "ApplePayErrorContactField.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ApplePayError final : public RefCounted<ApplePayError> {
public:
    static Ref<ApplePayError> create(ApplePayErrorCode code, Optional<ApplePayErrorContactField> contactField, const String& message)
    {
        return adoptRef(*new ApplePayError(code, contactField, message));
    }

    virtual ~ApplePayError() = default;

    ApplePayErrorCode code() const { return m_code; }
    void setCode(ApplePayErrorCode code) { m_code = code; }

    Optional<ApplePayErrorContactField> contactField() const { return m_contactField; }
    void setContactField(Optional<ApplePayErrorContactField> contactField) { m_contactField = contactField; }

    String message() const { return m_message; }
    void setMessage(String&& message) { m_message = WTFMove(message); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static RefPtr<ApplePayError> decode(Decoder&);

private:
    ApplePayError(ApplePayErrorCode code, Optional<ApplePayErrorContactField> contactField, const String& message)
        : m_code(code)
        , m_contactField(contactField)
        , m_message(message)
    {
    }

    ApplePayErrorCode m_code;
    Optional<ApplePayErrorContactField> m_contactField;
    String m_message;
};

template<class Encoder>
void ApplePayError::encode(Encoder& encoder) const
{
    encoder << m_code;
    encoder << m_contactField;
    encoder << m_message;
}

template<class Decoder>
RefPtr<ApplePayError> ApplePayError::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    Optional<type> name; \
    decoder >> name; \
    if (!name) \
        return nullptr; \

    DECODE(code, ApplePayErrorCode)
    DECODE(contactField, Optional<ApplePayErrorContactField>)
    DECODE(message, String)

#undef DECODE

    return ApplePayError::create(WTFMove(*code), WTFMove(*contactField), WTFMove(*message));
}

} // namespace WebCore

#endif
