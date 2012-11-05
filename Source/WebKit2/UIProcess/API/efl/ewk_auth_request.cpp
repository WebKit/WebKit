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

#include "config.h"
#include "ewk_auth_request.h"

#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "WebCredential.h"
#include "WebProtectionSpace.h"
#include "WebString.h"
#include "ewk_auth_request_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

Ewk_Auth_Request::Ewk_Auth_Request(AuthenticationChallengeProxy* authenticationChallenge)
    : m_authenticationChallenge(authenticationChallenge)
    , m_wasHandled(false)
{
    ASSERT(m_authenticationChallenge);
}

Ewk_Auth_Request::~Ewk_Auth_Request()
{
    if (!m_wasHandled)
        continueWithoutCredential();
}

const char* Ewk_Auth_Request::suggestedUsername() const
{
    if (!m_suggestedUsername) {
        WebCredential* credential = m_authenticationChallenge->proposedCredential();
        ASSERT(credential);

        const String& suggestedUsername = credential->user();
        if (suggestedUsername.isEmpty())
            return 0;

        m_suggestedUsername = suggestedUsername.utf8().data();
    }

    return m_suggestedUsername;
}

const char* Ewk_Auth_Request::realm() const
{
    if (!m_realm) {
        WebProtectionSpace* protectionSpace = m_authenticationChallenge->protectionSpace();
        ASSERT(protectionSpace);

        const String& realm = protectionSpace->realm();
        if (realm.isEmpty())
            return 0;

        m_realm = realm.utf8().data();
    }

    return m_realm;
}

const char* Ewk_Auth_Request::host() const
{
    if (!m_host) {
        WebProtectionSpace* protectionSpace = m_authenticationChallenge->protectionSpace();
        ASSERT(protectionSpace);

        const String& host = protectionSpace->host();
        if (host.isEmpty())
            return 0;

        m_host = host.utf8().data();
    }

    return m_host;
}

bool Ewk_Auth_Request::continueWithoutCredential()
{
    if (m_wasHandled)
        return false;

    m_wasHandled = true;
    m_authenticationChallenge->useCredential(0);

    return true;
}

bool Ewk_Auth_Request::authenticate(const String& username, const String& password)
{
    if (m_wasHandled)
        return false;

    m_wasHandled = true;
    RefPtr<WebCredential> credential = WebCredential::create(WebString::create(username).get(), WebString::create(password).get(), CredentialPersistenceForSession);
    m_authenticationChallenge->useCredential(credential.get());

    return true;
}

bool Ewk_Auth_Request::isRetrying() const
{
    return m_authenticationChallenge->previousFailureCount() > 0;
}

Ewk_Auth_Request* ewk_auth_request_ref(Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    request->ref();

    return request;
}

void ewk_auth_request_unref(Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);

    request->deref();
}

const char* ewk_auth_request_suggested_username_get(const Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->suggestedUsername();
}

Eina_Bool ewk_auth_request_cancel(Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);

    return request->continueWithoutCredential();
}

Eina_Bool ewk_auth_request_authenticate(Ewk_Auth_Request* request, const char* username, const char* password)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(username, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(password, false);

    return request->authenticate(String::fromUTF8(username), String::fromUTF8(password));
}

Eina_Bool ewk_auth_request_retrying_get(const Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);

    return request->isRetrying();
}

const char* ewk_auth_request_realm_get(const Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->realm();
}

const char* ewk_auth_request_host_get(const Ewk_Auth_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    return request->host();
}
