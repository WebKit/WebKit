/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "BasicCredential.h"
#include "IDLTypes.h"
#include "IdentityCredentialProtocol.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DigitalCredential;
template<typename IDLType> class DOMPromiseDeferred;

using DigitalCredentialPromise = DOMPromiseDeferred<IDLInterface<DigitalCredential>>;

class DigitalCredential final : public BasicCredential {
public:
    static Ref<DigitalCredential> create(Ref<ArrayBuffer>&& data, IdentityCredentialProtocol);

    virtual ~DigitalCredential();

    ArrayBuffer* data() const
    {
        return m_data.get();
    };

    IdentityCredentialProtocol protocol() const
    {
        return m_protocol;
    }

private:
    DigitalCredential(Ref<ArrayBuffer>&& data, IdentityCredentialProtocol);

    Type credentialType() const final { return Type::DigitalCredential; }

    IdentityCredentialProtocol m_protocol;
    RefPtr<ArrayBuffer> m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(DigitalCredential, BasicCredential::Type::DigitalCredential)

#endif // ENABLE(WEB_AUTHN)
