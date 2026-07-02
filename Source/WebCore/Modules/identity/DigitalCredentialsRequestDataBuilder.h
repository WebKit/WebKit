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

#pragma once

#include "DigitalCredentialsRequestData.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "ISO18013DocumentRequest.h"

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
#include "DigitalCredentialsMobileDocumentRequestDataWithRequestInfo.h"
#include "ISO18013DocumentRequestInfo.h"
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

#include "SecurityOriginData.h"
#include "ValidatedMobileDocumentRequest.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class DigitalCredentialsRequestDataBuilder {

public:
    static ExceptionOr<std::pair<DigitalCredentialsRequestData, DigitalCredentialsRawRequests>> build(Vector<ValidatedMobileDocumentRequest>, const Document&, Vector<UnvalidatedDigitalCredentialRequest>&&);

private:
#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

    static ExceptionOr<std::pair<DigitalCredentialsMobileDocumentRequestDataWithRequestInfo, RawDigitalCredentialsWithRequestInfo>> buildAndValidateRequestDataWithRequestInfo(const ISO18013DocumentRequest&, const Document&);

    static ExceptionOr<std::pair<String, RawDigitalCredentialsWithRequestInfo>> parseMatchingHintAndRawRequests(const ISO18013DocumentRequest&);

    static ExceptionOr<RawDigitalCredentialsWithRequestInfo> parseRawRequests(const HashMap<String, Box<ISO18013Any>>&);

    static ExceptionOr<String> parseMatchingHint(const HashMap<String, Box<ISO18013Any>>&);

    static ExceptionOr<ResponseType> parseRequestedDataElements(const ISO18013DocumentRequest&);

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/DigitalCredentialsRequestDataBuilderAdditions.h>)
// FIXME: Properly support using WKA in modules.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-modular-include-in-module"
#include <WebKitAdditions/DigitalCredentialsRequestDataBuilderAdditions.cpp>
#pragma clang diagnostic pop
#else
    static constexpr ASCIILiteral ISO18013RequestInfoDocType = "org.iso.mdoc.requestInfo";
    static constexpr ASCIILiteral requestInfoNamespace = "mdoc.requestInfo"_s;

    static constexpr ASCIILiteral attestationElementIdentifier = "attestation"_s;
    static constexpr ASCIILiteral disclosureElementIdentifier = "disclosure"_s;

    static constexpr ASCIILiteral rawRequestKey = "rawRequest"_s;
    static constexpr ASCIILiteral matchingHintKey = "matchingHint"_s;
#endif

#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
};

} // namespace WebCore
