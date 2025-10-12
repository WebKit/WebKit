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
internal import WebKit_Internal

/// A type representing a valid URL scheme.
///
/// Scheme names are case sensitive, must start with an ASCII letter, and may contain only ASCII letters,
/// numbers, the “+” character, the “-” character, and the “.” character.
@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public struct URLScheme: Hashable, Sendable {
    /// Creates a new `URLScheme` value from a valid scheme, which WebKit does not already handle.
    ///
    /// To determine whether WebKit handles a specific scheme, call the `handlesURLScheme(_:)` static method of `WebPage`.
    ///
    /// - Parameter rawValue: The raw value of the scheme string; if this is an invalid scheme, of if WebKit already handles
    /// this scheme, the initializer returns `nil`.
    @MainActor
    public init?(_ rawValue: Swift.String) {
        guard WKWebViewConfiguration._isValidCustomScheme(rawValue) else {
            return nil
        }

        self.rawValue = rawValue
    }

    /// The raw value of the scheme string.
    public let rawValue: Swift.String
}

/// A value used as part of a sequence of results from a ``URLSchemeHandler``, which can either be a `Data` or a `URLResponse`.
@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public enum URLSchemeTaskResult: Sendable {
    /// The response to return to WebKit. The response value must include the MIME type of the request resource.
    ///
    /// This value is used to provide WebKit with the MIME type of the requested resource and its expected
    /// size. This must be added to the task result sequence at least once, but may be added multiple times
    /// if needed. It must be added to the sequence before any data values are.
    case response(URLResponse)

    /// Data for the resource. This value may contain all of the data or only some of it.
    ///
    /// If you load the data incrementally, multiple of these values may be added to the result sequence to deliver
    /// each new portion of data. Each time some new Data is added to the sequence, WebKit appends the data to any
    /// previously received data.
    ///
    /// A ``URLSchemeTaskResult/response(_:)`` must have been added to the sequence prior to any data being aded to it.
    case data(Data)
}

/// A protocol for loading resources with URL schemes that WebKit doesn't handle.
///
/// Adopt the `URLSchemeHandler` protocol in types that handle custom URL schemes for your web content.
/// Custom schemes let you integrate custom resource types into your web content, and you may define
/// custom schemes for resources that your app requires. For example, you might use a custom scheme to
/// integrate content that is available only on the user's device, such as the user's photos. These types
/// can then be registered to a particular WebPage by using the ``WebPage/Configuration-swift.struct/urlSchemeHandlers``
/// property of ``WebPage/Configuration-swift.struct``.
///
/// When a web page encounters a resource that uses a custom scheme, it passes the `URLRequest` to the
/// scheme handler, and expects a stream of responses and data to load the result.
///
/// If WebKit determines that it no longer needs a resource that your handler is loading, it will cancel
/// the Task responsible for the async sequence. Typically, this may happen when the user navigates to another
/// page, but may happen for other reasons.
@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
public protocol URLSchemeHandler {
    /// The type of sequence produced by the handler.
    associatedtype TaskSequence: AsyncSequence<URLSchemeTaskResult, any Error>

    /// Produces a sequence of intermixed responses and data to load a resource for a given request.
    ///
    /// Upon receiving the request, determine the size of the resource and add a ``URLSchemeTaskResult/response(_:)`` value to the async sequence. Providing a response mirrors
    /// the behavior that a web server performs when it receives a request.
    ///
    /// After you load some portion of the resource data, add a ``URLSchemeTaskResult/data(_:)`` value
    /// to the sequence. Multiple of these values may be added to the sequence to delivery data
    /// incrementally, or a single one with all of the data.
    ///
    /// If an error occurs at any point during the load process, a value of type ``Failure`` can be thrown
    /// to report it.
    func reply(for request: URLRequest) -> TaskSequence
}

#endif
