// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_CXX_INTEROP

import Foundation
private import TestWebKitAPILibrary.Helpers.cocoa.HTTPServer
import struct Swift.String

/// A description of an HTTP server route with a path and a response.
///
/// Typically, a simple route can be created using a path and some response data:
///
/// ```swift
/// Route("/hello") {
///     "Hello World!"
/// }
/// ```
///
/// A route can also be used to express a group of routes that share a common path prefix:
///
/// ```swift
/// Route("/parent") {
///     Route("/a") {
///         "My path is /parent/a"
///     }
///
///     Route("/b") {
///         "My path is /parent/b"
///     }
/// }
/// ```
public struct Route: Sendable {
    fileprivate struct Storage: Sendable {
        let pathComponents: [String]
        let response: String
    }

    fileprivate let children: [Storage]

    fileprivate init(children: [Storage]) {
        self.children = children
    }

    fileprivate init(path: String, response: String) {
        self.children = [Storage(pathComponents: [path], response: response)]
    }

    /// Creates a Route from a group of child Routes.
    ///
    /// - Parameters:
    ///   - path: The path of this route. If this value is non-empty, it must start with `/`.
    ///   - route: The children of this route; each child has it's full path prepended by the path of this route.
    public init(_ path: String, @RouteBuilder _ route: () -> Route) {
        self.children = route().children
            .map {
                Storage(pathComponents: [path] + $0.pathComponents, response: $0.response)
            }
    }

    /// Creates a Route whose response data is a String.
    ///
    /// - Parameters:
    ///   - path: The path of this route. If this value is non-empty, it must start with `/`.
    ///   - response: The response to be used.
    public init(_ path: String, _ response: () -> String) {
        self.init(path: path, response: response())
    }
}

/// A result builder used to create a ``Route``.
@resultBuilder
public struct RouteBuilder {
    /// Create a ``Route`` from a group of Routes.
    public static func buildBlock(_ components: Route...) -> Route {
        .init(children: components.flatMap(\.children))
    }
}

/// A type used to simulate an HTTP server with a set of predefined responses for different types of requests.
@MainActor
@safe
public struct HTTPServer: ~Copyable {
    /// A protocol describing how an HTTP connection handles requests.
    public enum `Protocol`: Sendable {
        /// The HTTP protocol.
        case http

        /// The HTTPS protocol.
        case https

        /// The HTTPS protocol, using a legacy version of TLS.
        case httpsWithLegacyTLS

        /// The HTTP2 protocol.
        case http2

        /// The HTTPS proxy protocol.
        case httpsProxy

        /// The HTTPS proxy protocol with authentication.
        case httpsProxyWithAuthentication
    }

    private var storage: TestWebKitAPI.__CxxHTTPServer

    /// Create a server from a group of routes.
    ///
    /// For example, a server which loads `/webkit/success` after loading `/example` can be created using two routes:
    ///
    /// ```swift
    /// func doSomethingCool() async throws {
    ///     let server = HTTPServer(protocol: .httpsProxy) {
    ///         Route("/example") {
    ///             "<script>w = window.open('https://webkit.org/webkit/success')</script>"
    ///         }
    ///
    ///         Route("/webkit/success") {
    ///             "Loaded!"
    ///         }
    ///     }
    ///
    ///     let page = WebPage()
    ///
    ///     let result = try await server.run {
    ///         let destination = address.appendingPathComponent("example")
    ///
    ///         try await page.load(destination).wait()
    ///
    ///         return page.url
    ///     }
    ///
    ///     // `result` is now a `URL` with a value like `http://127.0.0.1:8080/webkit/success`.
    /// }
    /// ```
    ///
    /// - Parameters:
    ///   - protocol: The HTTP protocol to use for this server.
    ///   - route: A group of routes that correspond to a mapping of request paths to responses.
    public init(protocol: `Protocol`, @RouteBuilder _ route: () -> Route) {
        var entries = unsafe TestWebKitAPI.__CxxHTTPServer.ResponseMap()

        let routes = route().children
        for child in routes {
            let path = child.pathComponents.joined()
            let response = unsafe TestWebKitAPI.HTTPResponse(WTF.String(child.response))

            unsafe hashMapSet(&entries, consuming: .init(path), consuming: response)
        }

        unsafe self.storage = .init(consuming: entries, .init(`protocol`), consuming: .init(), nil, .init(), .Yes)
    }

    /// Calls the given closure after starting the server, and then closes the server once finished.
    ///
    /// - Parameter body: A closure that will run while this server is active.
    /// - Returns: The return value, if any, of the `body` closure parameter.
    /// - Throws: Any error thrown by `body`.
    public mutating func run<Result, E>(
        _ body: () async throws(E) -> sending Result
    ) async throws(E) -> sending Result where E: Error, Result: ~Copyable {
        await withCheckedContinuation { continuation in
            unsafe self.storage.startListening(
                consuming: .init(
                    {
                        continuation.resume()
                    },
                    WTF.ThreadLikeAssertion(WTF.CurrentThreadLike())
                )
            )
        }

        let result = try await body()

        await withCheckedContinuation { continuation in
            unsafe self.storage.cancel(
                consuming: .init(
                    {
                        continuation.resume()
                    },
                    WTF.ThreadLikeAssertion(WTF.CurrentThreadLike())
                )
            )
        }

        return result
    }
}

extension TestWebKitAPI.__CxxHTTPServer.`Protocol` {
    fileprivate init(_ protocol: HTTPServer.`Protocol`) {
        self =
            switch `protocol` {
            case .http: .Http
            case .https: .Https
            case .httpsWithLegacyTLS: .HttpsWithLegacyTLS
            case .http2: .Http2
            case .httpsProxy: .HttpsProxy
            case .httpsProxyWithAuthentication: .HttpsProxyWithAuthentication
            }
    }
}

#endif // ENABLE_CXX_INTEROP
