/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RelatedOriginsValidator.h"

#if ENABLE(WEB_AUTHN)

#include "Logging.h"
#include "WellKnownResourceFetcher.h"
#include <WebCore/WellKnownOriginList.h>

namespace WebKit {

static constexpr auto webAuthnWellKnownPath = "/.well-known/webauthn"_s;
static constexpr auto webAuthnOriginsMember = "origins"_s;

void RelatedOriginsValidation::validate(WebPageProxy& page, const WebCore::SecurityOriginData& callerOrigin, const String& relyingPartyIdentifier, CompletionHandler<void(Result&&)>&& completionHandler)
{
    auto url = WebCore::wellKnownURL(relyingPartyIdentifier, webAuthnWellKnownPath);
    if (!url.isValid()) {
        RELEASE_LOG_ERROR(WebAuthn, "Related origins: relying party identifier is not a host.");
        completionHandler({ });
        return;
    }

    WellKnownResourceFetcher::fetch(page, WTF::move(url), { }, [callerOrigin, completionHandler = WTF::move(completionHandler)](WellKnownFetchResult&& result) mutable {
        if (!result.succeeded()) {
            RELEASE_LOG_ERROR(WebAuthn, "Related origins: fetch failed (%" PUBLIC_LOG_STRING ").", wellKnownFetchStatusDescription(result.status).characters());
            completionHandler({ });
            return;
        }

        auto origins = WebCore::parseOriginsFromWellKnownList(result.body.span(), webAuthnOriginsMember);
        auto found = WebCore::findOriginInWellKnownList(callerOrigin, result.body.span(), webAuthnOriginsMember);
        if (found == WebCore::WellKnownOriginListResult::Malformed)
            RELEASE_LOG_ERROR(WebAuthn, "Related origins: well-known resource is malformed.");

        completionHandler({ found == WebCore::WellKnownOriginListResult::Found, WTF::move(origins) });
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
