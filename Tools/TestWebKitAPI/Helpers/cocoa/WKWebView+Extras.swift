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

import CoreGraphics
public import Foundation
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
public import WebKit
public import WebKit_Private
private import WebKit_Private.WKWebViewPrivate

public import struct Foundation.URL
public import struct Swift.String

/// An error thrown when evaluated JavaScript produces a value of an unexpected type.
public struct UnexpectedJavaScriptResult: Error, CustomStringConvertible {
    /// The script that was evaluated.
    public let script: String

    /// The type the caller asked the script to produce.
    public let expectedType: String

    /// A description of the value the script actually produced.
    public let actualValue: String

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        "JavaScript \"\(script)\" was expected to produce \(expectedType), but produced \(actualValue)"
    }
}

extension WKWebView {
    /// Loads markup and waits until the navigation finishes.
    ///
    /// - Parameters:
    ///   - html: The markup to load.
    ///   - baseURL: The base URL to resolve relative URLs against. Defaults to the test resources bundle.
    /// - Throws: The navigation error if the provisional load fails.
    public func load(html: String, baseURL: URL? = Bundle.testResources.resourceURL) async throws {
        loadHTMLString(html, baseURL: baseURL)
        try await _test_waitForDidFinishNavigation()
    }

    /// Loads a request and waits until the navigation finishes.
    ///
    /// - Parameter request: The request to load.
    /// - Throws: The navigation error if the provisional load fails.
    public func loadAndWait(_ request: URLRequest) async throws {
        load(request)
        try await _test_waitForDidFinishNavigation()
    }

    /// Loads a request, trusting any server certificate, and waits until the navigation finishes.
    ///
    /// - Parameter request: The request to load.
    /// - Throws: The navigation error if the provisional load fails.
    public func loadAndWaitIgnoringSSLErrors(_ request: URLRequest) async throws {
        load(request)
        try await _test_waitForDidFinishNavigationWhileIgnoringSSLErrors()
    }

    /// Loads a page from the test resources bundle and waits until the navigation finishes.
    ///
    /// - Parameter pageName: The name of the HTML resource, without its extension.
    /// - Throws: The navigation error if the provisional load fails.
    public func load(testPageNamed pageName: String) async throws {
        loadTestPageNamed(pageName)
        try await _test_waitForDidFinishNavigation()
    }

    /// Calls a JavaScript function body and returns its result as the requested type.
    ///
    /// The script is an asynchronous function body, so it may `await` and must `return` the value
    /// it produces.
    ///
    /// ```swift
    /// let clicked = try await webView.callJavaScript(returning: Bool.self) {
    ///     "return window.settingsClicked"
    /// }
    /// ```
    ///
    /// - Parameters:
    ///   - type: The type the function body is expected to return.
    ///   - frame: The frame to call the function body in, or `nil` for the main frame.
    ///   - contentWorld: The content world to call the function body in.
    ///   - arguments: The arguments to pass to the function body, available in it as local variables.
    ///   - functionBody: A closure producing the JavaScript function body to call.
    /// - Returns: The value the function body returned.
    /// - Throws: ``UnexpectedJavaScriptResult`` if the function body returned a value of a
    ///   different type, or any error raised while running it.
    public func callJavaScript<Result>(
        returning type: Result.Type,
        in frame: WKFrameInfo? = nil,
        contentWorld: WKContentWorld = .page,
        arguments: [String: Any] = [:],
        _ functionBody: () -> String
    ) async throws -> Result {
        let script = functionBody()
        let result = try await __callAsyncJavaScript(script, arguments: arguments, inFrame: frame, in: contentWorld)

        guard let value = result as? Result else {
            throw UnexpectedJavaScriptResult(
                script: script,
                expectedType: "\(Result.self)",
                actualValue: result.map { "\($0)" } ?? "nil"
            )
        }

        return value
    }

    /// Calls a JavaScript function body for its side effects, discarding anything it returns.
    ///
    /// - Parameters:
    ///   - frame: The frame to call the function body in, or `nil` for the main frame.
    ///   - contentWorld: The content world to call the function body in.
    ///   - arguments: The arguments to pass to the function body, available in it as local variables.
    ///   - functionBody: A closure producing the JavaScript function body to call.
    /// - Throws: Any error raised while running the function body.
    public func callJavaScript(
        in frame: WKFrameInfo? = nil,
        contentWorld: WKContentWorld = .page,
        arguments: [String: Any] = [:],
        _ functionBody: () -> String
    ) async throws {
        _ = try await __callAsyncJavaScript(functionBody(), arguments: arguments, inFrame: frame, in: contentWorld)
    }

