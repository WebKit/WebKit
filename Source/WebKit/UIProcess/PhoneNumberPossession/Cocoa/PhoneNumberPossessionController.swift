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

import WebKit_Internal
import os
import wtf

#if WTF_PLATFORM_IOS_FAMILY
import UIKit
#else
import AppKit
#endif

final class PhoneNumberPossessionController {
    private let base: Base

    init() {
        base = Base(manager: UnavailablePhoneNumberPossessionManager())
    }

    func canRequest(completionHandler: sending WebKit.PhoneNumberPossessionCanRequestCompletionHandler) {
        _ = Task.immediate { @MainActor [base] in
            completionHandler.pointee(await base.canRequest())
        }
    }

    func request(
        _ request: sending WebKit.PhoneNumberPossessionControllerRequest,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor?,
        completionHandler: sending WebKit.PhoneNumberPossessionRequestCompletionHandler
    ) {
        _ = Task.immediate { @MainActor [base] in
            base.start(request, presentationAnchor: presentationAnchor, completionHandler: completionHandler)
        }
    }

    func cancel() {
        _ = Task.immediate { @MainActor [base] in
            base.cancel()
        }
    }
}

// MARK: PhoneNumberPossessionController.Base

extension PhoneNumberPossessionController {
    @MainActor
    fileprivate final class Base {
        private static let logger = Logger(subsystem: "com.apple.WebKit", category: "PhoneNumberPossession")

        private let manager: any PhoneNumberPossessionManaging

        private var requestTask: Task<Void, Never>?

        // WebKit creates a new controller for each request, so once cancelled, it stays
        // cancelled, and a second request is a bug.
        private var isCancelled = false
        private var hasStartedRequest = false

        nonisolated init(manager: any PhoneNumberPossessionManaging) {
            self.manager = manager
        }

        func canRequest() async -> Bool {
            await manager.canRequestPhoneNumberPossession
        }

        func start(
            _ request: sending WebKit.PhoneNumberPossessionControllerRequest,
            presentationAnchor: PhoneNumberPossessionPresentationAnchor?,
            completionHandler: sending WebKit.PhoneNumberPossessionRequestCompletionHandler
        ) {
            if hasStartedRequest {
                assertionFailure("PhoneNumberPossessionController request called more than once on a single-use controller")
                completionHandler.pointee(.init(error: .Failed))
                return
            }
            hasStartedRequest = true

            requestTask = Task {
                completionHandler.pointee(await perform(request, presentationAnchor: presentationAnchor))
            }
        }

        private func perform(
            _ request: WebKit.PhoneNumberPossessionControllerRequest,
            presentationAnchor: PhoneNumberPossessionPresentationAnchor?
        ) async -> WebKit.PhoneNumberPossessionControllerResult {
            // A cancel can arrive before this starts.
            if isCancelled {
                return .init(error: .Cancelled)
            }

            guard let presentationAnchor else {
                Self.logger.error("PhoneNumberPossessionController has no window to present a request in")
                return .init(error: .Failed)
            }

            return await manager.requestPhoneNumberPossession(request: request, presentationAnchor: presentationAnchor)
        }

        func cancel() {
            isCancelled = true
            requestTask?.cancel()
        }
    }
}

extension WebKit.PhoneNumberPossessionControllerResult {
    init(error: WebKit.PhoneNumberPossessionControllerError) {
        self.init()
        self.error = error
    }
}

#endif // ENABLE_PHONE_NUMBER_POSSESSION
