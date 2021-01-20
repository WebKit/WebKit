/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include <WebCore/SecurityOrigin.h>
#include <wtf/Ref.h>

namespace API {

class SecurityOrigin : public API::ObjectImpl<API::Object::Type::SecurityOrigin> {
public:
    static Ref<SecurityOrigin> createFromString(const WTF::String& string)
    {
        return create(WebCore::SecurityOrigin::createFromString(string));
    }

    static Ref<SecurityOrigin> create(const WTF::String& protocol, const WTF::String& host, Optional<uint16_t> port)
    {
        return create(WebCore::SecurityOrigin::create(protocol, host, port));
    }

    static Ref<SecurityOrigin> create(const WebCore::SecurityOrigin& securityOrigin)
    {
        return adoptRef(*new SecurityOrigin(securityOrigin));
    }

    WebCore::SecurityOrigin& securityOrigin() const { return m_securityOrigin.get(); }

private:
    SecurityOrigin(Ref<WebCore::SecurityOrigin>&& securityOrigin)
        : m_securityOrigin(WTFMove(securityOrigin))
    {
    }

    SecurityOrigin(const WebCore::SecurityOrigin& securityOrigin)
        : m_securityOrigin(securityOrigin.isolatedCopy())
    {
    }

    Ref<WebCore::SecurityOrigin> m_securityOrigin;
};

}
