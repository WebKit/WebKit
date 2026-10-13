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
    // same order, so that a raw request can be released by its position.
    Vector<ValidatedMobileDocumentRequest> mobileDocumentRequests;
    Vector<UnvalidatedDigitalCredentialRequest> mobileDocumentRawRequests;
    Vector<ValidatedOpenID4VPRequest> openID4VPRequests;
    Vector<UnvalidatedDigitalCredentialRequest> openID4VPRawRequests;

    for (size_t index = 0; index < validatedCredentialRequests.size(); ++index) {
        auto& validatedRequest = validatedCredentialRequests[index];
        if (!validatedRequest)
            continue;

        WTF::switchOn(*validatedRequest,
            [&](ValidatedMobileDocumentRequest& request) {
                mobileDocumentRequests.append(WTF::move(request));
                mobileDocumentRawRequests.append(WTF::move(unvalidatedRequests[index]));
            },
            [&](ValidatedOpenID4VPRequest& request) {
                if (!openID4VPRequests.isEmpty() && openID4VPRequests.first().protocol != request.protocol) {
                    LOG(DigitalCredentials, "DigitalCredentialsRequestDataBuilder::build() - not presenting an OpenID4VP request of a second protocol.");
                    return;
                }
                if (openID4VPRequests.size() == maxOpenID4VPRequestCount) {
                    LOG(DigitalCredentials, "DigitalCredentialsRequestDataBuilder::build() - not presenting OpenID4VP requests beyond the limit.");
                    return;
                }
                openID4VPRequests.append(WTF::move(request));
                openID4VPRawRequests.append(WTF::move(unvalidatedRequests[index]));
            });
    }

    if (mobileDocumentRequests.isEmpty()) {
        return std::make_pair(
            DigitalCredentialsRequestData {
                DigitalCredentialsOpenID4VPRequestData {
                    origins,
                    WTF::move(openID4VPRequests) } },
            DigitalCredentialsRawRequests { WTF::move(openID4VPRawRequests) });
    }

    if (!openID4VPRequests.isEmpty())
        LOG(DigitalCredentials, "DigitalCredentialsRequestDataBuilder::build() - mdoc requests present; %zu OpenID4VP request(s) will not be presented.", openID4VPRequests.size());

    return std::make_pair(
        DigitalCredentialsRequestData {
            DigitalCredentialsMobileDocumentRequestData {
                origins,
                WTF::move(mobileDocumentRequests) } },
        DigitalCredentialsRawRequests { WTF::move(mobileDocumentRawRequests) });
}

} // namespace WebCore
