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
    ///   - functionBody: A closure producing the JavaScript function body to call.
    /// - Returns: The value the function body returned.
    /// - Throws: ``UnexpectedJavaScriptResult`` if the function body returned a value of a
    ///   different type, or any error raised while running it.
    public func callJavaScript<Result>(
        returning type: Result.Type,
        _ functionBody: () -> String
    ) async throws -> Result {
        let script = functionBody()
        let result = try await __callAsyncJavaScript(script, arguments: [:], inFrame: nil, in: .page)

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
    /// - Parameter functionBody: A closure producing the JavaScript function body to call.
    /// - Throws: Any error raised while running the function body.
    public func callJavaScript(_ functionBody: () -> String) async throws {
        _ = try await __callAsyncJavaScript(functionBody(), arguments: [:], inFrame: nil, in: .page)
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
