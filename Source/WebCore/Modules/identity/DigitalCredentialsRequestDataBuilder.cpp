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
#include "DigitalCredentialsRequestDataBuilder.h"

#include <Logging.h>
#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentSecurityOrigin.h>
#include <WebCore/ISO18013DocumentRequest.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

ExceptionOr<std::pair<DigitalCredentialsRequestData, DigitalCredentialsRawRequests>> DigitalCredentialsRequestDataBuilder::build(Vector<std::optional<ValidatedDigitalCredentialRequest>>&& validatedCredentialRequests, const Document& document, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests)
{
    if (validatedCredentialRequests.size() != unvalidatedRequests.size())
        return Exception { ExceptionCode::TypeError, "Validation did not account for every credential request."_s };

    DigitalCredentialsSecurityOriginData origins {
        .topOrigin = document.topOrigin().data(),
        .documentOrigin = document.securityOrigin().data(),
    };

    // The raw requests handed to the document provider are exactly the presented requests, in the
    // same order.
    Vector<ValidatedDigitalCredentialRequest> requests;
    Vector<UnvalidatedDigitalCredentialRequest> rawRequests;
    size_t openID4VPRequestCount = 0;

    for (size_t index = 0; index < validatedCredentialRequests.size(); ++index) {
        auto& validatedRequest = validatedCredentialRequests[index];
        if (!validatedRequest)
            continue;

        if (std::holds_alternative<ValidatedOpenID4VPRequest>(*validatedRequest)) {
            if (openID4VPRequestCount == maxOpenID4VPRequestCount) {
                LOG(DigitalCredentials, "DigitalCredentialsRequestDataBuilder::build() - not presenting OpenID4VP requests beyond the limit.");
                continue;
            }
            ++openID4VPRequestCount;
        }

        requests.append(WTF::move(*validatedRequest));
        rawRequests.append(WTF::move(unvalidatedRequests[index]));
    }

    return std::make_pair(
        DigitalCredentialsRequestData { origins, WTF::move(requests) },
        WTF::move(rawRequests));
}

} // namespace WebCore
