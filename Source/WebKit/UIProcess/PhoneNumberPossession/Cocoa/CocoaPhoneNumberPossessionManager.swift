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

#if ENABLE_PHONE_NUMBER_POSSESSION

import Foundation
import WebCore_Private
import WebKit_Internal
import os
import wtf

#if WTF_PLATFORM_IOS_FAMILY
import UIKit
#else
import AppKit
#endif

final class CocoaPhoneNumberPossessionManager: PhoneNumberPossessionManaging {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "PhoneNumberPossession")

    private let platformManager: any WKPhoneNumberPossessionPlatformManaging

    nonisolated init(platformManager: any WKPhoneNumberPossessionPlatformManaging) {
        self.platformManager = platformManager
    }

    var canRequestPhoneNumberPossession: Bool {
        get async { await platformManager.canRequest() }
    }

    func requestPhoneNumberPossession(
        request: WebKit.RefCountedPhoneNumberPossessionRequest,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor
    ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
        #if WTF_PLATFORM_IOS_FAMILY
        // A window without a scene can't present the request.
        guard presentationAnchor.windowScene != nil else {
            throw CxxUnexpected(error: .Failed)
        }
        #endif

        guard await platformManager.canRequest() else {
            throw CxxUnexpected(error: .Unavailable)
        }

        let phoneNumberSuffixHint = request.pointee.phoneNumberSuffixHint
        do {
            let response = try await platformManager.request(
                relyingPartyIdentifier: request.pointee.relyingPartyIdentifier.description,
                challenge: phoneNumberPossessionChallenge(request),
                includesPhoneNumber: request.pointee.desiredResponseType == .PossessionIdentifierAndPhoneNumber,
                phoneNumberSuffixHint: phoneNumberSuffixHint.isNull() ? nil : phoneNumberSuffixHint.description,
                presentationAnchor: presentationAnchor
            )
            try Task.checkCancellation()
            return WebCore.PhoneNumberPossessionResponse(
                encryptedResult: response.encryptedResult,
                carrierDomain: response.carrierDomain,
                carrierConfigurationID: response.carrierConfigurationID,
                encryptedDecryptionKey: response.encryptedDecryptionKey,
                clientDataJSON: response.clientDataJSON
            )
        } catch {
            if Task.isCancelled {
                throw CxxUnexpected(error: .Cancelled)
            }

            let nsError = error as NSError
            Self.logger.error(
                "Phone number possession request failed: \(nsError.domain, privacy: .public) \(nsError.code, privacy: .public)"
            )
            throw CxxUnexpected(error: .Failed)
        }
    }
}

#endif // ENABLE_PHONE_NUMBER_POSSESSION
