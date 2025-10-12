// Copyright (C) 2025 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if HAVE_DIGITAL_CREDENTIALS_UI

internal import IdentityDocumentServices
internal import IdentityDocumentServicesUI

extension ISO18013MobileDocumentRequest.ElementInfo {
    init(_ source: WKIdentityDocumentPresentmentMobileDocumentElementInfo) {
        self = .init(isRetaining: source.isRetaining)
    }
}

extension ISO18013MobileDocumentRequest.DocumentRequest {
    init(_ source: WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest) {
        self = .init(
            documentType: source.documentType,
            namespaces: source.namespaces.mapValues {
                $0.mapValues(ISO18013MobileDocumentRequest.ElementInfo.init(_:))
            }
        )
    }
}

extension ISO18013MobileDocumentRequest.PresentmentRequest {
    init(_ source: WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest) {
        self = .init(
            documentRequestSets: source.documentSets.map {
                ISO18013MobileDocumentRequest.DocumentRequestSet(
                    requests: $0.map(ISO18013MobileDocumentRequest.DocumentRequest.init(_:))
                )
            },
            isMandatory: source.isMandatory
        )
    }
}

extension ISO18013MobileDocumentRequest {
    init(_ source: WKIdentityDocumentPresentmentMobileDocumentRequest) {
        let presentmentRequests = source.presentmentRequests.map(ISO18013MobileDocumentRequest.PresentmentRequest.init(_:))

        let requestAuthentications = source.authenticationCertificates.map {
            RequestAuthentication(authenticationCertificateChain: $0.map(\.certificate))
        }

        self.init(
            presentmentRequests: presentmentRequests,
            requestAuthentications: requestAuthentications
        )
    }
}

#endif
