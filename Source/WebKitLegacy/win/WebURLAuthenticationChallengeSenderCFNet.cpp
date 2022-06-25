/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "WebKitDLL.h"
#include <initguid.h>
#include "WebURLAuthenticationChallengeSender.h"

#include "WebKit.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLCredential.h"
#include <WebCore/AuthenticationClient.h>
#include <WebCore/COMPtr.h>

using namespace WebCore;

// IWebURLAuthenticationChallengeSender -------------------------------------------------------------------

HRESULT WebURLAuthenticationChallengeSender::cancelAuthenticationChallenge(_In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    COMPtr<WebURLAuthenticationChallenge> webChallenge(Query, challenge);
    if (!webChallenge)
        return E_FAIL;

    m_client->receivedCancellation(webChallenge->authenticationChallenge());
    return S_OK;
}

HRESULT WebURLAuthenticationChallengeSender::continueWithoutCredentialForAuthenticationChallenge(_In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    COMPtr<WebURLAuthenticationChallenge> webChallenge(Query, challenge);
    if (!webChallenge)
        return E_FAIL;

    m_client->receivedRequestToContinueWithoutCredential(webChallenge->authenticationChallenge());
    return S_OK;
}

HRESULT WebURLAuthenticationChallengeSender::useCredential(_In_opt_ IWebURLCredential* credential, _In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    COMPtr<WebURLAuthenticationChallenge> webChallenge(Query, challenge);
    if (!webChallenge)
        return E_FAIL;
    
    COMPtr<WebURLCredential> webCredential;
    if (!credential || FAILED(credential->QueryInterface(__uuidof(WebURLCredential), (void**)&webCredential)))
        return E_FAIL;

    m_client->receivedCredential(webChallenge->authenticationChallenge(), webCredential->credential());
    return S_OK;
}
