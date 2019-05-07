/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/CustomHeaderFields.h>

namespace API {

class CustomHeaderFields final : public ObjectImpl<Object::Type::CustomHeaderFields> {
public:
    template<typename... Args> static Ref<CustomHeaderFields> create(Args&&... args)
    {
        return adoptRef(*new CustomHeaderFields(std::forward<Args>(args)...));
    }

    CustomHeaderFields() = default;

    const Vector<WebCore::HTTPHeaderField>& fields() const { return m_fields.fields; }
    void setFields(Vector<WebCore::HTTPHeaderField>&& fields) { m_fields.fields = WTFMove(fields); }

    const Vector<WTF::String> thirdPartyDomains() const { return m_fields.thirdPartyDomains; }
    void setThirdPartyDomains(Vector<WTF::String>&& domains) { m_fields.thirdPartyDomains = WTFMove(domains); }

    const WebCore::CustomHeaderFields& coreFields() const { return m_fields; }

private:
    CustomHeaderFields(const WebCore::CustomHeaderFields& fields)
        : m_fields(fields) { }

    WebCore::CustomHeaderFields m_fields;
};

} // namespace API
