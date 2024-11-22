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

public import SwiftUI

#if canImport(UIKit)
fileprivate typealias PlatformView = UIView
#else
fileprivate typealias PlatformView = NSView
#endif

@_spi(Private)
public struct WebView_v0: View {
    fileprivate let page: WebPage_v0

    public init(_ page: WebPage_v0) {
        self.page = page
    }

    public var body: some View {
        WebViewRepresentable(owner: self)
    }
}

fileprivate class WebViewWrapper: PlatformView {
    override var frame: CGRect {
        get {
            super.frame
        }
        set {
            super.frame = newValue
            updateWebViewFrame()
        }
    }

    override var bounds: CGRect {
        get {
            super.bounds
        }
        set {
            super.bounds = newValue
            updateWebViewFrame()
        }
    }

    var webView: WKWebView? = nil {
        willSet {
            webView?.removeFromSuperview()
        }
        didSet {
            guard let webView else {
                return
            }

            addSubview(webView)
            updateWebViewFrame()
        }
    }

    private func updateWebViewFrame() {
        webView?.frame = bounds
    }
}

@MainActor
fileprivate struct WebViewRepresentable {
    let owner: WebView_v0

    func makePlatformView(context: Context) -> WebViewWrapper {
        // FIXME: Make this more robust by figuring out what happens when a WebPage moves between representables.
        // We can't have multiple owners regardless, but we'll want to decide if it's an error, if we can handle it gracefully, and how deterministic it might even be.
        // Perhaps we should keep an ownership assertion which we can tear down in something like dismantleUIView().

        precondition(!owner.page.isBoundToWebView, "This web page is already bound to another web view.")

        let parent = WebViewWrapper()
        parent.webView = owner.page.backingWebView
        owner.page.isBoundToWebView = true

        return parent
    }

    func updatePlatformView(_ platformView: WebViewWrapper, context: Context) {
        platformView.webView = owner.page.backingWebView
    }
}

#if canImport(UIKit)
extension WebViewRepresentable: UIViewRepresentable {
    func makeUIView(context: Context) -> WebViewWrapper {
        makePlatformView(context: context)
    }

    func updateUIView(_ uiView: WebViewWrapper, context: Context) {
        updatePlatformView(uiView, context: context)
    }
}
#else
extension WebViewRepresentable: NSViewRepresentable {
    func makeNSView(context: Context) -> WebViewWrapper {
        makePlatformView(context: context)
    }
    
    func updateNSView(_ nsView: WebViewWrapper, context: Context) {
        updatePlatformView(nsView, context: context)
    }
}
#endif

#endif
