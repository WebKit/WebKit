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
internal import WebKit_Internal

/// An object that controls and manages the behavior of interactive web content.
///
/// A ``WebPage`` is an ``Observable`` type, which you use to access various properties of web content
/// and track changes to them. Use ``WebPage`` to interact with web content, like evaluating JavaScript
/// or converting the page to PDF data. The following example shows you how you can combine these
/// capabilities to get specific metadata from an ephemeral page with a custom user agent:
///
/// ```swift
/// func fetchMetadata(for url: URL) async throws -> (title: String, description: String) {
///     let botAgent = """
///     Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_1) AppleWebKit/601.2.4 (KHTML, like Gecko) Version/9.0.1 Safari/601.2.4 facebookexternalhit/1.1 Facebot Twitterbot/1.0
///     """
///
///     var configuration = WebPage.Configuration()
///     configuration.loadsSubresources = false
///     configuration.defaultNavigationPreferences.allowsContentJavaScript = false
///     configuration.websiteDataStore = .nonPersistent()
///
///     // Set up the configured page.
///
///     let page = WebPage(configuration: configuration)
///     page.customUserAgent = botAgent
///
///     // Load the request and wait for navigation to complete.
///
///     let request = URLRequest(url: url)
///     for try await event in page.load(request) {
///         // Optionally do something with `event`.
///     }
///
///     // At this point, the navigation is complete.
///     // Now, use JavaScript to query the appropriate properties of the page.
///
///     let fetchOpenGraphProperty = """
///     const propertyValues = document.querySelectorAll(`meta[property="${property}"]`);
///     return propertyValues[0];
///     """
///
///     let javaScriptResult = try await page.callJavaScript(fetchOpenGraphProperty, arguments: arguments)
///     guard let description = javaScriptResult as? String else {
///         // Handle failure, like throwing an error.
///     }
///
///     guard let title = page.title else {
///         // Handle failure, like throwing an error.
///     }
///
///     return (title, description)
/// }
/// ```
///
/// Use ``WebPage`` to programmatically navigate to various types of resources like URL requests,
/// HTML strings, and data. Optionally, you can observe these navigations through the async sequence
/// returned by their associated loading functions, and you can customize them by using a type that
/// conforms to the ``WebPage/NavigationDeciding`` protocol. You can also use the ``WebPage/backForwardList``
/// property to observe changes to people’s navigation history, and to programmatically navigate to a
/// specific back-forward list item.
///
/// ``WebPage`` also conforms to the ``Transferable`` protocol. You can use this conformance to export the
/// page to various different types of content, like PDF, web archive data, and other types. For customization
/// of PDF or image export, use ``WebPage/exported(as:)``.
@MainActor
@Observable
@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
#if compiler(>=6.2.3)
@_expose(!Cxx)
#endif
final public class WebPage {
    /// A CSS media type as defined by the [CSS specification](https://www.w3.org/TR/mediaqueries-4/#media-types), or an arbitrary media type value.
    ///
    /// Media types are one of several media queries that influence the `@media` CSS at-rule; this rule is used
    /// by webpages to apply parts of a style sheet depending on the media properties specified.
    ///
    /// You can customize the media type of a ``WebPage`` by using the ``WebPage/mediaType`` property.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct CSSMediaType: Hashable, RawRepresentable, Sendable {
        /// Corresponds to the "all" media type.
        public static let all = CSSMediaType(rawValue: "all")

        /// Corresponds to the "screen" media type.
        public static let screen = CSSMediaType(rawValue: "screen")

        /// Corresponds to the "print" media type.
        public static let print = CSSMediaType(rawValue: "print")

        /// Create a media type with an arbitrary value.
        ///
        /// Use the static type properties for the defined canonical CSS media type options.
        ///
        /// - Parameter rawValue: The raw value of the media type.
        public init(rawValue: Swift.String) {
            self.rawValue = rawValue
        }

