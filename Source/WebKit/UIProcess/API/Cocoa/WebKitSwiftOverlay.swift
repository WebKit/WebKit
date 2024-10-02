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

@_exported import WebKit
@_spi(CTypeConversion) import Network

@available(iOS 14.0, macOS 10.16, *)
extension WKPDFConfiguration {
    public var rect: CGRect? {
        get { __rect == .null ? nil : __rect }
        set { __rect = newValue == nil ? .null : newValue! }
    }
}

@available(iOS 14.0, macOS 10.16, *)
extension WKWebView {
    public func callAsyncJavaScript(_ functionBody: String, arguments: [String:Any] = [:], in frame: WKFrameInfo? = nil, in contentWorld: WKContentWorld, completionHandler: ((Result<Any, Error>) -> Void)? = nil) {
        __callAsyncJavaScript(functionBody, arguments: arguments, inFrame: frame, in: contentWorld, completionHandler: completionHandler.map(ObjCBlockConversion.boxingNilAsAnyForCompatibility))
    }

    public func createPDF(configuration: WKPDFConfiguration = .init(), completionHandler: @escaping (Result<Data, Error>) -> Void) {
        __createPDF(with: configuration, completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    public func createWebArchiveData(completionHandler: @escaping (Result<Data, Error>) -> Void) {
        __createWebArchiveData(completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    public func evaluateJavaScript(_ javaScript: String, in frame: WKFrameInfo? = nil, in contentWorld: WKContentWorld, completionHandler: ((Result<Any, Error>) -> Void)? = nil) {
        __evaluateJavaScript(javaScript, inFrame: frame, in: contentWorld, completionHandler: completionHandler.map(ObjCBlockConversion.boxingNilAsAnyForCompatibility))
    }

    public func find(_ string: String, configuration: WKFindConfiguration = .init(), completionHandler: @escaping (WKFindResult) -> Void) {
        __find(string, with: configuration, completionHandler: completionHandler)
    }
}

#if swift(>=5.5)
@available(iOS 15.0, macOS 12.0, *)
extension WKWebView {
    public func callAsyncJavaScript(_ functionBody: String, arguments: [String:Any] = [:], in frame: WKFrameInfo? = nil, contentWorld: WKContentWorld) async throws -> Any? {
        return try await __callAsyncJavaScript(functionBody, arguments: arguments, inFrame: frame, in: contentWorld)
    }

    public func pdf(configuration: WKPDFConfiguration = .init()) async throws -> Data {
        try await __createPDF(with: configuration)
    }

    public func evaluateJavaScript(_ javaScript: String, in frame: WKFrameInfo? = nil, contentWorld: WKContentWorld) async throws -> Any? {
        try await __evaluateJavaScript(javaScript, inFrame: frame, in: contentWorld)
    }

    public func find(_ string: String, configuration: WKFindConfiguration = .init()) async throws -> WKFindResult {
        await __find(string, with: configuration)
    }
}
#endif

@available(iOS 18.4, macOS 15.4, visionOS 2.4, *)
extension WKWebExtensionController {
    public func didCloseTab(_ closedTab: WKWebExtensionTab, windowIsClosing: Bool = false) {
        __didClose(closedTab, windowIsClosing: windowIsClosing)
    }

    public func didActivateTab(_ activatedTab: any WKWebExtensionTab, previousActiveTab previousTab: (any WKWebExtensionTab)? = nil) {
        __didActivate(activatedTab, previousActiveTab: previousTab)
    }

    public func didMoveTab(_ movedTab: any WKWebExtensionTab, from index: Int, in oldWindow: (any WKWebExtensionWindow)? = nil) {
        __didMove(movedTab, from: index, in: oldWindow)
    }
}

@available(iOS 18.4, macOS 15.4, visionOS 2.4, *)
extension WKWebExtensionContext {
    public func didCloseTab(_ closedTab: WKWebExtensionTab, windowIsClosing: Bool = false) {
        __didClose(closedTab, windowIsClosing: windowIsClosing)
    }

    public func didActivateTab(_ activatedTab: any WKWebExtensionTab, previousActiveTab previousTab: (any WKWebExtensionTab)? = nil) {
        __didActivate(activatedTab, previousActiveTab: previousTab)
    }

    public func didMoveTab(_ movedTab: any WKWebExtensionTab, from index: Int, in oldWindow: (any WKWebExtensionWindow)? = nil) {
        __didMove(movedTab, from: index, in: oldWindow)
    }
}

#if canImport(Network, _version: "3623.0.0.0")
@available(iOS 17.0, macOS 14.0, *)
extension WKWebsiteDataStore {
    public var proxyConfigurations: [ProxyConfiguration] {
        get { __proxyConfigurations?.map(ProxyConfiguration.init(_:)) ?? [] }
        set { __proxyConfigurations = newValue.map(\.nw) }
    }
}
#endif
