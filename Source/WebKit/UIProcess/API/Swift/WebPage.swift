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
import Observation
internal import WebKit_Private

@MainActor
@_spi(CrossImportOverlay)
public final class WebPageWebView: WKWebView {
    @_spi(CrossImportOverlay)
    public weak var delegate: (any Delegate)? = nil

#if os(iOS)
    override func findInteraction(_ interaction: UIFindInteraction, didBegin session: UIFindSession) {
        super.findInteraction(interaction, didBegin: session)
        delegate?.findInteraction(interaction, didBegin: session)
    }

    override func findInteraction(_ interaction: UIFindInteraction, didEnd session: UIFindSession) {
        super.findInteraction(interaction, didBegin: session)
        delegate?.findInteraction(interaction, didEnd: session)
    }

    override func supportsTextReplacement() -> Bool {
        guard let delegate else {
            return super.supportsTextReplacement()
        }

        return super.supportsTextReplacement() && delegate.supportsTextReplacement()
    }
#endif
}

extension WebPageWebView {
    @MainActor
    @_spi(CrossImportOverlay)
    public protocol Delegate: AnyObject {
#if os(iOS)
        func findInteraction(_ interaction: UIFindInteraction, didBegin session: UIFindSession)

        func findInteraction(_ interaction: UIFindInteraction, didEnd session: UIFindSession)

        func supportsTextReplacement() -> Bool
#endif
    }
}


/// An object that controls and manages the behavior of interactive web content.
@MainActor
@Observable
@available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
final public class WebPage {
    /// A CSS media type as defined by the [CSS specification](https://www.w3.org/TR/mediaqueries-4/#media-types), or an arbitrary media type value.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct CSSMediaType: Hashable, RawRepresentable, Sendable {
        /// Corresponds to the "all" media type.
        public static let all = CSSMediaType(rawValue: "all")

        /// Corresponds to the "screen" media type.
        public static let screen = CSSMediaType(rawValue: "screen")

        /// Corresponds to the "print" media type.
        public static let print = CSSMediaType(rawValue: "print")

        /// Create a media type with an arbitrary value. Use the static type properties for the defined canonical CSS media type options.
        /// - Parameter rawValue: The raw value of the media type.
        public init(rawValue: String) {
            self.rawValue = rawValue
        }

