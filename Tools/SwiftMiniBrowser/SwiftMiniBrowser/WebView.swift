/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

// For WKWebViewController:
@_spiOnly import WebKit_Private

@Observable class WebViewModel : Identifiable {
    var url : URL?
    var urlString : String {
        get { url?.absoluteString ?? "" }
        set { url = URL(string: newValue) }
    }
    var title : String = ""
    var canGoForward : Bool = false
    var canGoBack : Bool = false
    var isLoading : Bool = false
    var estimatedProgress : Double = 0.0

    var currentItem : WKBackForwardListItem?
    var backForwardList : WKBackForwardList?

    func goBack() {
        guard let list = backForwardList else { return }
        guard let backItem = list.backItem else { return }
        guard let action = goToItemAction else { return }
        action(backItem)
    }

    func goForward() {
        guard let list = backForwardList else { return }
        guard let forwardItem = list.forwardItem else { return }
        guard let action = goToItemAction else { return }
        action(forwardItem)
    }

    func reload() {
        guard let action = reloadAction else { return }
        action()
    }

    func loadURL() {
        guard let action = loadAction else { return }
        guard let url = self.url else { return }
        action(url)
    }

    typealias GoToAction = (_ item: WKBackForwardListItem) -> Void
    var goToItemAction : GoToAction?

    typealias ReloadAction = () -> Void
    var reloadAction : ReloadAction?

    typealias LoadAction = (_ item:URL) -> Void
    var loadAction : LoadAction?
}

struct WebView : View {
    @State var configuration : WKWebViewConfiguration
    @Binding var model : WebViewModel

    init(configuration: WKWebViewConfiguration, model: Binding<WebViewModel>) {
        self._configuration = State(initialValue: configuration)
        self._model = model
    }

    var body: some View {
        WebViewController(configuration: $configuration, model: $model)
    }

    public mutating func setConfiguration(_ configuration: WKWebViewConfiguration) -> Self {
        self.configuration = configuration
        return self
    }
}

struct WebViewController : UIViewControllerRepresentable {
    @Binding var configuration : WKWebViewConfiguration
    @Binding var model : WebViewModel

    init(configuration: Binding<WKWebViewConfiguration>, model: Binding<WebViewModel>) {
        self._configuration = configuration
        self._model = model
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(model:$model)
    }

    public func makeUIViewController(context: Context) -> WKWebViewController {
        let controller = WKWebViewController(configuration: $configuration.wrappedValue)
        context.coordinator.setWebView(controller.webView)
        self.model.loadURL()
        return controller;
    }

    public func updateUIViewController(_ webViewController: WKWebViewController, context: Context) {
    }

    class Coordinator: NSObject, WKNavigationDelegate, Sendable {
        @Binding var model : WebViewModel
        @objc var webView : WKWebView?
        var canGoBackObservation: NSKeyValueObservation?
        var canGoForwardObservation: NSKeyValueObservation?
        var progressObservation: NSKeyValueObservation?
        var refreshControl: UIRefreshControl?

        init(model: Binding<WebViewModel>) {
            self._model = model
            super.init()

            self.model.goToItemAction = self.goToItem
            self.model.reloadAction = self.reload
            self.model.loadAction = self.loadURL
        }

        func goToItem(_ item : WKBackForwardListItem) -> Void {
            guard let webView = self.webView else { return }
            webView.go(to:item)
        }

        func reload() -> Void {
            guard let webView = self.webView else { return }
            webView.reload()
        }

        @objc func reloadAction() {
            reload()
        }

        func loadURL(_ url: URL) {
            guard let webView = self.webView else { return }
            let request = URLRequest(url: url)
            webView.load(request)
            model.isLoading = true
        }

        func setWebView(_ webView: WKWebView) {
            self.webView = webView
            webView.navigationDelegate = self
            refreshControl = UIRefreshControl();
            refreshControl?.addTarget(self, action:#selector(reloadAction), for: .valueChanged)
            webView.scrollView.refreshControl = refreshControl

            canGoBackObservation = observe(
                \.webView?.canGoBack,
                options: [.new]
            ) { object, change in
                guard let canGoBack = change.newValue else { return }
                self.model.canGoBack = canGoBack ?? false
            }

            canGoForwardObservation = observe(
                \.webView?.canGoForward,
                options: [.new]
            ) { object, change in
                guard let canGoForward = change.newValue else { return }
                self.model.canGoForward = canGoForward ?? false
            }

            progressObservation = observe(
                \.webView?.estimatedProgress,
                 options: [.new]
            ) { object, change in
                guard let estimatedProgress = change.newValue else { return }
                self.model.estimatedProgress = estimatedProgress ?? 0
            }

            model.backForwardList = webView.backForwardList
        }

        func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
            self.model.url = webView.url
            self.model.isLoading = true
        }

        func webView(_ webView: WKWebView, didReceiveServerRedirectForProvisionalNavigation: WKNavigation!) {
            self.model.url = webView.url
        }

        func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) {
            self.model.url = webView.url
        }

        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            self.model.url = webView.url
            self.model.isLoading = false
            guard let refreshControl = self.refreshControl else { return }
            guard refreshControl.isRefreshing else { return }
            refreshControl.endRefreshing()
        }

        func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: any Error) {
            self.model.isLoading = false
            guard let refreshControl = self.refreshControl else { return }
            guard refreshControl.isRefreshing else { return }
            refreshControl.endRefreshing()
        }
    }
}
