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

    init(mockConfiguration: WebCore.MockPhoneNumberPossessionConfiguration) {
        base = Base(manager: MockPhoneNumberPossessionManager(configuration: mockConfiguration))
    }

    // FIXME: rdar://191032757 Each task below captures completionHandler explicitly, because the
    // Swift 6.4 optimizer crashes when a closure captures a sending C++ reference implicitly.
    // Drop completionHandler from the capture lists once that's fixed.
    func canRequest(completionHandler: sending WebKit.PhoneNumberPossessionCanRequestCompletionHandler) {
        Task.immediate { @MainActor [base, completionHandler] in
            completionHandler.pointee(await base.canRequest())
        }
    }

    func request(
        _ request: sending WebKit.RefCountedPhoneNumberPossessionRequest,
        origin: sending WTF.String,
        presentationAnchor: PhoneNumberPossessionPresentationAnchor?,
        completionHandler: sending WebKit.PhoneNumberPossessionRequestCompletionHandler
    ) {
        Task.immediate { @MainActor [base, completionHandler] in
            let result = await WebKit.PhoneNumberPossessionResult { () async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) in
                try await base.request(request, origin: origin, presentationAnchor: presentationAnchor)
            }

            completionHandler.pointee(result)
        }
    }

    func cancel(completionHandler: sending WebKit.PhoneNumberPossessionCancelCompletionHandler) {
        Task.immediate { @MainActor [base, completionHandler] in
            await base.cancel()
            completionHandler.pointee()
        }
    }
}

// MARK: WebKit.PhoneNumberPossessionResult + CxxInitializableExpected

extension WebKit.PhoneNumberPossessionResult: CxxInitializableExpected {
    init(value: Value) {
        self = WTF.makeExpected(value)
    }

    init(unexpected error: Failure) {
        self = WTF.makeUnexpected(error)
    }
}

// MARK: PhoneNumberPossessionController.Base

extension PhoneNumberPossessionController {
    @MainActor
    fileprivate final class Base {
        private static let logger = Logger(subsystem: "com.apple.WebKit", category: "PhoneNumberPossession")

        private let manager: any PhoneNumberPossessionManaging

        private var requestTask: Task<Void, Never>?

        // Task requires a Sendable result, and the response isn't one, so the request task stores
        // its result here instead. The task runs on the main actor, so the response never leaves it.
        private var requestResult: Result<WebCore.PhoneNumberPossessionResponse, CxxUnexpected<WebKit.PhoneNumberPossessionError>>?

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

        func request(
            _ request: sending WebKit.RefCountedPhoneNumberPossessionRequest,
            origin: sending WTF.String,
            presentationAnchor: PhoneNumberPossessionPresentationAnchor?
        ) async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) -> WebCore.PhoneNumberPossessionResponse {
            if hasStartedRequest {
                assertionFailure("PhoneNumberPossessionController request called more than once on a single-use controller")
                throw CxxUnexpected(error: .Failed)
            }

            hasStartedRequest = true

            guard !isCancelled else {
                throw CxxUnexpected(error: .Cancelled)
            }

            guard let presentationAnchor else {
                Self.logger.error("PhoneNumberPossessionController has no window to present a request in")
                throw CxxUnexpected(error: .Failed)
            }

            // Task.immediate runs inline up to the manager call, so a cancel can't arrive before it starts.
            let task = Task.immediate {
                requestResult = await Result { () async throws(CxxUnexpected<WebKit.PhoneNumberPossessionError>) in
                    try await manager.requestPhoneNumberPossession(
                        request: request,
                        origin: origin,
                        presentationAnchor: presentationAnchor
                    )
                }
            }

            requestTask = task
            await task.value

            guard let requestResult else {
                assertionFailure("PhoneNumberPossessionController request task finished without a result")
                throw CxxUnexpected(error: .Failed)
            }

            return try requestResult.get()
        }

        func cancel() async {
            isCancelled = true
            requestTask?.cancel()
            await requestTask?.value
        }
    }
}

#endif // ENABLE_PHONE_NUMBER_POSSESSION
