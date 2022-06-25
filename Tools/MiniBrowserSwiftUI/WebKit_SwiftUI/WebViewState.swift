/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import Combine
import SwiftUI
import WebKit

/// Holds the dynamic state of a `WebView`.
final public class WebViewState : ObservableObject {
    var initialRequest: URLRequest?
    var webViewObservations: [NSKeyValueObservation] = []
    var webView: WKWebView? {
        didSet {
            webViewObservations.forEach { $0.invalidate() }
            guard let webView = webView else {
                webViewObservations.removeAll()
                return
            }

            func register<T>(
                _ keyPath: KeyPath<WKWebView, T>
            ) -> NSKeyValueObservation where T : Equatable {
                webView.observe(
                    keyPath, options: [.prior, .old, .new],
                    changeHandler: webView(_:didChangeKeyPath:))
            }

            // TODO: Evaluate the performance impact of pre-registering all of these observations.
            // We can't know what changes will trigger view updates, so we need to observe every
            // property that get's republished below; however, there may be more efficient ways to
            // achieve this.
            webViewObservations = [
                register(\.canGoBack), register(\.canGoForward), register(\.title), register(\.url),
                register(\.isLoading), register(\.estimatedProgress),
            ]
        }
    }

    public let objectWillChange = ObservableObjectPublisher()

    /// Creates a new `WebViewState`.
    /// - Parameters:
    ///   - initialURL: The `URL` to navigate to immediatly upon `WebView` creation.
    ///   - configuration: Configuration for the `WebView`.
    public convenience init(
        initialURL: URL? = nil, configuration: WKWebViewConfiguration = .init()
    ) {
        self.init(
            initialRequest: initialURL.map { URLRequest(url: $0) },
            configuration: configuration)
    }

    /// Creates a new `WebViewState`.
    /// - Parameters:
    ///   - initialRequest: The `URLRequest` to navigate to immediatly upon `WebView` creation.
    ///   - configuration: Configuration for the `WebView`.
    public init(initialRequest: URLRequest?, configuration: WKWebViewConfiguration = .init()) {
        self.initialRequest = initialRequest
    }

    func webView<Value>(
        _: WKWebView, didChangeKeyPath change: NSKeyValueObservedChange<Value>
    ) where Value : Equatable {
        if change.isPrior && change.oldValue != change.newValue {
            objectWillChange.send()
        }
    }

    // NOTE: I'm eliding documentation comments here. These properties and methods related 1:1 with
    // their `WKWebView` counterparts.

    public var canGoBack: Bool { webView?.canGoBack ?? false }
    public var canGoForward: Bool { webView?.canGoForward ?? false }
    public var title: String { webView?.title ?? "" }
    public var url: URL? { webView?.url }
    public var isLoading: Bool { webView?.isLoading ?? false }
    public var estimatedProgress: Double? { isLoading ? webView?.estimatedProgress : nil }
    public var hasOnlySecureContent: Bool { webView?.hasOnlySecureContent ?? false }

    public func load(_ url: URL) {
        load(URLRequest(url: url))
    }

    public func load(_ request: URLRequest) {
        webView?.load(request)
    }

    public func goBack() {
        webView?.goBack()
    }

    public func goForward() {
        webView?.goForward()
    }

    public func reload() {
        webView?.reload()
    }

    public func stopLoading() {
        webView?.stopLoading()
    }

    func createPDF(
        configuration: WKPDFConfiguration = .init(),
        completion: @escaping (Result<Data, Error>) -> Void
    ) {
        if let webView = webView {
            webView.createPDF(configuration: configuration, completionHandler: completion)
        } else {
            completion(.failure(WKError(.unknown)))
        }
    }
}
