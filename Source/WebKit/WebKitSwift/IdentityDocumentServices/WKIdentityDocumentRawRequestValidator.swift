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
import os

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

@objc @implementation extension WKISO18013Request {
    var encryptionInfo: String?
    var deviceRequest: String?

    init?(encryptionInfo: String?, deviceRequest: String?) {
        self.encryptionInfo = encryptionInfo
        self.deviceRequest = deviceRequest
    }
}

@objc @implementation extension WKIdentityDocumentRawRequestValidator {
    private static let isoMdocProtocol = "org.iso.mdoc"

    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "DigitalCredentials")

    // Used to workaround the fact that `@objc @implementation does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc private let _unsafeValidator = IdentityDocumentWebPresentmentRawRequestValidator() as Any

    @nonobjc private final var validator: IdentityDocumentWebPresentmentRawRequestValidator {
        _unsafeValidator as! IdentityDocumentWebPresentmentRawRequestValidator
    }

    @objc(validateISO18013Request:origin:error:)
    func validate(_ iso18013Request: WKISO18013Request, origin: URL) throws -> WKIdentityDocumentPresentmentMobileDocumentRequest {
        do {
            let requestToEncode = [
                "deviceRequest": iso18013Request.deviceRequest!,
                "encryptionInfo": iso18013Request.encryptionInfo!
            ]

            let encodedRequestData = try JSONSerialization.data(withJSONObject: requestToEncode)

            let validatedResponse = try validator.validateISO18013MobileDocumentRequest(encodedRequestData, origin: origin)

            return .init(validatedResponse)
        } catch let error as IdentityDocumentPresentmentError {
            let userInfo = [NSDebugDescriptionErrorKey: error.debugDescription]

            Self.logger.debug(
                """
                WKIdentityDocumentRawRequestValidator encountered IdentityDocumentPresentmentError \
                while validating request \(String(describing: iso18013Request)). \
                Error: \(error)
                """
            )

            switch error {
            case .unknown:
                throw WKIdentityDocumentPresentmentError(.unknown, userInfo: userInfo)
            case .invalidRequest:
                throw WKIdentityDocumentPresentmentError(.invalidRequest, userInfo: userInfo)
            case .requestInProgress:
                throw WKIdentityDocumentPresentmentError(.requestInProgress, userInfo: userInfo)
            case .cancelled:
                throw WKIdentityDocumentPresentmentError(.cancelled, userInfo: userInfo)
            case .notEntitled:
                throw WKIdentityDocumentPresentmentError(.notEntitled, userInfo: userInfo)
            default:
                throw WKIdentityDocumentPresentmentError(.unknown, userInfo: userInfo)
            }
        } catch {
            Self.logger.debug("WKIdentityDocumentRawRequestValidator encountered error while validating request \(String(describing: iso18013Request)). Error: \(error)")

            throw WKIdentityDocumentPresentmentError(.invalidRequest)
        }
    }

}
#endif
