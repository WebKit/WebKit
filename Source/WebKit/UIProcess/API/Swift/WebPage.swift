// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import Observation
public import SwiftUI // FIXME: (283455) Do not import SwiftUI in WebKit proper.

@_spi(Private)
@MainActor
@Observable
public class WebPage_v0 {
    public let navigations: Navigations

    public var url: URL? {
        self.access(keyPath: \.url)
        return backingWebView.url
    }

    public var title: String {
        self.access(keyPath: \.title)

        // The title property is annotated as optional in WKWebView, but is never actually `nil`.
        return backingWebView.title!
    }

    public var estimatedProgress: Double {
        self.access(keyPath: \.estimatedProgress)
        return backingWebView.estimatedProgress
    }

    public var isLoading: Bool {
        self.access(keyPath: \.isLoading)
        return backingWebView.isLoading
    }

    public var serverTrust: SecTrust? {
        self.access(keyPath: \.serverTrust)
        return backingWebView.serverTrust
    }

    public var hasOnlySecureContent: Bool {
        self.access(keyPath: \.hasOnlySecureContent)
        return backingWebView.hasOnlySecureContent
    }

    public var themeColor: Color? {
        self.access(keyPath: \.themeColor)

        // The themeColor property is a UIColor/NSColor in WKWebView.
#if canImport(UIKit)
        return backingWebView.themeColor.map(Color.init(uiColor:))
#else
        return backingWebView.themeColor.map(Color.init(nsColor:))
#endif
    }

    public var mediaType: String? {
        get { backingWebView.mediaType }
        set { backingWebView.mediaType = newValue }
    }

    public var customUserAgent: String? {
        get { backingWebView.customUserAgent }
        set { backingWebView.customUserAgent = newValue }
    }

    public var isInspectable: Bool {
        get { backingWebView.isInspectable }
        set { backingWebView.isInspectable = newValue }
    }

    private let backingNavigationDelegate: WKNavigationDelegateAdapter

    @ObservationIgnored
    private var observations = KeyValueObservations()

    @ObservationIgnored
    var isBoundToWebView = false

    @ObservationIgnored
    lazy var backingWebView: WKWebView = {
        let webView = WKWebView(frame: .zero)
        webView.navigationDelegate = backingNavigationDelegate
        return webView
    }()

    public init() {
        // FIXME: Consider whether we want to have a single value here or if the getter for `navigations` should return a fresh sequence every time.
        let (stream, continuation) = AsyncStream.makeStream(of: NavigationEvent.self)
        navigations = Navigations(source: stream)

        backingNavigationDelegate = WKNavigationDelegateAdapter(navigationProgressContinuation: continuation)

        observations.contents = [
            createObservation(for: \.url, backedBy: \.url),
            createObservation(for: \.title, backedBy: \.title),
            createObservation(for: \.estimatedProgress, backedBy: \.estimatedProgress),
            createObservation(for: \.isLoading, backedBy: \.isLoading),
            createObservation(for: \.serverTrust, backedBy: \.serverTrust),
            createObservation(for: \.hasOnlySecureContent, backedBy: \.hasOnlySecureContent),
            createObservation(for: \.themeColor, backedBy: \.themeColor),
        ]
    }

    @discardableResult
    public func load(_ request: URLRequest) -> NavigationID? {
        backingWebView.load(request).map(NavigationID.init(_:))
    }

    @discardableResult
    public func load(htmlString: String, baseURL: URL) -> NavigationID? {
        backingWebView.loadHTMLString(htmlString, baseURL: baseURL).map(NavigationID.init(_:))
    }

    private func createObservation<Value, BackingValue>(for keyPath: KeyPath<WebPage_v0, Value>, backedBy backingKeyPath: KeyPath<WKWebView, BackingValue>) -> NSKeyValueObservation {
        return backingWebView.observe(backingKeyPath, options: [.prior, .old, .new]) { [_$observationRegistrar, unowned self] _, change in
            if change.isPrior {
                _$observationRegistrar.willSet(self, keyPath: keyPath)
            } else {
                _$observationRegistrar.didSet(self, keyPath: keyPath)
            }
        }
    }
}

extension WebPage_v0 {
    private struct KeyValueObservations: ~Copyable {
        var contents: Set<NSKeyValueObservation> = []

        deinit {
            for observation in contents {
                observation.invalidate()
            }
        }
    }
}

#endif
