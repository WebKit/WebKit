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

#if HAVE_CREDENTIAL_UPDATE_API

import Foundation

@_spi(Private) internal import AuthenticationServices

@objc
@implementation
extension CredentialUpdaterShim {
    class func signalUnknownCredential(withRelyingPartyIdentifier relyingPartyIdentifier: String, credentialID: Data) async throws {
        try await ASCredentialUpdater()
            .reportUnknownPublicKeyCredential(relyingPartyIdentifier: relyingPartyIdentifier, credentialID: credentialID)
    }

    class func signalAllAcceptedCredentials(
        withRelyingPartyIdentifier relyingPartyIdentifier: String,
        userHandle: Data,
        acceptedCredentialIDs: [Data]
    ) async throws {
        try await ASCredentialUpdater()
            .reportAllAcceptedPublicKeyCredentials(
                relyingPartyIdentifier: relyingPartyIdentifier,
                userHandle: userHandle,
                acceptedCredentialIDs: acceptedCredentialIDs
            )
    }

    class func signalCurrentUserDetails(
        withRelyingPartyIdentifier relyingPartyIdentifier: String,
        userHandle: Data,
        newName: String
    ) async throws {
        try await ASCredentialUpdater()
            .reportPublicKeyCredentialUpdate(relyingPartyIdentifier: relyingPartyIdentifier, userHandle: userHandle, newName: newName)
    }
}

#endif