    /// Evaluates a script without a user gesture, and discards the result.
    ///
    /// The `callJavaScript` functions always run with a user gesture, which changes whether a page can open windows and
    /// whether its navigations lock the history.
    ///
    /// - Parameter script: The script to evaluate.
    /// - Throws: Any error raised while evaluating the script.
    public func evaluateJavaScriptWithoutUserGesture(_ script: String) async throws {
        _ = try await _evaluateJavaScriptWithoutUserGesture(script)
    }

    /// Starts evaluating a script, and returns without waiting for it to run.
    ///
    /// Use this for a script that starts a navigation, and then wait for the navigation. Awaiting the script instead
    /// lets the navigation's delegate callbacks happen before the test starts waiting for them.
    ///
    /// - Parameters:
    ///   - script: The script to evaluate.
    ///   - frame: The frame to evaluate the script in, or `nil` for the main frame.
    ///   - userGesture: Whether the script runs with a user gesture, as scripts that `evaluateJavaScript` runs do.
    public func startEvaluatingJavaScript(_ script: String, in frame: WKFrameInfo? = nil, withUserGesture userGesture: Bool = true) {
        _evaluateJavaScript(script, withSourceURL: nil, inFrame: frame, in: .page, withUserGesture: userGesture, completionHandler: nil)
    }

    /// Creates a handle for the first element matching a selector.
    ///
    /// - Parameters:
    ///   - selector: The CSS selector to match.
    ///   - world: A content world configured to allow JS handle creation.
    ///   - frame: The frame to evaluate in, or `nil` for the main frame.
    /// - Returns: A handle for the matching element, or `nil` if there was no match.
    /// - Throws: Any error raised while evaluating the script.
    public func querySelector(
        _ selector: String,
        in world: WKContentWorld,
        frame: WKFrameInfo? = nil
    ) async throws -> _WKJSHandle? {
        let script = "window.webkit.createJSHandle(document.querySelector(`\(selector)`))"
        return try await __evaluateJavaScript(script, inFrame: frame, in: world) as? _WKJSHandle
    }

    /// The center of the first element matching a selector, in view coordinates.
    ///
    /// - Parameter selector: The CSS selector to match.
    /// - Returns: The midpoint of the element's bounding client rect.
    /// - Throws: ``UnexpectedJavaScriptResult`` if the element's rect could not be read.
    public func elementMidpoint(selector: String) async throws -> CGPoint {
        let script = """
            const r = document.querySelector('\(selector)').getBoundingClientRect();
            return [r.left, r.top, r.width, r.height];
            """
        let values = try await callJavaScript(returning: [Double].self) { script }
        guard values.count == 4 else {
            throw UnexpectedJavaScriptResult(
                script: script,
                expectedType: "an array of 4 numbers",
                actualValue: "\(values)"
            )
        }

        let rect = CGRect(x: values[0], y: values[1], width: values[2], height: values[3])
        return CGPoint(x: rect.midX, y: rect.midY)
    }

    /// Waits until the next presentation update has occurred.
    public func nextPresentationUpdate() async {
        await withCheckedContinuation { continuation in
            _do(afterNextPresentationUpdate: {
                continuation.resume()
            })
        }
    }
}

extension TestNavigationDelegate {
    /// Waits for a content rule list to perform an action on a load.
    ///
    /// - Parameter body: Work that causes the action, such as loading a page with a resource a rule blocks.
    public func nextContentRuleListAction(triggeredBy body: () -> Void) async {
        let previousCallback = contentRuleListPerformedAction
        defer { contentRuleListPerformedAction = previousCallback }

        await withCheckedContinuation { continuation in
            var resumed = false

            // One load can perform several actions before this call resumes and restores the callback.
            contentRuleListPerformedAction = { _, _, _, _ in
                guard !resumed else {
                    return
                }

                resumed = true
                continuation.resume()
            }

            body()
        }
    }

