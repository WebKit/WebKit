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

#if ENABLE_WEB_AUTHN && HAVE_IDENTITY_DOCUMENT_SERVICES_SYSTEM_PRESENTMENT

import Foundation
import IdentityDocumentServices
@_weakLinked @_spi(Private) import IdentityDocumentServicesUI
import os

private let isoMdocProtocol = "org.iso.mdoc"

private let logger = Logger(subsystem: "com.apple.WebKit", category: "DigitalCredentials")

private func platformRawRequest(_ rawRequest: WKIdentityDocumentPresentmentRawRequest) -> IdentityDocumentWebPresentmentRawRequest? {
    if rawRequest.requestProtocol == isoMdocProtocol {
        return IdentityDocumentWebPresentmentRawRequest(requestType: .iso18013MobileDocument, requestData: rawRequest.requestData)
    }
    #if HAVE_DIGITAL_CREDENTIALS_OPENID4VP
    return wkPlatformOpenID4VPRawRequest(requestType: rawRequest.requestProtocol, requestData: rawRequest.requestData)
    #else
    return nil
    #endif
}

private func webKitError(_ error: any Error) -> WKIdentityDocumentPresentmentError {
    if let error = error as? WKIdentityDocumentPresentmentError {
        return error
    }
    guard let error = error as? IdentityDocumentPresentmentError else {
        return error is CancellationError ? WKIdentityDocumentPresentmentError(.cancelled) : WKIdentityDocumentPresentmentError(.unknown)
    }
    let userInfo = [NSDebugDescriptionErrorKey: error.debugDescription]
    switch error {
    case .invalidRequest:
        return WKIdentityDocumentPresentmentError(.invalidRequest, userInfo: userInfo)
    case .requestInProgress:
        return WKIdentityDocumentPresentmentError(.requestInProgress, userInfo: userInfo)
    case .cancelled:
        return WKIdentityDocumentPresentmentError(.cancelled, userInfo: userInfo)
    case .notEntitled:
        return WKIdentityDocumentPresentmentError(.notEntitled, userInfo: userInfo)
    default:
        return WKIdentityDocumentPresentmentError(.unknown, userInfo: userInfo)
    }
}

@objc
@implementation
extension WKIdentityDocumentSystemPresentmentController {
    @nonobjc
    private var task: Task<Void, Never>?

    @objc(performRawRequests:origin:completionHandler:)
    func perform(
        _ rawRequests: [WKIdentityDocumentPresentmentRawRequest],
        origin: URL,
        completionHandler: @escaping @MainActor (WKIdentityDocumentPresentmentResponse?, (any Error)?) -> Void
    ) {
        task = Task { @MainActor in
            do {
                completionHandler(try await Self.perform(rawRequests, origin: origin), nil)
            } catch {
                completionHandler(nil, webKitError(error))
            }
        }
    }

    @objc
    func cancelRequest() {
        task?.cancel()
        task = nil
    }

    private static func perform(
        _ rawRequests: [WKIdentityDocumentPresentmentRawRequest],
        origin: URL
    ) async throws -> WKIdentityDocumentPresentmentResponse {
        guard #_hasSymbol(IdentityDocumentWebPresentmentSystemController.self) else {
            throw WKIdentityDocumentPresentmentError(.notEntitled)
        }
        let platformRawRequests = rawRequests.compactMap(platformRawRequest)
        guard !platformRawRequests.isEmpty, platformRawRequests.count == rawRequests.count else {
            throw WKIdentityDocumentPresentmentError(.invalidRequest)
        }

        let response = try await IdentityDocumentWebPresentmentSystemController.performRequests(platformRawRequests, origin: origin)
        if let response = response as? ISO18013MobileDocumentResponse {
            return .init(protocolString: isoMdocProtocol, responseData: response.responseData)
        }
        #if HAVE_DIGITAL_CREDENTIALS_OPENID4VP
        if let (exchangeProtocol, responseData) = wkPlatformOpenID4VPResponse(from: response) {
            return .init(protocolString: exchangeProtocol, responseData: responseData)
        }
        #endif
        logger.error("WKIdentityDocumentSystemPresentmentController received a response of an unrecognized type")
        throw WKIdentityDocumentPresentmentError(.invalidRequest)
    }
}

#endif