        /// The raw value of the media type.
        public let rawValue: Swift.String
    }

    /// The set of possible fullscreen states a webpage may be in.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum FullscreenState: Hashable, Sendable {
        /// The page is entering fullscreen.
        case enteringFullscreen

        /// The page is exiting fullscreen.
        case exitingFullscreen

        /// The page is currently in fullscreen.
        case inFullscreen

        /// The page is not currently in fullscreen.
        case notInFullscreen
    }

    // This is based on the XGA standard resolution size.
    private static let defaultFrame = CGRect(x: 0, y: 0, width: 1024, height: 768)

    // MARK: Initializers

    private init(
        internalHelperWithConfiguration configuration: Configuration,
        navigationDecider: (any NavigationDeciding)?,
        dialogPresenter: (any DialogPresenting)?,
    ) {
        self.configuration = configuration

        backingUIDelegate = WKUIDelegateAdapter(
            dialogPresenter: dialogPresenter
        )
        backingNavigationDelegate = WKNavigationDelegateAdapter(
            navigationDecider: navigationDecider
        )

        backingUIDelegate.owner = self
        backingNavigationDelegate.owner = self
    }

    /// Create a new WebPage.
    ///
    /// - Parameters:
    ///   - configuration: A ``WebPage/Configuration`` value to use when initializing the page.
    ///   - navigationDecider: A navigation decider used to customize navigations that happen within the page.
    ///   - dialogPresenter: A dialog presenter which controls how JavaScript dialogs are handled.
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding,
        dialogPresenter: some DialogPresenting
    ) {
        self.init(internalHelperWithConfiguration: configuration, navigationDecider: navigationDecider, dialogPresenter: dialogPresenter)
    }

    /// Create a new WebPage.
    ///
    /// - Parameters:
    ///   - configuration: A ``WebPage/Configuration`` value to use when initializing the page.
    ///   - dialogPresenter: A dialog presenter which controls how JavaScript dialogs are handled.
    public convenience init(
        configuration: Configuration = Configuration(),
        dialogPresenter: some DialogPresenting
    ) {
        self.init(internalHelperWithConfiguration: configuration, navigationDecider: nil, dialogPresenter: dialogPresenter)
    }

    /// Create a new WebPage.
    ///
    /// - Parameters:
    ///   - configuration: A ``WebPage/Configuration`` value to use when initializing the page.
    ///   - navigationDecider: A navigation decider used to customize navigations that happen within the page.
    public convenience init(
        configuration: Configuration = Configuration(),
        navigationDecider: some NavigationDeciding
    ) {
        self.init(internalHelperWithConfiguration: configuration, navigationDecider: navigationDecider, dialogPresenter: nil)
    }

    /// Create a new WebPage.
    ///
    /// - Parameter configuration: A ``WebPage/Configuration`` value to use when initializing the page.
    public convenience init(
        configuration: Configuration = Configuration(),
    ) {
        self.init(internalHelperWithConfiguration: configuration, navigationDecider: nil, dialogPresenter: nil)
    }

    // MARK: Properties

    let configuration: Configuration

    /// The webpage's back-forward list.
    public internal(set) var backForwardList: BackForwardList = BackForwardList()

    /// A sequence of all the navigation events that occur throughout the webpage, including both user navigation
    /// and programmatic navigation.
    ///
    /// A specific navigation is comprised of a sequential set of ``NavigationEvent``s; a new navigation begins when an
    /// event is ``NavigationEvent/startedProvisionalNavigation``.
    ///
    /// This property produces a new sequence each time it is called, and starts tracking events as soon as it is created.
    /// The sequence is indefinite, but may be terminated under several circumstances:
    ///
    /// * The owning ``WebPage``'s lifetime ends.
    /// * An error occurs during navigation, at which point the sequence will throw the error and terminate.
    /// * The ``Task`` enclosing iteration of the sequence is cancelled.
    ///
    /// To track a specific programmatic navigation, use the return value of one of the loading APIs.
    public var navigations: some AsyncSequence<NavigationEvent, any Error> {
        createIndefiniteNavigationSequence()
    }

    /// The URL for the current webpage.
    ///
    /// This property contains the URL for the webpage currently being presented. Use this URL in places
    /// where you reflect the webpage address in your app’s user interface. If the webpage has not loaded
    /// any content yet, this value will be `nil`.
    public var url: Foundation.URL? {
        backingProperty(\.url, backedBy: \.url)
    }

    /// The page title.
    public var title: Swift.String {
        backingProperty(\.title, backedBy: \.title) { backingValue in
            // The title property is annotated as optional in WKWebView, but is never actually `nil`.
            // swift-format-ignore: NeverForceUnwrap
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
    /// - Returns: `true` if the page is still loading content, otherwise, `false`.
    public var isLoading: Bool {
        backingProperty(\.isLoading, backedBy: \.isLoading)
    }

    /// The trust management object you use to evaluate trust for the current webpage.
    ///
    /// Use the object in this property to validate the webpage’s certificate and associated credentials.
    /// See <doc://com.apple.documentation/documentation/security/evaluating-a-trust-and-parsing-the-result>
    /// for more details on how to use the trust.
    public var serverTrust: SecTrust? {
        backingProperty(\.serverTrust, backedBy: \.serverTrust)
    }

    /// Indicates whether the webpage loaded all resources on the page through securely encrypted connections.
    public var hasOnlySecureContent: Bool {
        backingProperty(\.hasOnlySecureContent, backedBy: \.hasOnlySecureContent)
    }

    /// Indicates whether Writing Tools is active for the page.
    public var isWritingToolsActive: Bool {
        backingProperty(\.isWritingToolsActive, backedBy: \.isWritingToolsActive)
    }

    /// Indicates whether Screen Time blocking has occurred.
    @available(visionOS, unavailable)
    public var isBlockedByScreenTime: Bool {
        backingProperty(\.isBlockedByScreenTime, backedBy: \.isBlockedByScreenTime)
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

    /// The media type for the contents of the webpage.
    ///
    /// When the value of this property is `nil`, the webpage derives the current media type from the CSS
    /// media property of its content. If you assign a value other than `nil` to this property, the webpage
    /// uses the value you provide instead.
    ///
    /// For example, you can use this property to configure a page for viewing as a print preview by setting
    /// it to ``WebPage/CSSMediaType/print``.
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
    public var customUserAgent: Swift.String? {
        get { backingWebView.customUserAgent }
        set { backingWebView.customUserAgent = newValue }
    }

    /// Indicates whether you can inspect the page with Safari Web Inspector.
    ///
    /// Set to `true` at any point in the page's lifetime to allow Safari Web Inspector access to inspect the view’s content.
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

    #if os(macOS)
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func setMenuBuilder(_ menuBuilder: ((WKContextMenuElementInfoAdapter) -> NSMenu)?) {
        backingUIDelegate.menuBuilder = menuBuilder
    }
    #endif

    @ObservationIgnored
    private var observations = KeyValueObservations()

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @ObservationIgnored
    @_spi(CrossImportOverlay)
    public var isBoundToWebView = false {
        didSet {
            backingWebView.frame = isBoundToWebView ? .zero : Self.defaultFrame
        }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @ObservationIgnored
    @_spi(CrossImportOverlay)
    public lazy var backingWebView: WebPageWebView = {
        let webView = WebPageWebView(frame: Self.defaultFrame, configuration: WKWebViewConfiguration(configuration))
        webView.navigationDelegate = backingNavigationDelegate
        webView.uiDelegate = backingUIDelegate
        #if os(macOS)
        webView._usePlatformFindUI = false
        #endif
        return webView
    }()

    #if os(macOS)
    // SPI for testing.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(Testing)
    public var smartListsEnabled: Bool {
        get { backingWebView._isSmartListsEnabled() }
        set { backingWebView._setSmartListsEnabled(newValue) }
    }
    #endif

    // MARK: Loading functions

    @ObservationIgnored
    private var scopedNavigations: [ObjectIdentifier: AsyncThrowingStream<NavigationEvent, any Error>.Continuation] = [:]

    @ObservationIgnored
    private var scopedStreams: [ObjectIdentifier: AsyncThrowingStream<NavigationEvent, any Error>] = [:]

    @ObservationIgnored
    private var indefiniteNavigations: [UUID: AsyncThrowingStream<NavigationEvent, any Error>.Continuation] = [:]

    /// Loads the web content that the specified URL references and navigates to that content.
    ///
    /// Use this method to load a page from a local or network-based URL. For example, you might use this method
    /// to navigate to a network-based webpage.
    ///
    /// - Parameter url: The URL to load. If this is `nil`, an error will be immediately thrown from the returned sequence.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(_ url: Foundation.URL?) -> some AsyncSequence<NavigationEvent, any Error> {
        guard let url else {
            return AsyncThrowingStream { continuation in
                continuation.finish(throwing: NavigationError.invalidURL)
            }
        }

        return toNavigationSequence { $0.load(URLRequest(url: url)) }
    }

    /// Loads the web content that the specified URL request object references and navigates to that content.
    ///
    /// Use this method to load a page from a local or network-based URL. For example, you might use this method
    /// to navigate to a network-based webpage.
    ///
    /// Provide the source of this load request for app activity data by setting the attribution parameter on your request.
    ///
    /// - Parameter request: A URL request that specifies the resource to display.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(_ request: URLRequest) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence { $0.load(request) }
    }

    /// Loads the content of the specified data object and navigates to it.
    ///
    /// Use this method to navigate to a webpage that you loaded yourself and saved in a data object. For example,
    /// if you previously wrote HTML content to a data object, use this method to navigate to that content.
    ///
    /// - Parameters:
    ///   - data: The data to use as the contents of the webpage.
    ///   - mimeType: The MIME type of the information in the data parameter. This parameter must not contain an empty string.
    ///   - characterEncoding: The data's character encoding.
    ///   - baseURL: A URL that you use to resolve relative URLs within the document.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(
        _ data: Data,
        mimeType: Swift.String,
        characterEncoding: Swift.String.Encoding,
        baseURL: Foundation.URL
    ) -> some AsyncSequence<NavigationEvent, any Error> {
        let cfEncoding = CFStringConvertNSStringEncodingToEncoding(characterEncoding.rawValue)
        guard cfEncoding != kCFStringEncodingInvalidId else {
            preconditionFailure("\(characterEncoding) is not a valid character encoding")
        }

        guard let convertedEncoding = CFStringConvertEncodingToIANACharSetName(cfEncoding) as? String else {
            preconditionFailure("\(characterEncoding) is not a valid character encoding")
        }

        return toNavigationSequence {
            $0.load(data, mimeType: mimeType, characterEncodingName: convertedEncoding, baseURL: baseURL)
        }
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
    ///   - baseURL: The base URL to use when the system resolves relative URLs within the HTML string. By default, this is `about:blank`.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    // swift-format-ignore: NeverForceUnwrap
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @discardableResult
    public func load(
        html: Swift.String,
        baseURL: Foundation.URL = Foundation.URL(string: "about:blank")!
    ) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence {
            $0.loadHTMLString(html, baseURL: baseURL)
        }
    }

    /// Loads the web content from the data you provide as if the data were the response to the request.
    ///
    /// - Parameters:
    ///   - request: A URL request that specifies the base URL and other loading details the system uses to interpret the data you provide.
    ///   - response: A response the system uses to interpret the data you provide.
    ///   - responseData: The data to use as the contents of the webpage.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(
        simulatedRequest request: URLRequest,
        response: URLResponse,
        responseData: Data
    ) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence {
            // `WKWebView` annotates this method as returning non-nil, but it may return nil.
            $0.loadSimulatedRequest(request, response: response, responseData: responseData) as WKNavigation?
        }
    }

    /// Loads the web content from the HTML you provide as if the HTML were the response to the request.
    ///
    /// - Parameters:
    ///   - request: A URL request that specifies the base URL and other loading details the system uses to interpret the HTML you provide.
    ///   - htmlString: The HTML code you provide in a string to use as the contents of the webpage.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(
        simulatedRequest request: URLRequest,
        responseHTML htmlString: Swift.String
    ) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence {
            // `WKWebView` annotates this method as returning non-nil, but it may return nil.
            $0.loadSimulatedRequest(request, responseHTML: htmlString) as WKNavigation?
        }
    }

    /// Navigates to an item from the back-forward list and sets it as the current item.
    ///
    /// - Parameter item: The item to navigate to. The item must be in the webpage's back-forward list.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func load(_ item: BackForwardList.Item) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence {
            $0.go(to: item.wrapped)
        }
    }

    /// Reloads the current webpage.
    ///
    /// - Parameter fromOrigin: If `true`, end-to-end revalidation of the content using cache-validating conditionals
    /// is performed, if possible.
    /// - Returns: An async sequence you use to track the loading progress of the navigation. If the `Task` enclosing the sequence is cancelled, the page will stop loading all resources.
    @discardableResult
    public func reload(fromOrigin: Bool = false) -> some AsyncSequence<NavigationEvent, any Error> {
        toNavigationSequence {
            fromOrigin ? $0.reloadFromOrigin() : $0.reload()
        }
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
    ///   - contentWorld: The namespace in which to evaluate the JavaScript code. This parameter doesn't apply to changes
    ///   you make in the underlying web content, such as the document's DOM structure. Those changes remain visible to
    ///   all scripts, regardless of which content world you specify. For more information about content worlds, see `WKContentWorld`.
    ///
    /// - Returns: The result of the script evaluation. If your function body doesn't return an explicit value, `nil` is returned. If your function body explicitly returns `null`, then `NSNull` is returned.
    /// - Throws: An error if a problem occurred while evaluating the JavaScript.
    @discardableResult
    public func callJavaScript(
        _ functionBody: Swift.String,
        arguments: [Swift.String: Any] = [:],
        in frame: FrameInfo? = nil,
        contentWorld: WKContentWorld? = nil
    ) async throws -> sending Any? {
        let result = try await backingWebView.callAsyncJavaScript(
            functionBody,
            arguments: arguments,
            in: frame?.wrapped,
            contentWorld: contentWorld ?? .page
        )

        guard let result else {
            return nil
        }

        // Safe force-unwrap because all plist types are Sendable.
        // swift-format-ignore: NeverForceUnwrap
        return result as! any Sendable
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

    // MARK: Helper functions

    func addNavigationEvent(_ event: Result<NavigationEvent, any Error>, for cocoaNavigation: WKNavigation?) {
        if let cocoaNavigation {
            scopedNavigations[ObjectIdentifier(cocoaNavigation)]?.yield(with: event)

            if case .success(.finished) = event {
                scopedNavigations[ObjectIdentifier(cocoaNavigation)]?.finish()
            }
        } else {
            for continuation in scopedNavigations.values {
                continuation.yield(with: event)
            }
        }

        for continuation in indefiniteNavigations.values {
            continuation.yield(with: event)
        }
    }

    private func createIndefiniteNavigationSequence() -> some AsyncSequence<NavigationEvent, any Error> {
        let id = UUID()

        let (stream, continuation) = AsyncThrowingStream.makeStream(of: NavigationEvent.self, throwing: (any Error).self)
        continuation.onTermination = { [weak self] termination in
            guard let self else {
                return
            }
            Task { @MainActor in
                // `stopLoading` is intentionally not called here because the semantics of doing
                // so would not be well-defined in the case of multiple navigation sequences.
                indefiniteNavigations[id] = nil
            }
        }

        indefiniteNavigations[id] = continuation
        return stream
    }

    private func toNavigationSequence(_ load: (WKWebView) -> WKNavigation?) -> AsyncThrowingStream<NavigationEvent, any Error> {
        guard let id = load(backingWebView) else {
            return AsyncThrowingStream { continuation in
                continuation.finish(throwing: NavigationError.pageClosed)
            }
        }

        let (stream, continuation) = AsyncThrowingStream.makeStream(of: NavigationEvent.self, throwing: (any Error).self)
        continuation.onTermination = { [weak self] termination in
            guard let self else {
                return
            }
            Task { @MainActor in
                if case .cancelled = termination {
                    stopLoading()
                }
                scopedNavigations[ObjectIdentifier(id)] = nil
                scopedStreams[ObjectIdentifier(id)] = nil
            }
        }

        scopedNavigations[ObjectIdentifier(id)] = continuation
        scopedStreams[ObjectIdentifier(id)] = stream
        return stream
    }

    private func createObservation<Value, BackingValue>(
        for keyPath: KeyPath<WebPage, Value>,
        backedBy backingKeyPath: KeyPath<WebPageWebView, BackingValue>
    ) -> NSKeyValueObservation {
        // The key path used within `createObservation` must be Sendable.
        // This is safe as long as it is not used for object subscripting and isn't created with captured subscript key paths.
        let boxed = UncheckedSendableKeyPathBox(keyPath: keyPath)

        return backingWebView.observe(backingKeyPath, options: [.prior, .old, .new]) { [_$observationRegistrar, unowned self] _, change in
            if change.isPrior {
                _$observationRegistrar.willSet(self, keyPath: boxed.keyPath)
            } else {
                _$observationRegistrar.didSet(self, keyPath: boxed.keyPath)
            }
        }
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func backingProperty<Value, BackingValue>(
        _ keyPath: KeyPath<WebPage, Value>,
        backedBy backingKeyPath: KeyPath<WebPageWebView, BackingValue>,
        _ transform: (BackingValue) -> Value
    ) -> Value {
        if observations.contents[keyPath] == nil {
            observations.contents[keyPath] = createObservation(for: keyPath, backedBy: backingKeyPath)
        }

        self.access(keyPath: keyPath)

        let backingValue = backingWebView[keyPath: backingKeyPath]
        return transform(backingValue)
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func backingProperty<Value>(_ keyPath: KeyPath<WebPage, Value>, backedBy backingKeyPath: KeyPath<WebPageWebView, Value>) -> Value
    {
        backingProperty(keyPath, backedBy: backingKeyPath) { $0 }
    }
}

extension WebPage.FullscreenState {
    init(_ wrapped: WKWebView.FullscreenState) {
        self =
            switch wrapped {
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
        var contents: [PartialKeyPath<WebPage>: NSKeyValueObservation] = [:]

        deinit {
            for (_, observation) in contents {
                observation.invalidate()
            }
        }
    }
}

extension WebPage {
    // SPI for testing.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(Testing)
    public func terminateWebContentProcess() {
        backingWebView._killWebContentProcess()
    }
}

#endif