        /// The raw value of the media type.
        public let rawValue: String
    }

    /// The set of possible fullscreen states a webpage may be in.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum FullscreenState: Hashable, Sendable {
        /// The page is entering fullscreen.
        case enteringFullscreen

        /// The page is exiting fullscreen.
        case exitingFullscreen

        /// The page is currently in a fullscreen state.
        case inFullscreen

        /// The page is not currently in a fullscreen state.
        case notInFullscreen
    }

    // MARK: Initializers

    private init(
        _configuration: Configuration,
        _navigationDecider navigationDecider: (any NavigationDeciding)?,
        _dialogPresenter dialogPresenter: (any DialogPresenting)?,
        _downloadCoordinator downloadCoordinator: (any DownloadCoordinator)?
    ) {
        self.configuration = _configuration

        let (downloadStream, downloadContinuation) = AsyncStream.makeStream(of: DownloadEvent.self)
        downloads = Downloads(source: downloadStream)

        backingDownloadDelegate = WKDownloadDelegateAdapter(
            downloadProgressContinuation: downloadContinuation,
            downloadCoordinator: downloadCoordinator
        )
        backingUIDelegate = WKUIDelegateAdapter(
            dialogPresenter: dialogPresenter
        )
        backingNavigationDelegate = WKNavigationDelegateAdapter(
            downloadProgressContinuation: downloadContinuation,
            navigationDecider: navigationDecider
        )

        backingUIDelegate.owner = self
        backingNavigationDelegate.owner = self
        backingDownloadDelegate.owner = self
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding,
        dialogPresenter: some DialogPresenting,
        downloadCoordinator: some DownloadCoordinator
    ) {
        self.init(_configuration: configuration, _navigationDecider: navigationDecider, _dialogPresenter: dialogPresenter, _downloadCoordinator: downloadCoordinator)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding,
        dialogPresenter: some DialogPresenting
    ) {
        self.init(_configuration: configuration, _navigationDecider: navigationDecider, _dialogPresenter: dialogPresenter, _downloadCoordinator: nil)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding,
        downloadCoordinator: some DownloadCoordinator
    ) {
        self.init(_configuration: configuration, _navigationDecider: navigationDecider, _dialogPresenter: nil, _downloadCoordinator: downloadCoordinator)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        dialogPresenter: some DialogPresenting,
        downloadCoordinator: some DownloadCoordinator
    ) {
        self.init(_configuration: configuration, _navigationDecider: nil, _dialogPresenter: dialogPresenter, _downloadCoordinator: downloadCoordinator)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        dialogPresenter: some DialogPresenting
    ) {
        self.init(_configuration: configuration, _navigationDecider: nil, _dialogPresenter: dialogPresenter, _downloadCoordinator: nil)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding
    ) {
        self.init(_configuration: configuration, _navigationDecider: navigationDecider, _dialogPresenter: nil, _downloadCoordinator: nil)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
        downloadCoordinator: some DownloadCoordinator
    ) {
        self.init(_configuration: configuration, _navigationDecider: nil, _dialogPresenter: nil, _downloadCoordinator: downloadCoordinator)
    }

    @_spi(Private)
    public convenience init(
        configuration: Configuration = Configuration(),
    ) {
        self.init(_configuration: configuration, _navigationDecider: nil, _dialogPresenter: nil, _downloadCoordinator: nil)
    }

    // MARK: Properties

    /// The current navigation event, or `nil` if there have been no navigations so far.
    ///
    /// This property may be used to observe changes to both an individual navigation, and across navigations.
    ///
    /// A new navigation begins when a `NavigationEvent` has a type of `startedProvisionalNavigation`, and is finished once a
    /// navigation event with a type of `.finished`, `.failedProvisionalNavigation`, or `.failed`.
    public internal(set) var currentNavigationEvent: WebPage.NavigationEvent? = nil

    @_spi(Private)
    public let downloads: Downloads

    @_spi(Private)
    public let configuration: Configuration

    /// The webpage's back-forward list.
    public internal(set) var backForwardList: BackForwardList = BackForwardList()

    /// The URL for the current webpage.
    ///
    /// This property contains the URL for the webpage currently being presented. Use this URL in places
    /// where you reflect the webpage address in your app’s user interface. If the webpage has not loaded
    /// any content yet, this value will be `nil`.
    public var url: URL? {
        backingProperty(\.url, backedBy: \.url)
    }

    /// The page title.
    public var title: String {
        backingProperty(\.title, backedBy: \.title) { backingValue in
            // The title property is annotated as optional in WKWebView, but is never actually `nil`.
            backingValue!
        }
    }

    /// An estimate of completion percentage of the current navigation.
    ///
    /// The value ranges from `0.0` to `1.0` based on the total number of bytes received, including the main
    /// document and all of its potential subresources. After navigation loading completes, the `estimatedProgress`
    /// value remains at `1.0` until a new navigation starts, at which point the `estimatedProgress` value resets
    /// to `0.0`.
    public var estimatedProgress: Double {
        backingProperty(\.estimatedProgress, backedBy: \.estimatedProgress)
    }

    /// Indicates whether the webpage is currently loading content.
    ///
    /// - Returns: `true` if the receiver is still loading content, otherwise, `false`.
    public var isLoading: Bool {
        backingProperty(\.isLoading, backedBy: \.isLoading)
    }

    /// The trust management object you use to evaluate trust for the current webpage.
    ///
    /// Use the object in this property to validate the webpage’s certificate and associated credentials.
    public var serverTrust: SecTrust? {
        backingProperty(\.serverTrust, backedBy: \.serverTrust)
    }

    /// Indicates whether the webpage loaded all resources on the page through securely encrypted connections.
    public var hasOnlySecureContent: Bool {
        backingProperty(\.hasOnlySecureContent, backedBy: \.hasOnlySecureContent)
    }

    /// Indicates whether Writing Tools is active for the view.
    public var isWritingToolsActive: Bool {
        backingProperty(\.isWritingToolsActive, backedBy: \.isWritingToolsActive)
    }

    /// The fullscreen state the page is currently in.
    public var fullscreenState: WebPage.FullscreenState {
        backingProperty(\.fullscreenState, backedBy: \.fullscreenState) { backingValue in
            WebPage.FullscreenState(backingValue)
        }
    }

    /// Indicates whether the webpage is using the camera to capture images or video.
    public var cameraCaptureState: WKMediaCaptureState {
        backingProperty(\.cameraCaptureState, backedBy: \.cameraCaptureState)
    }

    /// Indicates whether the webpage is using the microphone to capture audio.
    public var microphoneCaptureState: WKMediaCaptureState {
        backingProperty(\.microphoneCaptureState, backedBy: \.microphoneCaptureState)
    }

    /// The media type for the contents of the web view.
    ///
    /// When the value of this property is `nil`, the webpage derives the current media type from the CSS
    /// media property of its content. If you assign a value other than `nil` to this property, the webpage
    /// uses the value you provide instead.
    ///
    /// The default value of this property is `nil`.
    public var mediaType: WebPage.CSSMediaType? {
        get { backingWebView.mediaType.map(CSSMediaType.init(rawValue:)) }
        set { backingWebView.mediaType = newValue?.rawValue }
    }

    /// The custom user agent string.
    ///
    /// Use this property to specify a custom user agent string for the webpage.
    ///
    /// The default value of this property is `nil`.
    public var customUserAgent: String? {
        get { backingWebView.customUserAgent }
        set { backingWebView.customUserAgent = newValue }
    }

    /// Indicates whether you can inspect the view with Safari Web Inspector.
    ///
    /// Set to true at any point in the view’s lifetime to allow Safari Web Inspector access to inspect the view’s content.
    /// Then, select your view in Safari’s Develop menu for either your computer or an attached device to inspect it.
    ///
    /// If you set this value to false during inspection, the system immediately closes Safari Web Inspector and does not
    /// provide any further information about the web content.
    ///
    /// The default value of this property is `false`.
    public var isInspectable: Bool {
        get { backingWebView.isInspectable }
        set { backingWebView.isInspectable = newValue }
    }

    let backingUIDelegate: WKUIDelegateAdapter
    private let backingNavigationDelegate: WKNavigationDelegateAdapter
    let backingDownloadDelegate: WKDownloadDelegateAdapter

