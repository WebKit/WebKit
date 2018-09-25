/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Authenticator.h"
#include "LocalConnection.h"
#include <wtf/UniqueRef.h>

OBJC_CLASS LAContext;

namespace WebKit {

class LocalAuthenticator final : public Authenticator {
public:
    // Here is the FSM.
    // MakeCredential: Init => RequestReceived => UserConsented => Attested => End
    // GetAssertion: Init => RequestReceived => UserConsented => End
    enum class State {
        Init,
        RequestReceived,
        UserConsented,
        Attested,
    };

    static Ref<LocalAuthenticator> create(UniqueRef<LocalConnection>&& connection)
    {
        return adoptRef(*new LocalAuthenticator(WTFMove(connection)));
    }

private:
    explicit LocalAuthenticator(UniqueRef<LocalConnection>&&);

    void makeCredential() final;
    void continueMakeCredentialAfterUserConsented(LocalConnection::UserConsent);
    void continueMakeCredentialAfterAttested(SecKeyRef, NSArray *certificates, NSError *);

    void getAssertion() final;
    void continueGetAssertionAfterUserConsented(LocalConnection::UserConsent, LAContext *, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& userhandle);

    State m_state { State::Init };
    UniqueRef<LocalConnection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
