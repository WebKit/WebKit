// Copyright (C) 2026 Apple Inc. All rights reserved.
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

import Foundation

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery {
    let type: String
    let values: [String]

    init(type: String, values: [String]) {
        self.type = type
        self.values = values
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent {
    let key: String?
    let index: NSNumber?

    init(key: String?, index: NSNumber?) {
        self.key = key
        self.index = index
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPClaimValue {
    let stringValue: String?
    let integerValue: NSNumber?
    let booleanValue: NSNumber?

    init(stringValue: String?, integerValue: NSNumber?, booleanValue: NSNumber?) {
        self.stringValue = stringValue
        self.integerValue = integerValue
        self.booleanValue = booleanValue
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPClaimsQuery {
    let identifier: String?
    let path: [WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent]
    let values: [WKIdentityDocumentPresentmentOpenID4VPClaimValue]
    let intentToRetain: NSNumber?

    init(
        identifier: String?,
        path: [WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent],
        values: [WKIdentityDocumentPresentmentOpenID4VPClaimValue],
        intentToRetain: NSNumber?
    ) {
        self.identifier = identifier
        self.path = path
        self.values = values
        self.intentToRetain = intentToRetain
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPCredentialQuery {
    let identifier: String
    let format: String?
    let allowsMultiple: Bool
    let documentType: String?
    let verifiableCredentialTypes: [String]
    let trustedAuthorities: [WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery]
    let requiresCryptographicHolderBinding: Bool
    let claims: [WKIdentityDocumentPresentmentOpenID4VPClaimsQuery]
    let claimSets: [[String]]

    init(
        identifier: String,
        format: String?,
        allowsMultiple: Bool,
        documentType: String?,
        verifiableCredentialTypes: [String],
        trustedAuthorities: [WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery],
        requiresCryptographicHolderBinding: Bool,
        claims: [WKIdentityDocumentPresentmentOpenID4VPClaimsQuery],
        claimSets: [[String]]
    ) {
        self.identifier = identifier
        self.format = format
        self.allowsMultiple = allowsMultiple
        self.documentType = documentType
        self.verifiableCredentialTypes = verifiableCredentialTypes
        self.trustedAuthorities = trustedAuthorities
        self.requiresCryptographicHolderBinding = requiresCryptographicHolderBinding
        self.claims = claims
        self.claimSets = claimSets
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery {
    let options: [[String]]
    let isRequired: Bool

    init(options: [[String]], isRequired: Bool) {
        self.options = options
        self.isRequired = isRequired
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity {
    let clientIdentifierPrefix: String?
    let identifier: String
    let certificateChain: [WKIdentityDocumentPresentmentRequestAuthenticationCertificate]

    init(
        clientIdentifierPrefix: String?,
        identifier: String,
        certificateChain: [WKIdentityDocumentPresentmentRequestAuthenticationCertificate]
    ) {
        self.clientIdentifierPrefix = clientIdentifierPrefix
        self.identifier = identifier
        self.certificateChain = certificateChain
    }
}

@objc
@implementation
extension WKIdentityDocumentPresentmentOpenID4VPRequest {
    let requestType: String
    let credentials: [WKIdentityDocumentPresentmentOpenID4VPCredentialQuery]
    let credentialSets: [WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery]
    let verifierIdentities: [WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity]

    init(
        requestType: String,
        credentials: [WKIdentityDocumentPresentmentOpenID4VPCredentialQuery],
        credentialSets: [WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery],
        verifierIdentities: [WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity]
    ) {
        self.requestType = requestType
        self.credentials = credentials
        self.credentialSets = credentialSets
        self.verifierIdentities = verifierIdentities
    }
}

#endif
