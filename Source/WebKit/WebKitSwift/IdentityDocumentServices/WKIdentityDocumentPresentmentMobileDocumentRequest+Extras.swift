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

extension WKIdentityDocumentPresentmentMobileDocumentElementInfo {
    convenience init(_ source: ISO18013MobileDocumentRequest.ElementInfo) {
        self.init(isRetaining: source.isRetaining)
    }
}

extension WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest.DocumentRequest) {
        self.init(
            documentType: source.documentType,
            namespaces: source.namespaces.mapValues {
                $0.mapValues(WKIdentityDocumentPresentmentMobileDocumentElementInfo.init(_:))
            }
        )
    }
}

extension WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest.PresentmentRequest) {
        self.init(
            documentSets: source.documentRequestSets.map {
                $0.requests.map(WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest.init(_:))
            },
            isMandatory: source.isMandatory
        )
    }
}

extension WKIdentityDocumentPresentmentMobileDocumentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest) {
        self.init(
            presentmentRequests: source.presentmentRequests.map(WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest.init(_:)),
            authenticationCertificates: source.requestAuthentications.map {
                $0.authenticationCertificateChain.map(WKIdentityDocumentPresentmentRequestAuthenticationCertificate.init(certificate:))
            }
        )
    }
}

#endif