    /// Waits for the main frame's next provisional navigation to fail, such as one whose policy decision is to cancel.
    ///
    /// Start waiting before the navigation can fail: start it without waiting for its script or load call to return,
    /// such as with `evaluateJavaScript(_:completionHandler:)`. Any `didFailProvisionalNavigation` callback the test
    /// set still runs.
    ///
    /// - Returns: The navigation error.
    public func waitForDidFailProvisionalNavigation() async -> any Error {
        let previousCallback = didFailProvisionalNavigation
        defer { didFailProvisionalNavigation = previousCallback }

        return await withCheckedContinuation { continuation in
            var resumed = false
            didFailProvisionalNavigation = { webView, navigation, error in
                previousCallback?(webView, navigation, error)
                guard !resumed else {
                    return
                }

                resumed = true
                continuation.resume(returning: error)
            }
        }
    }

    /// Waits for the main frame's next navigation to commit.
    ///
    /// Any `didCommitNavigation` callback the test set still runs.
    public func waitForDidCommitNavigation() async {
        let previousCallback = didCommitNavigation
        defer { didCommitNavigation = previousCallback }

        await withCheckedContinuation { continuation in
            var resumed = false
            didCommitNavigation = { webView, navigation in
                previousCallback?(webView, navigation)
                guard !resumed else {
                    return
                }

                resumed = true
                continuation.resume()
            }
        }
    }

    /// Waits for the next load in a frame other than the main frame to finish.
    ///
    /// Any `didFinishLoadWithRequestInFrame` callback the test set still runs.
    public func waitForDidFinishLoadInSubframe() async {
        let previousCallback = didFinishLoadWithRequestInFrame
        defer { didFinishLoadWithRequestInFrame = previousCallback }

        await withCheckedContinuation { continuation in
            var resumed = false
            didFinishLoadWithRequestInFrame = { webView, request, frame in
                previousCallback?(webView, request, frame)
                guard !resumed, !frame.isMainFrame else {
                    return
                }

                resumed = true
                continuation.resume()
            }
        }
    }

    /// Waits for the web content process of the main frame to terminate.
    ///
    /// Any `webContentProcessDidTerminate` callback the test set still runs.
    ///
    /// - Returns: Why the process terminated.
    public func waitForWebContentProcessDidTerminate() async -> _WKProcessTerminationReason {
        let previousCallback = webContentProcessDidTerminate
        defer { webContentProcessDidTerminate = previousCallback }

        return await withCheckedContinuation { continuation in
            var resumed = false
            webContentProcessDidTerminate = { webView, reason in
                previousCallback?(webView, reason)
                guard !resumed else {
                    return
                }

                resumed = true
                continuation.resume(returning: reason)
            }
        }
    }

    /// Waits for the policy decision of the next navigation action that matches a condition, and holds the decision,
    /// so that the navigation waits until the test makes it.
    ///
    /// ```swift
    /// let decidePolicy = await navigationDelegate.holdNextNavigationActionPolicyDecision {
    ///     $0.request.url == pendingURL
    /// } triggeredBy: {
    ///     webView.load(URLRequest(url: pendingURL))
    /// }
    /// // Check what happens while the navigation waits for its policy decision.
    /// decidePolicy(.cancel)
    /// ```
    ///
    /// The `decidePolicyForNavigationAction` callback the test set, if any, decides every other navigation action;
    /// without one, they are allowed.
    ///
    /// - Parameters:
    ///   - condition: Whether to hold the decision for a navigation action.
    ///   - body: Work that starts the navigation, such as a load.
    /// - Returns: The decision handler of the navigation action. Call it once, before the test ends.
    public func holdNextNavigationActionPolicyDecision(
        where condition: @escaping (WKNavigationAction) -> Bool,
        triggeredBy body: () -> Void
    ) async -> (WKNavigationActionPolicy) -> Void {
        let previousCallback = decidePolicyForNavigationAction
        defer { decidePolicyForNavigationAction = previousCallback }

        var heldDecisionHandler: ((WKNavigationActionPolicy) -> Void)?
        await withCheckedContinuation { continuation in
            decidePolicyForNavigationAction = { navigationAction, decisionHandler in
                guard heldDecisionHandler == nil, condition(navigationAction) else {
                    if let previousCallback {
                        previousCallback(navigationAction, decisionHandler)
                    } else {
                        decisionHandler(.allow)
                    }
                    return
                }

                heldDecisionHandler = decisionHandler
                continuation.resume()
            }

            body()
        }

        guard let heldDecisionHandler else {
            fatalError("The continuation only resumes once it holds a decision handler")
        }
        return heldDecisionHandler
    }
}
