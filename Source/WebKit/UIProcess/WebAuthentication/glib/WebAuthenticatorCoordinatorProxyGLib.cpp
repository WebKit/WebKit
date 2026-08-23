/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "WebAuthenticatorCoordinatorProxy.h"

#if ENABLE(WEB_AUTHN)

#include "WebAuthenticationRequestData.h"
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/AuthenticatorResponseData.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

void WebAuthenticatorCoordinatorProxy::performRequest(WebAuthenticationRequestData&& requestData, RequestCompletionHandler&& handler)
{
    UNUSED_PARAM(requestData);
    handler({ }, AuthenticatorAttachment::CrossPlatform, ExceptionData { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void WebAuthenticatorCoordinatorProxy::cancel(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOriginData&, QueryCompletionHandler&& handler)
{
    handler(false);
}

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(const SecurityOriginData&, QueryCompletionHandler&& handler)
{
    handler(false);
}

void WebAuthenticatorCoordinatorProxy::getClientCapabilities(const SecurityOriginData&, CapabilitiesCompletionHandler&& handler)
{
    // Keys must be sorted in lexicographic order.
    Vector<KeyValuePair<String, bool>> capabilities;
    capabilities.append({ "conditionalCreate"_s, false });
    capabilities.append({ "conditionalGet"_s, false });
    capabilities.append({ "hybridTransport"_s, false });
    capabilities.append({ "passkeyPlatformAuthenticator"_s, false });
    capabilities.append({ "relatedOrigins"_s, false });
    capabilities.append({ "signalAllAcceptedCredentials"_s, false });
    capabilities.append({ "signalCurrentUserDetails"_s, false });
    capabilities.append({ "signalUnknownCredential"_s, false });
    capabilities.append({ "userVerifyingPlatformAuthenticator"_s, false });
    handler(WTF::move(capabilities));
}

void WebAuthenticatorCoordinatorProxy::signalUnknownCredential(const SecurityOriginData&, UnknownCredentialOptions&&, CompletionHandler<void(std::optional<ExceptionData>)>&& handler)
{
    handler(ExceptionData { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void WebAuthenticatorCoordinatorProxy::signalAllAcceptedCredentials(const SecurityOriginData&, AllAcceptedCredentialsOptions&&, CompletionHandler<void(std::optional<ExceptionData>)>&& handler)
{
    handler(ExceptionData { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void WebAuthenticatorCoordinatorProxy::signalCurrentUserDetails(const SecurityOriginData&, CurrentUserDetailsOptions&&, CompletionHandler<void(std::optional<ExceptionData>)>&& handler)
{
    handler(ExceptionData { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
