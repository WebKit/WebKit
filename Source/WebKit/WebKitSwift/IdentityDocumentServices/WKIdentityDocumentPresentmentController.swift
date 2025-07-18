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
import os

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

#if canImport(UIKit)
private typealias IdentityDocumentPresentationAnchor = UIWindow
#else
private typealias IdentityDocumentPresentationAnchor = NSWindow
#endif

// MARK: WKIdentityDocumentPresentmentController.Base

extension WKIdentityDocumentPresentmentController {
    @MainActor
    fileprivate final class Base {
        private static let isoMdocProtocol = "org.iso.mdoc"

        private static let logger = Logger(subsystem: "com.apple.WebKit", category: "DigitalCredentials")

        private let controller: IdentityDocumentWebPresentmentController

        private var performRequestTask: Task<IdentityDocumentWebPresentmentResponse, Error>?

        weak var delegate: (any WKIdentityDocumentPresentmentDelegate)?

        init() {
            self.controller = IdentityDocumentWebPresentmentController()
            self.controller.presentationContextProvider = self
            self.controller.delegate = self
        }

        func perform(request: WKIdentityDocumentPresentmentRequest) async throws -> WKIdentityDocumentPresentmentResponse {
            do {
                Self.logger.debug("IdentityDocumentPresentmentController performRequest called with request \(String(describing: request))")
                let convertedRequests = request.mobileDocumentRequests.map(ISO18013MobileDocumentRequest.init(_:))

                Self.logger.debug("IdentityDocumentPresentmentController build converted request \(String(describing: convertedRequests))")

                let task = Task {
                    return try await controller.performRequests(convertedRequests, origin: request.origin)
                }

                performRequestTask = task
                let response = try await task.value

                guard let response = response as? ISO18013MobileDocumentResponse else {
                    Self.logger.error("IdentityDocumentPresentmentController unexpectedly received a response that is not of type ISO18013MobileDocumentResponse")
                    throw WKIdentityDocumentPresentmentError(.invalidRequest)
                }

                return .init(protocolString: Self.isoMdocProtocol, responseData: response.responseData)
            } catch let error as IdentityDocumentPresentmentError {
                let userInfo = [NSDebugDescriptionErrorKey: error.debugDescription]

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
                throw WKIdentityDocumentPresentmentError(.unknown)
            }
        }

        func cancelRequest() {
            performRequestTask?.cancel()
        }
    }
}

// MARK: WKIdentityDocumentPresentmentController.Base  IdentityDocumentPresentmentControllerPresentationContextProviding & IdentityDocumentWebPresentmentControllerDelegate

extension WKIdentityDocumentPresentmentController.Base: IdentityDocumentPresentmentControllerPresentationContextProviding, IdentityDocumentWebPresentmentControllerDelegate {
    func presentationAnchorForPresentmentController(_ presentmentController: any IdentityDocumentPresentmentControlling) -> IdentityDocumentPresentationAnchor? {
        delegate?.presentationAnchor()
    }

    func rawRequestsForWebPresentmentController(_ webPresentmentController: IdentityDocumentWebPresentmentController) async -> [IdentityDocumentWebPresentmentRawRequest] {
        guard let rawRequests = await delegate?.fetchRawRequests() else {
            Self.logger.error("IdentityDocumentPresentmentController delegate is not implemented, sending no raw requests")
            return []
        }

        return rawRequests.compactMap { rawRequest in
            switch rawRequest.requestProtocol {
            case Self.isoMdocProtocol:
                return IdentityDocumentWebPresentmentRawRequest(requestType: .iso18013MobileDocument, requestData: rawRequest.requestData)

            default:
                Self.logger.debug("IdentityDocumentPresentmentController raw request call back encountered a non-ISO request. Skipping")
                return nil
            }
        }
    }
}

// MARK: WKIdentityDocumentPresentmentController

@objc @implementation extension WKIdentityDocumentPresentmentController {
    @nonobjc private let base = Base()

    weak var delegate: (any WKIdentityDocumentPresentmentDelegate)? {
        get { base.delegate }
        set { base.delegate = newValue }
    }

    @objc(performRequest:completionHandler:)
    func perform(_ request: WKIdentityDocumentPresentmentRequest) async throws -> WKIdentityDocumentPresentmentResponse {
        try await base.perform(request: request)
    }

    func cancelRequest() {
        base.cancelRequest()
    }
}

#endif
