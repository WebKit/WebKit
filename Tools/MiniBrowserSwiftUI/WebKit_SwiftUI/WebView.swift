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

import SwiftUI
import WebKit

/// A view that displays interactive web content.
///
/// A `WebView` is useful in any scenario that requires rendering HTML, such as for a web browser,
/// ePub reader, or simply to display local HTML resources. It provides basic behaviors for handling
/// web content by default, which can be customized to provide alternate behaviors.
public struct WebView : View {
    /// Checks whether or not WebView can handle the given URL by default.
    public static func canHandle(_ url: URL) -> Bool {
        return url.scheme.map(WKWebView.handlesURLScheme(_:)) ?? false
    }

    fileprivate var state: WebViewState
    @State private var defaultDialog: Dialog? = nil
    private var customDialog: Binding<Dialog?>? = nil
    fileprivate var dialog: Dialog? {
        get { (self.customDialog ?? self.$defaultDialog).wrappedValue }
        nonmutating set { (self.customDialog ?? self.$defaultDialog).wrappedValue = newValue }
    }
    private var useInternalDialogHandling = true

    /// Create a new `WebView` instance.
    ///
    /// In order to store the state of webpages, a `WebViewState` instance must be provided by the
    /// caller. Once the web view is displayed its state can be both read and modified from the
    /// `WebViewState`, such as determining whether the `WebView` contains back-forward history, or
    /// to reload the current page.
    ///
    /// - Parameters:
    ///   - state: State to be associated with this web view.
    ///   - dialog: A binding representing any dialog that the webpage needs displayed. A custom
    ///   binding should only be provided if you need to customize the display of dialogs, otherwise
    ///   `WebView` will manage display.
    public init(state: WebViewState, dialog: Binding<Dialog?>? = nil) {
        self.state = state
        self.customDialog = dialog
        self.useInternalDialogHandling = dialog == nil
    }

    public var body: some View {
        _WebView(owner: self)
            .overlay(dialogView)
    }

    @ViewBuilder
    private var dialogView: some View {
        if useInternalDialogHandling, let configuration = dialog?.configuration {
            switch configuration {
            case let .javaScriptAlert(message, completion):
                JavaScriptAlert(message: message, completion: {
                    dialog = nil
                    completion()
                })
            case let .javaScriptConfirm(message, completion):
                JavaScriptConfirm(message: message, completion: {
                    dialog = nil
                    completion($0)
                })
            case let .javaScriptPrompt(message, defaultText, completion):
                JavaScriptPrompt(message: message, defaultText: defaultText, completion: {
                    dialog = nil
                    completion($0)
                })
            }
        } else {
            EmptyView().hidden()
        }
    }
}

private struct _WebView {
    let owner: WebView

    func makeView(coordinator: Coordinator, environment: EnvironmentValues) -> WKWebView {
        let view = WKWebView()
        view.navigationDelegate = coordinator
        view.uiDelegate = coordinator
        coordinator.webView = view
        coordinator.environment = environment

        if let request = coordinator.initialRequest {
            view.load(request)
        }

        return view
    }

    func updateView(_ view: WKWebView, coordinator: Coordinator, environment: EnvironmentValues) {
        coordinator.environment = environment

        if let flag = environment.allowsBackForwardNavigationGestures {
            view.allowsBackForwardNavigationGestures = flag
        }
    }

    static func dismantleView(_ view: WKWebView, coordinator: Coordinator) {
        coordinator.webView = nil
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(owner: owner)
    }
}

#if os(macOS)
extension _WebView : NSViewRepresentable {
    func makeNSView(context: Context) -> WKWebView {
        makeView(coordinator: context.coordinator, environment: context.environment)
    }

    func updateNSView(_ nsView: WKWebView, context: Context) {
        updateView(nsView, coordinator: context.coordinator, environment: context.environment)
    }

    static func dismantleNSView(_ nsView: WKWebView, coordinator: Coordinator) {
        dismantleView(nsView, coordinator: coordinator)
    }
}
#else
extension _WebView : UIViewRepresentable {
    func makeUIView(context: Context) -> WKWebView {
        makeView(coordinator: context.coordinator, environment: context.environment)
    }

    func updateUIView(_ uiView: WKWebView, context: Context) {
        updateView(uiView, coordinator: context.coordinator, environment: context.environment)
    }

    static func dismantleUIView(_ uiView: WKWebView, coordinator: Coordinator) {
        dismantleView(uiView, coordinator: coordinator)
    }
}
#endif

@dynamicMemberLookup
private final class Coordinator : NSObject, WKNavigationDelegate, WKUIDelegate {
    private var owner: WebView
    fileprivate var environment: EnvironmentValues?

    init(owner: WebView) {
        self.owner = owner
    }

    // MARK: WebViewState Proxy

    var webView: WKWebView? {
        get { owner.state.webView }
        set { owner.state.webView = newValue }
    }

    subscript<T>(dynamicMember keyPath: ReferenceWritableKeyPath<WebViewState, T>) -> T {
        get { owner.state[keyPath: keyPath] }
        set { owner.state[keyPath: keyPath] = newValue }
    }

    // MARK: Navigation

    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        preferences: WKWebpagePreferences,
        decisionHandler: @escaping (WKNavigationActionPolicy, WKWebpagePreferences) -> Void
    ) {
        if let decider = environment?.navigationActionDecider {
            let action = NavigationAction(
                navigationAction, webpagePreferences: preferences, reply: decisionHandler)
            decider(action, owner.state)
        } else {
            decisionHandler(.allow, preferences)
        }
    }

    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationResponse: WKNavigationResponse,
        decisionHandler: @escaping (WKNavigationResponsePolicy) -> Void
    ) {
        if let decider = environment?.navigationResponseDecider {
            let response = NavigationResponse(navigationResponse, reply: decisionHandler)
            decider(response, owner.state)
        } else {
            decisionHandler(.allow)
        }
    }

    // MARK: Dialogs

    func webView(
        _ webView: WKWebView,
        runJavaScriptAlertPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping () -> Void
    ) {
        owner.dialog = .javaScriptAlert(message, completion: completionHandler)
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping (Bool) -> Void
    ) {
        owner.dialog = .javaScriptConfirm(message, completion: completionHandler)
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptTextInputPanelWithPrompt prompt: String,
        defaultText: String?,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping (String?) -> Void
    ) {
        owner.dialog = .javaScriptPrompt(
            prompt, defaultText: defaultText ?? "", completion: completionHandler)
    }
}