#if os(macOS)
    @_spi(CrossImportOverlay)
    public func setMenuBuilder(_ menuBuilder: ((WebPage.ElementInfo) -> NSMenu)?) {
        backingUIDelegate.menuBuilder = menuBuilder
    }
#endif

    @ObservationIgnored
    private var observations = KeyValueObservations()

    @ObservationIgnored
    @_spi(CrossImportOverlay)
    public var isBoundToWebView = false

    @ObservationIgnored
    @_spi(CrossImportOverlay)
    public lazy var backingWebView: WebPageWebView = {
        let webView = WebPageWebView(frame: .zero, configuration: WKWebViewConfiguration(configuration))
        webView.navigationDelegate = backingNavigationDelegate
        webView.uiDelegate = backingUIDelegate
#if os(macOS)
        webView._usePlatformFindUI = false
#endif
        return webView
    }()

    // MARK: Loading functions

    /// Loads the web content that the specified URL request object references and navigates to that content.
    ///
    /// Use this method to load a page from a local or network-based URL. For example, you might use this method
    /// to navigate to a network-based webpage.
    ///
    /// Provide the source of this load request for app activity data by setting the attribution parameter on your request.
    ///
    /// - Parameter request: A URL request that specifies the resource to display.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(_ request: URLRequest) -> NavigationID? {
        backingWebView.load(request).map(NavigationID.init(_:))
    }

    /// Loads the content of the specified data object and navigates to it.
    ///
    /// Use this method to navigate to a webpage that you loaded yourself and saved in a data object. For example,
    /// if you previously wrote HTML content to a data object, use this method to navigate to that content.
    ///
    /// - Parameters:
    ///   - data: The data to use as the contents of the webpage.
    ///   - mimeType: The MIME type of the information in the data parameter. This parameter must not contain an empty string.
    ///   - encoding: The data's character encoding.
    ///   - baseURL: A URL that you use to resolve relative URLs within the document.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(_ data: Data, mimeType: String, characterEncoding: String.Encoding, baseURL: URL) -> NavigationID? {
        let cfEncoding = CFStringConvertNSStringEncodingToEncoding(characterEncoding.rawValue)
        guard cfEncoding != kCFStringEncodingInvalidId else {
            preconditionFailure("\(characterEncoding) is not a valid character encoding")
        }

        guard let convertedEncoding = CFStringConvertEncodingToIANACharSetName(cfEncoding) as? String else {
            preconditionFailure("\(characterEncoding) is not a valid character encoding")
        }

        return backingWebView.load(data, mimeType: mimeType, characterEncodingName: convertedEncoding, baseURL: baseURL).map(NavigationID.init(_:))
    }

    /// Loads the contents of the specified HTML string and navigates to it.
    ///
    /// Use this method to navigate to a webpage that you loaded or created yourself. For example, you might use
    /// this method to load HTML content that your app generates programmatically.
    ///
    /// This method sets the source of this load request for app activity data to NSURLRequest.Attribution.developer.
    ///
    /// - Parameters:
    ///   - html: The string to use as the contents of the webpage.
    ///   - baseURL: The base URL to use when the system resolves relative URLs within the HTML string.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(html: String, baseURL: URL) -> NavigationID? {
        backingWebView.loadHTMLString(html, baseURL: baseURL).map(NavigationID.init(_:))
    }

    /// Loads the web content from the specified file and navigates to it.
    ///
    /// - Parameters:
    ///   - fileURL: The URL of a file that contains web content. This URL must be a file-based URL.
    ///   - readAccessURL: The URL of a file or directory containing web content that you grant the system permission
    ///   to read. This URL must be a file-based URL and must not be empty. To prevent WebKit from reading any other
    ///   content, specify the same value as the URL parameter. To read additional files related to the content file,
    ///   specify a directory.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(fileURL url: URL, allowingReadAccessTo readAccessURL: URL) -> NavigationID? {
        backingWebView.loadFileURL(url, allowingReadAccessTo: readAccessURL).map(NavigationID.init(_:))
    }

    /// Loads the web content from the file the URL request object specifies and navigates to that content.
    ///
    /// Provide the source of this load request for app activity data by setting the `attribution` parameter on your request.
    ///
    /// - Parameters:
    ///   - request: A URL request that specifies the file to display. The URL in this request must be a file-based URL.
    ///   - readAccessURL: The URL of a file or directory containing web content that you grant the system permission
    ///   to read. This URL must be a file-based URL and must not be empty. To prevent WebKit from reading any other
    ///   content, specify the same value as the URL parameter. To read additional files related to the content file,
    ///   specify a directory.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(fileRequest request: URLRequest, allowingReadAccessTo readAccessURL: URL) -> NavigationID? {
        // `WKWebView` annotates this method as returning non-nil, but it may return nil.

        let navigation = backingWebView.loadFileRequest(request, allowingReadAccessTo: readAccessURL) as WKNavigation?
        return navigation.map(NavigationID.init(_:))
    }

    /// Loads the web content from the data you provide as if the data were the response to the request.
    ///
    /// - Parameters:
    ///   - request: A URL request that specifies the base URL and other loading details the system uses to interpret the data you provide.
    ///   - response: A response the system uses to interpret the data you provide.
    ///   - responseData: The data to use as the contents of the webpage.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(simulatedRequest request: URLRequest, response: URLResponse, responseData: Data) -> NavigationID? {
        // `WKWebView` annotates this method as returning non-nil, but it may return nil.

        let navigation = backingWebView.loadSimulatedRequest(request, response: response, responseData: responseData) as WKNavigation?
        return navigation.map(NavigationID.init(_:))
    }

    /// Loads the web content from the HTML you provide as if the HTML were the response to the request.
    ///
    /// - Parameters:
    ///   - request: A URL request that specifies the base URL and other loading details the system uses to interpret the HTML you provide.
    ///   - htmlString: The HTML code you provide in a string to use as the contents of the webpage.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(simulatedRequest request: URLRequest, responseHTML htmlString: String) -> NavigationID? {
        // `WKWebView` annotates this method as returning non-nil, but it may return nil.

        let navigation = backingWebView.loadSimulatedRequest(request, responseHTML: htmlString) as WKNavigation?
        return navigation.map(NavigationID.init(_:))
    }

    /// Navigates to an item from the back-forward list and sets it as the current item.
    ///
    /// - Parameters:
    ///   - item: The item to navigate to. The item must be in the webpage's back-forward list.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func load(_ item: BackForwardList.Item) -> NavigationID? {
        backingWebView.go(to: item.wrapped).map(NavigationID.init(_:))
    }

    /// Reloads the current webpage.
    ///
    /// - Parameter fromOrigin: If `true`, end-to-end revalidation of the content using cache-validating conditionals
    /// is performed, if possible.
    /// - Returns: A navigation identifier you use to track the loading progress of the request.
    @discardableResult
    public func reload(fromOrigin: Bool = false) -> NavigationID? {
        let navigation = fromOrigin ? backingWebView.reloadFromOrigin() : backingWebView.reload()
        return navigation.map(NavigationID.init(_:))
    }

    /// Stops loading all resources on the current page.
    public func stopLoading() {
        backingWebView.stopLoading()
    }

    // MARK: Utility functions

    /// Executes the specified string as an async JavaScript function.
    ///
    /// Don’t format the string in the functionBody parameter as a function-like callable object, as you would in pure
    /// JavaScript. Instead, put only the body of the function in the string. For example, the following string shows a valid function body that takes x, y, and z arguments and returns a result.
    ///
    /// ```javascript
    /// return x ? y : z;
    /// ```
    ///
    /// If your JavaScript code returns an object with a callable then property, WebKit calls that property on
    /// the resulting object and waits for its resolution. If resolution succeeds, WebKit returns the resulting
    /// object. If resolution fails, WebKit throws a `WKErrorJavaScriptAsyncFunctionResultRejected` error. If the
    /// garbage collector reclaims the object before resolution finishes, WebKit throws a `WKErrorJavaScriptAsyncFunctionResultUnreachable` error.
    ///
    /// Because this method calls your JavaScript code asynchronously, you can call `await` on objects with a
    /// `then` property inside your function body. The following code example illustrates this technique.
    ///
    /// ```javascript
    /// var p = new Promise(function (f) {
    ///   window.setTimeout("f(42)", 1000);
    /// });
    /// await p;
    /// return p;
    /// ```
    ///
    /// - Parameters:
    ///   - functionBody: The JavaScript string to use as the function body. This method treats the string as an anonymous
    ///   JavaScript function body and calls it with the named arguments in the `arguments` parameter.
    ///
    ///   - arguments: A dictionary of the arguments to pass to the function call. Each key in the dictionary corresponds
    ///   to the name of an argument in the `functionBody` string, and the value of that key is the value to use during
    ///   the evaluation of the code. Supported value types are `Numeric`, `String`, `Date`, and arrays, dictionaries,
    ///   and optional values of those types.
    ///
    ///   - frame: The frame in which to evaluate the JavaScript code. Specify `nil` to target the main frame. If this
    ///   frame is no longer valid when script evaluation begins, this function throws an error with the
    ///   `WKError.Code.javaScriptInvalidFrameTarget` code.
    ///
    ///   - contentWorld: The namespace in which to evaluate the JavaScript code. THis parameter doesn't apply to changes
    ///   you make in the underlying web content, such as the document's DOM structure. Those changes remain visible to
    ///   all scripts, regardless of which content world you specify. For more information about content worlds, see `WKContentWorld`.
    ///
    /// - Returns: The result of the script evaluation. If your function body doesn't return an explicit value, `nil` is returned.
    ///  If your function body explicitly returns `null`, then `NSNull` is returned.
    public func callJavaScript(_ functionBody: String, arguments: [String : Any] = [:], in frame: FrameInfo? = nil, contentWorld: WKContentWorld? = nil) async throws -> Any? {
        try await backingWebView.callAsyncJavaScript(functionBody, arguments: arguments, in: frame?.wrapped, contentWorld: contentWorld ?? .page)
    }

    /// Generates PDF data from the webpage's contents
    /// - Parameter configuration: The object that specifies the portion of the web view to capture as PDF data.
    /// - Returns: A data object that contains the PDF data to use for rendering the contents of the webpage.
    public func pdf(configuration: WKPDFConfiguration = .init()) async throws -> Data {
        try await backingWebView.pdf(configuration: configuration)
    }

    /// Creates a web archive of the webpage's current contents.
    public func webArchiveData() async throws -> Data {
        try await withCheckedThrowingContinuation { continuation in
            backingWebView.createWebArchiveData {
                continuation.resume(with: $0)
            }
        }
    }

    // MARK: Media functions

    /// Pauses playback of all media in the web view.
    public func pauseAllMediaPlayback() async {
        await backingWebView.pauseAllMediaPlayback()
    }

    /// Determine the playback status of media in the page.
    /// - Returns: The current state of media playback within the page.
    public func mediaPlaybackState() async -> WKMediaPlaybackState {
        await backingWebView.requestMediaPlaybackState()
    }

    /// Changes whether the webpage is suspending playback of all media in the page.
    /// - Parameter suspended: Indicates whether the webpage should suspend media playback.
    public func setAllMediaPlaybackSuspended(_ suspended: Bool) async {
        await backingWebView.setAllMediaPlaybackSuspended(suspended)
    }

    /// Closes all media the webpage is presenting, including picture-in-picture video and fullscreen video.
    public func closeAllMediaPresentations() async {
        await backingWebView.closeAllMediaPresentations()
    }

    /// Changes whether the webpage is using the camera to capture images or video.
    /// - Parameter state: The new capture state the page should use.
    public func setCameraCaptureState(_ state: WKMediaCaptureState) async {
        await backingWebView.setCameraCaptureState(state)
    }

    /// Changes whether the webpage is using the microphone to capture audio.
    /// - Parameter state: The new capture state the page should use.
    public func setMicrophoneCaptureState(_ state: WKMediaCaptureState) async {
        await backingWebView.setMicrophoneCaptureState(state)
    }

    // MARK: Downloads

    // For these to work, a custom implementation of `DownloadCoordinator.destination(forDownload:response:suggestedFilename:) async -> URL?`
    // must be provided so that the downloads are not immediately cancelled.

    @_spi(Private)
    public func startDownload(using request: URLRequest) async -> WebPage.DownloadID {
        let cocoaDownload = await backingWebView.startDownload(using: request)
        return WebPage.DownloadID(cocoaDownload)
    }

    @_spi(Private)
    public func resumeDownload(fromResumeData resumeData: Data) async -> WebPage.DownloadID {
        let cocoaDownload = await backingWebView.resumeDownload(fromResumeData: resumeData)
        return WebPage.DownloadID(cocoaDownload)
    }

    // MARK: Private helper functions

    private func createObservation<Value, BackingValue>(for keyPath: KeyPath<WebPage, Value>, backedBy backingKeyPath: KeyPath<WebPageWebView, BackingValue>) -> NSKeyValueObservation {
        let boxed = UncheckedSendableKeyPathBox(keyPath: keyPath)

        return backingWebView.observe(backingKeyPath, options: [.prior, .old, .new]) { [_$observationRegistrar, unowned self] _, change in
            if change.isPrior {
                _$observationRegistrar.willSet(self, keyPath: boxed.keyPath)
            } else {
                _$observationRegistrar.didSet(self, keyPath: boxed.keyPath)
            }
        }
    }

    @_spi(CrossImportOverlay)
    public func backingProperty<Value, BackingValue>(_ keyPath: KeyPath<WebPage, Value>, backedBy backingKeyPath: KeyPath<WebPageWebView, BackingValue>, _ transform: (BackingValue) -> Value) -> Value {
        if observations.contents[keyPath] == nil {
            observations.contents[keyPath] = createObservation(for: keyPath, backedBy: backingKeyPath)
        }

        self.access(keyPath: keyPath)

        let backingValue = backingWebView[keyPath: backingKeyPath]
        return transform(backingValue)
    }

    @_spi(CrossImportOverlay)
    public func backingProperty<Value>(_ keyPath: KeyPath<WebPage, Value>, backedBy backingKeyPath: KeyPath<WebPageWebView, Value>) -> Value {
        backingProperty(keyPath, backedBy: backingKeyPath) { $0 }
    }
}

extension WebPage.FullscreenState {
    init(_ wrapped: WKWebView.FullscreenState) {
        self = switch wrapped {
        case .enteringFullscreen: .enteringFullscreen
        case .exitingFullscreen: .exitingFullscreen
        case .inFullscreen: .inFullscreen
        case .notInFullscreen: .notInFullscreen
        @unknown default:
            fatalError()
        }
    }
}

extension WebPage {
    private struct KeyValueObservations: ~Copyable {
        var contents: [PartialKeyPath<WebPage> : NSKeyValueObservation] = [:]

        deinit {
            for (_, observation) in contents {
                observation.invalidate()
            }
        }
    }
}

/// The key path used within `createObservation` must be Sendable.
/// This is safe as long as it is not used for object subscripting and isn't created with captured subscript key paths.
fileprivate struct UncheckedSendableKeyPathBox<Root, Value>: @unchecked Sendable {
    let keyPath: KeyPath<Root, Value>
}

#endif
