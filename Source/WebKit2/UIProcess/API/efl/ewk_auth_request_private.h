/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef ewk_auth_request_private_h
#define ewk_auth_request_private_h

#include "WKEinaSharedString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class AuthenticationChallengeProxy;
}

class Ewk_Auth_Request : public RefCounted<Ewk_Auth_Request> {
public:
    static PassRefPtr<Ewk_Auth_Request> create(WebKit::AuthenticationChallengeProxy* authenticationChallenge)
    {
        return adoptRef(new Ewk_Auth_Request(authenticationChallenge));
    }
    ~Ewk_Auth_Request();

    const char* suggestedUsername() const;
    const char* realm() const;
    const char* host() const;
    bool isRetrying() const;

    bool continueWithoutCredential();
    bool authenticate(const String& username, const String& password);

private:
    explicit Ewk_Auth_Request(WebKit::AuthenticationChallengeProxy* authenticationChallenge);

    RefPtr<WebKit::AuthenticationChallengeProxy> m_authenticationChallenge;
    bool m_wasHandled;
    mutable WKEinaSharedString m_suggestedUsername;
    mutable WKEinaSharedString m_realm;
    mutable WKEinaSharedString m_host;
};

#endif // ewk_auth_request_private_h
