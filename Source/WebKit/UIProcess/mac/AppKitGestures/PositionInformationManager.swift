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

#if HAVE_APPKIT_GESTURES_SUPPORT

import WebKit_Internal

// FIXME: Share this logic with iOS.

// Owns the asynchronous hit-test "position information" cache and the queue of completions waiting
// on it. A completion registered via doAfterUpdate(_:completion:) is guaranteed to run exactly once: either with
// the resolved information, or with nil when the request is abandoned (the press ended
// before the reply arrived, the page went away, or the manager is reset / destroyed).
@MainActor
final class PositionInformationManager {
    typealias UpdateCompletionHandler = (WebKit.InteractionInformationAtPosition?) -> Void

    private struct RequestAndCallback {
        let request: WebKit.InteractionInformationRequest
        let callback: UpdateCompletionHandler
    }

    // The web view's page answers the requests. The view gets its page after the controller makes this manager.
    private weak var view: WKWebView?
    private(set) var currentInformation = WebKit.InteractionInformationAtPosition()
    private var lastOutstandingRequest: WebKit.InteractionInformationRequest?
    private var pendingHandlers: [RequestAndCallback?] = []
    private var isInvokingPendingHandlers = false
    private(set) var hasValidCurrentInformation = false

    init(view: WKWebView) {
        self.view = view
    }

    isolated deinit {
        // Guarantee every still-queued callback runs exactly once, even when the manager is torn down
        // with requests in flight.
        invokeAndRemovePendingHandlers(matching: { _ in true }, with: nil)
    }

    // Runs the completion handler once the cache holds information valid for the request. If the cache is already
    // valid the callback runs synchronously; otherwise it is queued and an asynchronous update is
    // requested (coalesced with any in-flight request that would also satisfy this one).
    func doAfterUpdate(_ request: WebKit.InteractionInformationRequest, completion: @escaping UpdateCompletionHandler) {
        if currentIsValid(request) {
            completion(currentInformation)
            return
        }

        pendingHandlers.append(RequestAndCallback(request: request, callback: completion))

        if !hasValidOutstandingRequest(request) {
            requestUpdate(request)
        }
    }

    // Called when fresh information arrives from the web process; resolves all queued callbacks that
    // the new information satisfies.
    func didChange(_ information: WebKit.InteractionInformationAtPosition) {
        if let lastOutstandingRequest, information.request.isValidForRequest(lastOutstandingRequest, 0) {
            self.lastOutstandingRequest = nil
        }

        var newInformation = information
        newInformation.mergeCompatibleOptionalInformation(currentInformation)

        currentInformation = newInformation
        hasValidCurrentInformation = currentInformation.canBeValid

        // Only the callbacks whose request the newly-arrived information satisfies should resolve now.
        invokeAndRemovePendingHandlers(matching: currentIsValid, with: currentInformation)
    }

    // Drops the cached information (e.g. on a main-frame load commit). Does not touch queued callbacks.
    func invalidate() {
        hasValidCurrentInformation = false
        currentInformation = WebKit.InteractionInformationAtPosition()
    }

    // Full teardown: invalidates the cache, forgets any in-flight request, and resolves every queued
    // callback with nil.
    func reset() {
        invalidate()
        lastOutstandingRequest = nil
        invokeAndRemovePendingHandlers(matching: { _ in true }, with: nil)
    }

    // Abandons the in-flight request (if any) and resolves the queued callbacks waiting on it with
    // nil. Used when a deferring gesture's press ends before the reply arrives.
    func abandonOutstandingRequest() {
        guard let abandonedRequest = lastOutstandingRequest.take() else {
            return
        }

        invokeAndRemovePendingHandlers(matching: { $0.isValidForRequest(abandonedRequest, 0) }, with: nil)
    }

    func currentIsApproximatelyValid(_ request: WebKit.InteractionInformationRequest, radius: Int32) -> Bool {
        hasValidCurrentInformation && currentInformation.request.isApproximatelyValidForRequest(request, radius)
    }

    private func currentIsValid(_ request: WebKit.InteractionInformationRequest) -> Bool {
        hasValidCurrentInformation && currentInformation.request.isValidForRequest(request, 0)
    }

    private func requestUpdate(_ request: WebKit.InteractionInformationRequest) {
        if currentIsValid(request) {
            return
        }

        guard let page = view?._protectedPage().get(), page.hasRunningProcess() else {
            return
        }

        lastOutstandingRequest = request
        page.requestPositionInformation(request)
    }

    private func hasValidOutstandingRequest(_ request: WebKit.InteractionInformationRequest) -> Bool {
        guard let lastOutstandingRequest else {
            return false
        }
        return lastOutstandingRequest.isValidForRequest(request, 0)
    }

    // Invokes (exactly once) and removes every queued callback whose request satisfies `matches`,
    // passing `information` to each. Reentrancy-safe: callbacks may re-enter and mutate the queue, so
    // consumed slots are left as tombstones and only compacted once the outermost invocation unwinds.
    private func invokeAndRemovePendingHandlers(
        matching matches: (WebKit.InteractionInformationRequest) -> Bool,
        with information: WebKit.InteractionInformationAtPosition?
    ) {
        let isOutermostInvocation = !exchange(&isInvokingPendingHandlers, with: true)

        // This is intentionally not a for-in loop, which would iterate over a copy of the queue and miss the
        // callbacks that callbacks add.
        var index = 0
        while index < pendingHandlers.count {
            if let request = pendingHandlers[index]?.request, matches(request), let handler = pendingHandlers[index].take() {
                handler.callback(information)
            }
            index += 1
        }

        guard isOutermostInvocation else {
            return
        }

        isInvokingPendingHandlers = false
        pendingHandlers.removeAll(where: Self.isTombstone)
    }

    // Nonisolated, since a closure written in this main-actor class would check the actor for each slot.
    private nonisolated static func isTombstone(_ slot: RequestAndCallback?) -> Bool {
        slot == nil
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
