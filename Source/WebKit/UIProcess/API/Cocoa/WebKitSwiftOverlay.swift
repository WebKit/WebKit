// Copyright (C) 2020 Apple Inc. All rights reserved.
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

// FIXME: Eliminate this file since the refined API can now just go with the rest of the normal API where it belongs.

#if !os(tvOS) && !os(watchOS)

// Older versions of the Swift compiler fail to import WebKit_Private. Can be
// removed when WebKit drops support for macOS Sonoma.
#if ENABLE_WK_WEB_EXTENSIONS && compiler(>=6.1)
internal import WebKit_Private.WKWebExtensionPrivate
#endif

@available(iOS 14.0, macOS 10.16, *)
extension WKPDFConfiguration {
    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var rect: CGRect? {
        get { __rect == .null ? nil : __rect }
        set { __rect = newValue ?? .null }
    }
}

@available(iOS 14.0, macOS 10.16, *)
extension WKWebView {
    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @preconcurrency
    public func callAsyncJavaScript(
        _ functionBody: Swift.String,
        arguments: [Swift.String: Any] = [:],
        in frame: WKFrameInfo? = nil,
        in contentWorld: WKContentWorld,
        completionHandler: (@MainActor (Result<Any, Error>) -> Void)? = nil
    ) {
        let thunk = completionHandler.map { ObjCBlockConversion.boxingNilAsAnyForCompatibility($0) }
        __callAsyncJavaScript(functionBody, arguments: arguments, inFrame: frame, in: contentWorld, completionHandler: thunk)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @preconcurrency
    public func createPDF(
        configuration: WKPDFConfiguration = .init(),
        completionHandler: @MainActor @escaping (Result<Data, Error>) -> Void
    ) {
        __createPDF(with: configuration, completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @preconcurrency
    public func createWebArchiveData(completionHandler: @MainActor @escaping (Result<Data, Error>) -> Void) {
        __createWebArchiveData(completionHandler: ObjCBlockConversion.exclusive(completionHandler))
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @preconcurrency
    public func evaluateJavaScript(
        _ javaScript: Swift.String,
        in frame: WKFrameInfo? = nil,
        in contentWorld: WKContentWorld,
        completionHandler: (@MainActor (Result<Any, Error>) -> Void)? = nil
    ) {
        let thunk = completionHandler.map { ObjCBlockConversion.boxingNilAsAnyForCompatibility($0) }
        __evaluateJavaScript(javaScript, inFrame: frame, in: contentWorld, completionHandler: thunk)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @preconcurrency
    public func find(
        _ string: Swift.String,
        configuration: WKFindConfiguration = .init(),
        completionHandler: @MainActor @escaping (WKFindResult) -> Void
    ) {
        __find(string, with: configuration, completionHandler: completionHandler)
    }
}

@available(iOS 15.0, macOS 12.0, *)
extension WKWebView {
    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func callAsyncJavaScript(
        _ functionBody: Swift.String,
        arguments: [Swift.String: Any] = [:],
        in frame: WKFrameInfo? = nil,
        contentWorld: WKContentWorld
    ) async throws -> Any? {
        try await __callAsyncJavaScript(functionBody, arguments: arguments, inFrame: frame, in: contentWorld)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func pdf(configuration: WKPDFConfiguration = .init()) async throws -> Data {
        try await __createPDF(with: configuration)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func evaluateJavaScript(
        _ javaScript: Swift.String,
        in frame: WKFrameInfo? = nil,
        contentWorld: WKContentWorld
    ) async throws -> Any? {
        try await __evaluateJavaScript(javaScript, inFrame: frame, in: contentWorld)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func find(_ string: Swift.String, configuration: WKFindConfiguration = .init()) async throws -> WKFindResult {
        await __find(string, with: configuration)
    }
}

#if ENABLE_WK_WEB_EXTENSIONS && compiler(>=6.1)
@available(iOS 18.4, macOS 15.4, visionOS 2.4, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WKWebExtension {
    /// Creates a web extension initialized with a specified app extension bundle.
    ///
    /// The app extension bundle must contain a `manifest.json` file in its resources directory.
    /// If the manifest is invalid or missing, or the bundle is otherwise improperly configured, an error will be thrown.
    ///
    /// - Parameter appExtensionBundle: The bundle to use for the new web extension.
    /// - Throws: An error if the manifest is invalid or missing, or the bundle is otherwise improperly configured.
    public convenience init(appExtensionBundle: Bundle) async throws {
        // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
        try self.init(appExtensionBundle: appExtensionBundle, resourceBaseURL: nil)
    }

    /// Creates a web extension initialized with a specified resource base URL, which can point to either a directory or a ZIP archive.
    ///
    /// The URL must be a file URL that points to either a directory with a `manifest.json` file or a ZIP archive containing a `manifest.json` file.
    /// If the manifest is invalid or missing, or the URL points to an unsupported format or invalid archive, an error will be returned.
    ///
    /// - Parameter resourceBaseURL: The file URL to use for the new web extension.
    /// - Throws: An error if the manifest is invalid or missing, or the URL points to an unsupported format or invalid archive.
    public convenience init(resourceBaseURL: Foundation.URL) async throws {
        // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
        try self.init(appExtensionBundle: nil, resourceBaseURL: resourceBaseURL)
    }
}

@available(iOS 18.4, macOS 15.4, visionOS 2.4, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WKWebExtensionController {
    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didCloseTab(_ closedTab: WKWebExtensionTab, windowIsClosing: Bool = false) {
        __didClose(closedTab, windowIsClosing: windowIsClosing)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didActivateTab(_ activatedTab: any WKWebExtensionTab, previousActiveTab previousTab: (any WKWebExtensionTab)? = nil) {
        __didActivate(activatedTab, previousActiveTab: previousTab)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didMoveTab(_ movedTab: any WKWebExtensionTab, from index: Int, in oldWindow: (any WKWebExtensionWindow)? = nil) {
        __didMove(movedTab, from: index, in: oldWindow)
    }
}

@available(iOS 18.4, macOS 15.4, visionOS 2.4, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WKWebExtensionContext {
    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didCloseTab(_ closedTab: WKWebExtensionTab, windowIsClosing: Bool = false) {
        __didClose(closedTab, windowIsClosing: windowIsClosing)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didActivateTab(_ activatedTab: any WKWebExtensionTab, previousActiveTab previousTab: (any WKWebExtensionTab)? = nil) {
        __didActivate(activatedTab, previousActiveTab: previousTab)
    }

    // This is pre-existing API whose documentation does not use the source code.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func didMoveTab(_ movedTab: any WKWebExtensionTab, from index: Int, in oldWindow: (any WKWebExtensionWindow)? = nil) {
        __didMove(movedTab, from: index, in: oldWindow)
    }
}
#endif

#endif // !os(tvOS) && !os(watchOS)
